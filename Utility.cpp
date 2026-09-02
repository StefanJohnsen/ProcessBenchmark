#include "Utility.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace benchmark
{
namespace
{
static std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};

    const auto length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        throw std::runtime_error("Configuration text is not valid UTF-8.");

    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                            length) != length)
    {
        throw std::runtime_error("Could not convert configuration text from UTF-8.");
    }

    return result;
}

static std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};

    const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                            nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        throw std::runtime_error("Could not convert a Windows path to UTF-8.");

    std::string result(static_cast<size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                            length, nullptr, nullptr) != length)
    {
        throw std::runtime_error("Could not convert a Windows path to UTF-8.");
    }

    return result;
}

static std::tm localTime()
{
    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::system_clock::to_time_t(now);
    std::tm result{};
    if (localtime_s(&result, &value) != 0)
        throw std::runtime_error("Could not obtain the local time.");
    return result;
}
} // namespace

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char character)
                   {
                       if (character >= 'A' && character <= 'Z')
                           return static_cast<char>(character - 'A' + 'a');
                       return static_cast<char>(character);
                   });
    return value;
}

std::filesystem::path pathFromUtf8(const std::string& value)
{
    return std::filesystem::path(utf8ToWide(value));
}

std::string pathToUtf8(const std::filesystem::path& value)
{
    return wideToUtf8(value.generic_wstring());
}

std::filesystem::path pathWithoutExtension(std::filesystem::path value)
{
    value.replace_extension();
    return value;
}

std::string extensionWithoutDot(std::string value)
{
    if (!value.empty() && value.front() == '.')
        value.erase(value.begin());
    return value;
}

std::filesystem::path absoluteLexicalPath(const std::filesystem::path& value)
{
    std::error_code error;
    auto result = std::filesystem::absolute(value, error);
    if (error)
        throw std::runtime_error("Could not resolve absolute path: " + pathToUtf8(value));

    return result.lexically_normal();
}

std::filesystem::path normalizeAbsolutePath(const std::filesystem::path& value)
{
    std::error_code error;
    auto result = absoluteLexicalPath(value);

    const auto canonical = std::filesystem::weakly_canonical(result, error);
    if (!error)
        result = canonical;

    return result.lexically_normal();
}

std::wstring foldWindowsCase(const std::wstring& value)
{
    if (value.empty())
        return {};
    if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("A Windows path is too long to normalize.");

    const auto sourceLength = static_cast<int>(value.size());
    const auto resultLength = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, value.data(), sourceLength, nullptr,
                                            0, nullptr, nullptr, 0);
    if (resultLength <= 0)
        throw std::runtime_error("Could not normalize Windows path casing: " + windowsErrorMessage(GetLastError()));

    std::wstring result(static_cast<size_t>(resultLength), L'\0');
    if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, value.data(), sourceLength, result.data(), resultLength,
                      nullptr, nullptr, 0) != resultLength)
    {
        throw std::runtime_error("Could not normalize Windows path casing: " + windowsErrorMessage(GetLastError()));
    }
    return result;
}

std::wstring normalizedPathKey(const std::filesystem::path& value)
{
    return foldWindowsCase(normalizeAbsolutePath(value).generic_wstring());
}

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
    return normalizedPathKey(left) == normalizedPathKey(right);
}

bool pathWithin(const std::filesystem::path& value, const std::filesystem::path& directory)
{
    const auto valueKey = normalizedPathKey(value);
    auto directoryKey = normalizedPathKey(directory);

    if (valueKey == directoryKey)
        return true;

    if (!directoryKey.empty() && directoryKey.back() != L'/')
        directoryKey.push_back(L'/');

    return valueKey.rfind(directoryKey, 0) == 0;
}

bool pathsOverlap(const std::filesystem::path& left, const std::filesystem::path& right)
{
    return pathWithin(left, right) || pathWithin(right, left);
}

bool isFilesystemRoot(const std::filesystem::path& value)
{
    const auto normalized = normalizeAbsolutePath(value);
    return samePath(normalized, normalized.root_path());
}

