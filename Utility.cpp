#include "Utility.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace benchmark
{
namespace
{
static std::filesystem::path absoluteLexicalPath(const std::filesystem::path& value)
{
    std::error_code error;
    auto result = std::filesystem::absolute(value, error);
    if (error)
        throw std::runtime_error("Could not resolve absolute path: " + pathToUtf8(value));

    return result.lexically_normal();
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

std::filesystem::path normalizeAbsolutePath(const std::filesystem::path& value)
{
    std::error_code error;
    auto result = absoluteLexicalPath(value);

    const auto canonical = std::filesystem::weakly_canonical(result, error);
    if (!error)
        result = canonical;

    return result.lexically_normal();
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
    stream << std::fixed << std::setprecision(0);
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
    stream << std::fixed << std::setprecision(1);

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

std::string formatRatio(const double value)
{
    if (value <= 0.0)
        return "N/A";
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << 'x';
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
