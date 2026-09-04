#include "ReportPlatform.h"

#include <Windows.h>
#include <shellapi.h>

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "Types.h"
#include "Utility.h"

namespace benchmark::detail
{
std::string localReportTime()
{
    SYSTEMTIME time{};
    GetLocalTime(&time);

    wchar_t date[128]{};
    wchar_t clock[128]{};
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &time, nullptr, date,
                        static_cast<int>(std::size(date)), nullptr) != 0 &&
        GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &time, nullptr, clock,
                        static_cast<int>(std::size(clock))) != 0)
    {
        return pathToUtf8(std::filesystem::path(date)) + ' ' + pathToUtf8(std::filesystem::path(clock));
    }

    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2) << time.wMonth << '-'
           << std::setw(2) << time.wDay << ' ' << std::setw(2) << time.wHour << ':' << std::setw(2) << time.wMinute;
    return stream.str();
}

std::string formatArgumentsForReport(const std::string& arguments)
{
    const auto wide = pathFromUtf8(arguments).wstring();
    int count = 0;
    auto* values = CommandLineToArgvW(wide.c_str(), &count);
    if (values == nullptr)
        return arguments;
    std::wstring result;
    for (int index = 0; index < count; ++index)
    {
        if (index != 0)
            result.push_back(L' ');
        std::filesystem::path value(values[index]);
        const auto visible = value.is_absolute() ? value.filename().wstring() : value.wstring();
        result += quoteWindowsArgument(visible);
    }
    LocalFree(values);
    return pathToUtf8(std::filesystem::path(result));
}

void renameReportFile(const std::filesystem::path& temporary, const std::filesystem::path& reportPath)
{
    if (!MoveFileExW(temporary.c_str(), reportPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto error = GetLastError();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("Could not finalize Markdown report: " + systemErrorMessage(error));
    }
}

std::string hardwareMemorySourceNote()
{
    return "Memory values are collected with Microsoft's Windows APIs: "
           "[GetPhysicallyInstalledSystemMemory](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/"
           "nf-sysinfoapi-getphysicallyinstalledsystemmemory) "
           "and "
           "[GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/"
           "nf-sysinfoapi-globalmemorystatusex).\n\n";
}

std::string memoryMeasurementNote()
{
    std::ostringstream stream;
    stream << "RAM columns in result tables show the highest resident physical memory usage. It is read with "
              "Microsoft's "
              "[GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/"
              "nf-psapi-getprocessmemoryinfo) from the direct process. `PeakWorkingSetSize` is "
              "maintained by Windows as the highest amount of resident physical memory during the run. "
              "The process is polled every "
           << ProcessPollIntervalMilliseconds
           << " ms, but the reported counter is maintained by Windows. Threads are included. Child "
              "process memory and GPU/VRAM are not included. Values use MiB, where 1 MiB is 1,048,576 bytes. "
              "See Microsoft's "
              "[PROCESS_MEMORY_COUNTERS_EX documentation](https://learn.microsoft.com/en-us/windows/win32/api/"
              "psapi/ns-psapi-process_memory_counters_ex) for the counter definitions.\n\n";
    return stream.str();
}
} // namespace benchmark::detail
