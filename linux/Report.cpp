#define _DEFAULT_SOURCE

#include "ReportPlatform.h"

#include <time.h>

#include <cerrno>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "Utility.h"

namespace benchmark::detail
{
namespace
{
std::vector<std::string> splitShellWords(const std::string& text)
{
    std::vector<std::string> result;
    std::string current;
    bool inToken = false;
    size_t index = 0;
    while (index < text.size())
    {
        const char character = text[index];
        if (character == '\'')
        {
            inToken = true;
            ++index;
            const auto end = text.find('\'', index);
            current += text.substr(index, end == std::string::npos ? std::string::npos : end - index);
            index = end == std::string::npos ? text.size() : end + 1;
            continue;
        }
        if (character == '"')
        {
            inToken = true;
            ++index;
            while (index < text.size() && text[index] != '"')
            {
                if (text[index] == '\\' && index + 1 < text.size() &&
                    (text[index + 1] == '"' || text[index + 1] == '\\' || text[index + 1] == '$' ||
                     text[index + 1] == '`'))
                {
                    current.push_back(text[index + 1]);
                    index += 2;
                    continue;
                }
                current.push_back(text[index]);
                ++index;
            }
            if (index < text.size())
                ++index;
            continue;
        }
        if (character == '\\' && index + 1 < text.size())
        {
            inToken = true;
            current.push_back(text[index + 1]);
            index += 2;
            continue;
        }
        if (character == ' ' || character == '\t')
        {
            if (inToken)
            {
                result.push_back(current);
                current.clear();
                inToken = false;
            }
            ++index;
            continue;
        }
        inToken = true;
        current.push_back(character);
        ++index;
    }
    if (inToken)
        result.push_back(current);
    return result;
}

std::string quotePosixArgument(const std::string& value)
{
    const auto needsQuoting =
        value.empty() || value.find_first_of(" \t\n\"'\\$`;&|<>*?[](){}~!#") != std::string::npos;
    if (!needsQuoting)
        return value;

    std::string result = "'";
    for (const char character : value)
    {
        if (character == '\'')
            result += "'\\''";
        else
            result.push_back(character);
    }
    result.push_back('\'');
    return result;
}
} // namespace

std::string localReportTime()
{
    const auto now = time(nullptr);
    struct tm local{};
    if (localtime_r(&now, &local) == nullptr)
        return "N/A";
    char buffer[64];
    if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local) == 0)
        return "N/A";
    return buffer;
}

std::string formatArgumentsForReport(const std::string& arguments)
{
    const auto tokens = splitShellWords(arguments);
    std::string result;
    for (size_t index = 0; index < tokens.size(); ++index)
    {
        if (index != 0)
            result.push_back(' ');
        const std::filesystem::path value(tokens[index]);
        const auto visible = value.is_absolute() ? value.filename().string() : tokens[index];
        result += quotePosixArgument(visible);
    }
    return result;
}

void renameReportFile(const std::filesystem::path& temporary, const std::filesystem::path& reportPath)
{
    if (rename(temporary.c_str(), reportPath.c_str()) != 0)
    {
        const auto error = errno;
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("Could not finalize Markdown report: " +
                                 systemErrorMessage(static_cast<uint32_t>(error)));
    }
}

std::string hardwareMemorySourceNote()
{
    return "Memory values are read from the Linux kernel's `/proc/meminfo` and `/proc/cpuinfo` "
           "pseudo-files. Linux does not expose the SMBIOS-reported installed memory capacity to "
           "unprivileged processes, so installed memory is reported as N/A; usable and available "
           "memory reflect what the kernel itself manages.\n\n";
}

std::string memoryMeasurementNote()
{
    return "RAM columns in result tables show the highest resident set size reached during the run. "
           "It is read from the kernel's own high-water-mark accounting, returned as `ru_maxrss` by "
           "`wait4` once the process has exited, so no polling is needed and no peak can be missed. "
           "Threads are included. Child process memory and GPU/VRAM are not included. Values use MiB, "
           "where 1 MiB is 1,048,576 bytes. See the "
           "[getrusage(2) man page](https://man7.org/linux/man-pages/man2/getrusage.2.html) for the "
           "counter definition.\n\n";
}
} // namespace benchmark::detail