std::wstring quoteWindowsArgument(const std::wstring& value)
{
    if (value.empty())
        return L"\"\"";

    const auto requiresQuotes = value.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!requiresQuotes)
        return value;

    std::wstring result;
    result.push_back(L'\"');
    size_t slashCount = 0;

    for (const auto character : value)
    {
        if (character == L'\\')
        {
            ++slashCount;
            continue;
        }

        if (character == L'\"')
        {
            result.append(slashCount * 2 + 1, L'\\');
            result.push_back(L'\"');
            slashCount = 0;
            continue;
        }

        result.append(slashCount, L'\\');
        slashCount = 0;
        result.push_back(character);
    }

    result.append(slashCount * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

std::string windowsErrorMessage(const uint32_t errorCode)
{
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);

    if (length == 0 || buffer == nullptr)
        return "Windows error " + std::to_string(errorCode);

    std::wstring message(buffer, length);
    LocalFree(buffer);

    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
        message.pop_back();

    return wideToUtf8(message) + " (Windows error " + std::to_string(errorCode) + ")";
}

std::string timestampForFileName()
{
    const auto value = localTime();
    std::ostringstream stream;
    stream << std::put_time(&value, "%Y%m%d-%H%M%S");
    return stream.str();
}

std::string timestampForReport()
{
    const auto value = localTime();
    std::ostringstream stream;
    stream << std::put_time(&value, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string formatDuration(const double milliseconds)
{
    std::ostringstream stream;
    const auto totalMilliseconds = static_cast<uint64_t>(std::llround(std::max(0.0, milliseconds)));
    const auto minutes = totalMilliseconds / 60000;
    const auto seconds = (totalMilliseconds / 1000) % 60;
    const auto remainderMilliseconds = totalMilliseconds % 1000;

    stream << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds << '.' << std::setw(3)
           << remainderMilliseconds;

    return stream.str();
}

std::string formatMiB(const uint64_t bytes)
{
    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    constexpr double bytesPerGiB = bytesPerMiB * 1024.0;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (static_cast<double>(bytes) >= bytesPerGiB)
        stream << static_cast<double>(bytes) / bytesPerGiB << " GiB";
    else
        stream << static_cast<double>(bytes) / bytesPerMiB << " MiB";
    return stream.str();
}

std::string formatBytes(const uint64_t bytes)
{
    constexpr double bytesPerKiB = 1024.0;
    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    constexpr double bytesPerGiB = bytesPerMiB * 1024.0;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2);

    if (static_cast<double>(bytes) >= bytesPerGiB)
        stream << static_cast<double>(bytes) / bytesPerGiB << " GiB";
    else if (static_cast<double>(bytes) >= bytesPerMiB)
        stream << static_cast<double>(bytes) / bytesPerMiB << " MiB";
    else if (static_cast<double>(bytes) >= bytesPerKiB)
        stream << static_cast<double>(bytes) / bytesPerKiB << " KiB";
    else
        stream << bytes << " bytes";

    return stream.str();
}

std::string formatPercent(const double value)
{
    std::ostringstream stream;
    const auto absolute = std::abs(value);
    const auto precision = absolute > 0.0 && absolute < 0.1 ? 3 : (absolute < 1.0 ? 2 : 1);
    stream << std::fixed << std::setprecision(precision) << value << '%';
    return stream.str();
}

double median(std::vector<double> values)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if ((values.size() % 2) != 0)
        return values[middle];
    return (values[middle - 1] + values[middle]) / 2.0;
}

uint64_t median(std::vector<uint64_t> values)
{
    if (values.empty())
        return 0;

    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if ((values.size() % 2) != 0)
        return values[middle];

    const auto lower = values[middle - 1];
    const auto upper = values[middle];
    return lower + (upper - lower) / 2;
}

std::string markdownCode(std::string value)
{
    std::replace(value.begin(), value.end(), '`', '\'');
    return "`" + value + "`";
}
} // namespace benchmark
