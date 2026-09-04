#include "Utility.h"

#include <Windows.h>

#include <stdexcept>

namespace benchmark
{
namespace
{
std::wstring utf8ToWide(const std::string& value)
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

std::string wideToUtf8(const std::wstring& value)
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
} // namespace

std::filesystem::path pathFromUtf8(const std::string& value)
{
    return std::filesystem::path(utf8ToWide(value));
}

std::string pathToUtf8(const std::filesystem::path& value)
{
    return wideToUtf8(value.generic_wstring());
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

std::string systemErrorMessage(const uint32_t errorCode)
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
} // namespace benchmark
