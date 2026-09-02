#include "Report.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "CmdArgs.h"
#include "Utility.h"

namespace benchmark
{
namespace
{
enum class ResultState
{
    Pending,
    Valid,
    Invalid
};

struct Representative final
{
    ResultState state = ResultState::Pending;
    double elapsedMilliseconds = 0.0;
    uint64_t peakWorkingSetBytes = 0;
};

struct Aggregate final
{
    size_t totalFiles = 0;
    size_t comparableFiles = 0;
    std::array<size_t, GroupCount> invalidFiles{};
    std::array<double, GroupCount> elapsedMilliseconds{};
    std::array<uint64_t, GroupCount> peakWorkingSetBytes{};
};

static std::string markdownText(std::string value)
{
    size_t position = 0;
    while ((position = value.find('|', position)) != std::string::npos)
    {
        value.insert(position, "\\");
        position += 2;
    }
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    return value;
}

static void replaceText(std::string& value, const std::string& source, const std::string& replacement)
{
    if (source.empty()) return;
    size_t position = 0;
    while ((position = value.find(source, position)) != std::string::npos)
    {
        value.replace(position, source.size(), replacement);
        position += replacement.size();
    }
}

static std::string reportSafeText(const Config& config, std::string value)
{
    const auto hidePath = [&](const std::filesystem::path& path)
    {
        const auto replacement = pathToUtf8(path.filename());
        auto slashPath = pathToUtf8(path);
        replaceText(value, slashPath, replacement);
        std::replace(slashPath.begin(), slashPath.end(), '/', '\\');
        replaceText(value, slashPath, replacement);
    };
    hidePath(config.configPath);
    for (const auto& file : config.files) hidePath(file);
    for (const auto& [name, engine] : config.engines) { static_cast<void>(name); hidePath(engine); }
    return markdownText(std::move(value));
}

static std::string exitCodeText(const RunResult& run)
{
    if (!run.exitCode.has_value())
        return "N/A";
    return std::to_string(run.exitCode.value());
}

static std::string registryString(HKEY key, const wchar_t* name)
{
    wchar_t value[512]{};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, &type, value, &bytes) != ERROR_SUCCESS) return "N/A";
    return pathToUtf8(std::filesystem::path(value));
}

static std::string upperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char character)
                   {
                       if (character >= 'a' && character <= 'z')
                           return static_cast<char>(character - 'a' + 'A');
                       return static_cast<char>(character);
                   });
    return value;
}

static std::string formatRatio(const double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << 'x';
    return stream.str();
}

static std::string localReportTime()
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

static uint64_t registryDword(HKEY key, const wchar_t* name)
{
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    return RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS ? value : 0;
}

static size_t physicalCoreCount()
{
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    std::vector<std::byte> buffer(bytes);
    if (bytes == 0 || !GetLogicalProcessorInformationEx(RelationProcessorCore,
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &bytes)) return 0;
    size_t count = 0;
    for (DWORD offset = 0; offset < bytes;)
    {
        const auto* item = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        if (item->Relationship == RelationProcessorCore) ++count;
        offset += item->Size;
    }
    return count;
}

static std::string reportArguments(const std::string& arguments)
{
    const auto wide = pathFromUtf8(arguments).wstring();
    int count = 0;
    auto* values = CommandLineToArgvW(wide.c_str(), &count);
    if (values == nullptr) return arguments;
    std::wstring result;
    for (int index = 0; index < count; ++index)
    {
        if (index != 0) result.push_back(L' ');
        std::filesystem::path value(values[index]);
        const auto visible = value.is_absolute() ? value.filename().wstring() : value.wstring();
        result += quoteWindowsArgument(visible);
    }
    LocalFree(values);
    return pathToUtf8(std::filesystem::path(result));
}

static void writeHardware(std::ostringstream& report)
{
    HKEY cpu = nullptr;
    RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &cpu);
    const auto cpuName = cpu != nullptr ? registryString(cpu, L"ProcessorNameString") : "N/A";
    const auto vendor = cpu != nullptr ? registryString(cpu, L"VendorIdentifier") : "N/A";
    const auto mhz = cpu != nullptr ? registryDword(cpu, L"~MHz") : 0;
    if (cpu != nullptr) RegCloseKey(cpu);

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    const bool memoryOk = GlobalMemoryStatusEx(&memory) != FALSE;
    ULONGLONG installedKiB = 0;
    const bool installedOk = GetPhysicallyInstalledSystemMemory(&installedKiB) != FALSE;
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);

    report << "<br>\n\n## Benchmark Hardware\n\n| Component | Value |\n|---|---|\n";
    report << "| CPU | " << markdownText(cpuName) << " |\n";
    report << "| CPU vendor | " << markdownText(vendor) << " |\n";
    report << "| Physical cores | " << physicalCoreCount() << " |\n";
    report << "| Logical processors | " << GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) << " |\n";
    report << "| Reported CPU clock | " << (mhz == 0 ? "N/A" : std::to_string(mhz) + " MHz") << " |\n";
    report << "| Installed memory | " << (installedOk ? formatBytes(installedKiB * 1024ULL) : "N/A") << " |\n";
    report << "| Usable physical memory | " << (memoryOk ? formatBytes(memory.ullTotalPhys) : "N/A") << " |\n";
    report << "| Available physical memory at report time | " << (memoryOk ? formatBytes(memory.ullAvailPhys) : "N/A") << " |\n";
    report << "| Total page file limit | " << (memoryOk ? formatBytes(memory.ullTotalPageFile) : "N/A") << " |\n";
    report << "| Memory load at report time | " << (memoryOk ? std::to_string(memory.dwMemoryLoad) + "%" : "N/A") << " |\n";
    report << "| Native architecture | " << (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "Other") << " |\n\n";
    report << "Memory values are collected with Microsoft's Windows APIs: "
              "[GetPhysicallyInstalledSystemMemory](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getphysicallyinstalledsystemmemory) "
              "and [GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex).\n\n";
}

static Representative representative(const std::vector<RunResult>& runs, const size_t expectedRuns)
{
    Representative result;
    if (runs.size() < expectedRuns)
        return result;

    if (runs.size() != expectedRuns ||
        std::any_of(runs.begin(), runs.end(), [](const RunResult& run) { return !run.success; }))
    {
        result.state = ResultState::Invalid;
        return result;
    }

    std::vector<double> elapsed;
    std::vector<uint64_t> workingSet;
    elapsed.reserve(runs.size());
    workingSet.reserve(runs.size());

    for (const auto& run : runs)
    {
        elapsed.emplace_back(run.elapsedMilliseconds);
        workingSet.emplace_back(run.peakWorkingSetBytes);
    }

    result.state = ResultState::Valid;
    result.elapsedMilliseconds = median(std::move(elapsed));
    result.peakWorkingSetBytes = median(std::move(workingSet));
    return result;
}

static std::array<Representative, GroupCount> representatives(const Config& config, const FileResult& file)
{
    return {representative(file.groupRuns[0], config.runsPerFile),
            representative(file.groupRuns[1], config.runsPerFile)};
}

static bool comparable(const std::array<Representative, GroupCount>& values)
{
    return values[0].state == ResultState::Valid && values[1].state == ResultState::Valid;
}

static std::string stateText(const std::array<Representative, GroupCount>& values)
{
    if (comparable(values))
        return "Comparable";
    if (values[0].state == ResultState::Pending || values[1].state == ResultState::Pending)
        return "Pending";
    return "Not comparable";
}

static std::string lowerComparison(const std::array<std::string, GroupCount>& groupNames,
                                   const std::array<double, GroupCount>& values, const std::string& metricDescription,
                                   const bool includeSpeedup)
{
    if (values[0] == values[1])
        return "Tie";

    const size_t winner = values[0] < values[1] ? 0 : 1;
    const size_t loser = 1 - winner;
    const auto percentage = (values[loser] - values[winner]) / values[loser] * 100.0;

    auto result = markdownText(groupNames[winner]) + ": " + formatPercent(percentage) + " lower " + metricDescription;
    if (includeSpeedup && values[winner] > 0.0)
    {
        std::ostringstream speedup;
        speedup << std::fixed << std::setprecision(2) << values[loser] / values[winner];
        result += " (" + speedup.str() + "x speedup)";
    }
    return result;
}

static std::string lowerComparison(const std::array<std::string, GroupCount>& groupNames,
                                   const std::array<uint64_t, GroupCount>& values, const std::string& metricDescription)
{
    if (values[0] == values[1])
        return "Tie";

    const size_t winner = values[0] < values[1] ? 0 : 1;
    const size_t loser = 1 - winner;
    const auto percentage =
        static_cast<double>(values[loser] - values[winner]) / static_cast<double>(values[loser]) * 100.0;
    return markdownText(groupNames[winner]) + ": " + formatPercent(percentage) + " less " + metricDescription;
}

static std::string fasterComparison(const std::array<std::string, GroupCount>& groupNames,
                                    const std::array<double, GroupCount>& values)
{
    if (values[0] == values[1])
        return "Tie";

    const size_t winner = values[0] < values[1] ? 0 : 1;
    const size_t loser = 1 - winner;
    if (values[winner] <= 0.0)
        return "N/A";

    std::ostringstream speedup;
    speedup << std::fixed << std::setprecision(2) << values[loser] / values[winner];
    return markdownText(groupNames[winner]) + ": " + speedup.str() + "x faster";
}

static std::string lessComparison(const std::array<std::string, GroupCount>& groupNames,
                                  const std::array<uint64_t, GroupCount>& values)
{
    if (values[0] == values[1])
        return "Tie";

    const size_t winner = values[0] < values[1] ? 0 : 1;
    const size_t loser = 1 - winner;
    if (values[loser] == 0)
        return "N/A";

    const auto percentage = static_cast<double>(values[loser] - values[winner]) /
                            static_cast<double>(values[loser]) * 100.0;
    return markdownText(groupNames[winner]) + ": " + formatPercent(percentage) + " less";
}

static std::string bestValue(const std::string& value)
{
    return "**" + value + "**";
}

static Aggregate aggregate(const Config& config, const std::vector<const FileResult*>& files)
{
    Aggregate result;
    result.totalFiles = files.size();

    for (const auto* file : files)
    {
        const auto values = representatives(config, *file);
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            if (values[groupIndex].state == ResultState::Invalid)
                ++result.invalidFiles[groupIndex];
        }

        if (!comparable(values))
            continue;

        ++result.comparableFiles;
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            result.elapsedMilliseconds[groupIndex] += values[groupIndex].elapsedMilliseconds;
            result.peakWorkingSetBytes[groupIndex] =
                std::max(result.peakWorkingSetBytes[groupIndex], values[groupIndex].peakWorkingSetBytes);
        }
    }

    return result;
}

static void writeAggregate(std::ostringstream& report, const Config& config, const Aggregate& value,
                           const BenchmarkOptions& options, const bool includeBest)
{
    const std::array<std::string, GroupCount> names = {config.groups[0].name, config.groups[1].name};

    report << "Comparable files: **" << value.comparableFiles << '/' << value.totalFiles << "**";
    if (value.invalidFiles[0] != 0 || value.invalidFiles[1] != 0)
    {
        report << "  \nInvalid files: " << markdownText(names[0]) << " **" << value.invalidFiles[0] << "**, "
               << markdownText(names[1]) << " **" << value.invalidFiles[1] << "**";
    }
    report << "\n\n";

    report << "| Metric | " << markdownText(names[0]) << " | " << markdownText(names[1]) << " | Comparison |";
    if (includeBest)
        report << " Best |";
    report << "\n|---|---:|---:|---|";
    if (includeBest)
        report << ":---:|";
    report << '\n';

    if (value.comparableFiles == 0)
    {
        if (options.measureTime)
            report << "| Run time | N/A | N/A | No comparable files |" << (includeBest ? " N/A |" : "") << '\n';
        if (options.measureMemory)
            report << "| RAM | N/A | N/A | No comparable files |" << (includeBest ? " N/A |" : "") << '\n';
        report << '\n';
        return;
    }

    if (options.measureTime)
    {
        report << "| Total time | ";
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            const auto formatted = formatDuration(value.elapsedMilliseconds[groupIndex]);
            report << (includeBest &&
                               value.elapsedMilliseconds[groupIndex] <= value.elapsedMilliseconds[1 - groupIndex]
                           ? bestValue(formatted)
                           : formatted)
                   << " | ";
        }
        report << lowerComparison(names, value.elapsedMilliseconds, "run time", true) << " |";
        if (includeBest)
        {
            report << ' ';
            if (value.elapsedMilliseconds[0] == value.elapsedMilliseconds[1])
                report << "Tie";
            else
                report << bestValue(
                    markdownText(names[value.elapsedMilliseconds[0] < value.elapsedMilliseconds[1] ? 0 : 1]));
            report << " |";
        }
        report << '\n';
    }
    if (options.measureMemory)
    {
        report << "| Highest median RAM | ";
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            const auto formatted = formatMiB(value.peakWorkingSetBytes[groupIndex]);
            report << (includeBest &&
                               value.peakWorkingSetBytes[groupIndex] <= value.peakWorkingSetBytes[1 - groupIndex]
                           ? bestValue(formatted)
                           : formatted)
                   << " | ";
        }
        report << lowerComparison(names, value.peakWorkingSetBytes, "RAM") << " |";
        if (includeBest)
        {
            report << ' ';
            if (value.peakWorkingSetBytes[0] == value.peakWorkingSetBytes[1])
                report << "Tie";
            else
                report << bestValue(
                    markdownText(names[value.peakWorkingSetBytes[0] < value.peakWorkingSetBytes[1] ? 0 : 1]));
            report << " |";
        }
        report << '\n';
    }
    report << '\n';
}

static void writeOverallPunchline(std::ostringstream& report, const Config& config, const Aggregate& value,
                                  const BenchmarkOptions& options)
{
    if (value.comparableFiles == 0)
        return;

    const std::array<std::string, GroupCount> names = {config.groups[0].name, config.groups[1].name};
    if (options.measureTime && value.elapsedMilliseconds[0] > 0.0 && value.elapsedMilliseconds[1] > 0.0)
    {
        if (value.elapsedMilliseconds[0] == value.elapsedMilliseconds[1])
            report << "### **SPEED: TIE**\n\n";
        else
        {
            const size_t winner = value.elapsedMilliseconds[0] < value.elapsedMilliseconds[1] ? 0 : 1;
            const size_t loser = 1 - winner;
            report << "### **" << upperAscii(markdownText(names[winner])) << " IS "
                   << formatRatio(value.elapsedMilliseconds[loser] / value.elapsedMilliseconds[winner])
                   << " FASTER**\n\n";
        }
    }

    if (options.measureMemory && value.peakWorkingSetBytes[0] > 0 && value.peakWorkingSetBytes[1] > 0)
    {
        if (value.peakWorkingSetBytes[0] == value.peakWorkingSetBytes[1])
            report << "### **RAM: TIE**\n\n";
        else
        {
            const size_t winner = value.peakWorkingSetBytes[0] < value.peakWorkingSetBytes[1] ? 0 : 1;
            const size_t loser = 1 - winner;
            const auto percentage =
                static_cast<double>(value.peakWorkingSetBytes[loser] - value.peakWorkingSetBytes[winner]) /
                static_cast<double>(value.peakWorkingSetBytes[loser]) * 100.0;
            report << "### **" << upperAscii(markdownText(names[winner])) << " USES " << formatPercent(percentage)
                   << " LESS RAM**\n\n";
        }
    }
}

static std::string comparisonBar(const double value, const double maximum)
{
    constexpr size_t width = 24;
    if (maximum <= 0.0 || value <= 0.0)
        return "N/A";

    const auto filled = std::max<size_t>(1, static_cast<size_t>(std::llround(value / maximum * width)));
    std::string result;
    for (size_t index = 0; index < width; ++index)
        result += index < filled ? "&#9608;" : "&#9617;";
    return result;
}

static std::string comparisonFactor(const double best, const double worst)
{
    if (best <= 0.0 || worst <= 0.0)
        return "N/A";

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << worst / best << 'x';
    return stream.str();
}

static void writeVisualComparison(std::ostringstream& report, const Config& config, const Aggregate& value,
                                  const BenchmarkOptions& options)
{
    if (value.comparableFiles == 0)
        return;

    report << "Lower is better. Bars are normalized independently for each metric.\n\n";
    report << "| Metric | Group | Usage | Value | Comp. | BEST |\n|---|---|---|---:|---:|:---:|\n";
    if (options.measureTime)
    {
        const auto maximum = std::max(value.elapsedMilliseconds[0], value.elapsedMilliseconds[1]);
        const auto winner = value.elapsedMilliseconds[0] < value.elapsedMilliseconds[1] ? 0U : 1U;
        const auto tie = value.elapsedMilliseconds[0] == value.elapsedMilliseconds[1];
        const auto factor = comparisonFactor(value.elapsedMilliseconds[winner], maximum);
        for (size_t index = 0; index < GroupCount; ++index)
        {
            report << "| Total time | " << markdownText(config.groups[index].name) << " | "
                   << comparisonBar(value.elapsedMilliseconds[index], maximum) << " | "
                   << formatDuration(value.elapsedMilliseconds[index]) << " | "
                   << (tie ? "1.00x" : (index == winner ? factor : "-")) << " | "
                   << (tie || index == winner ? "&#128994;" : "") << " |\n";
        }
    }
    if (options.measureMemory)
    {
        const auto maximum = static_cast<double>(std::max(value.peakWorkingSetBytes[0], value.peakWorkingSetBytes[1]));
        const auto winner = value.peakWorkingSetBytes[0] < value.peakWorkingSetBytes[1] ? 0U : 1U;
        const auto tie = value.peakWorkingSetBytes[0] == value.peakWorkingSetBytes[1];
        const auto improvement = tie || maximum <= 0.0
                                     ? 0.0
                                     : (maximum - static_cast<double>(value.peakWorkingSetBytes[winner])) / maximum *
                                           100.0;
        for (size_t index = 0; index < GroupCount; ++index)
        {
            report << "| Highest median RAM | " << markdownText(config.groups[index].name) << " | "
                   << comparisonBar(static_cast<double>(value.peakWorkingSetBytes[index]), maximum) << " | "
                   << formatMiB(value.peakWorkingSetBytes[index]) << " | "
                   << (tie ? "0.0%" : (index == winner ? formatPercent(improvement) + " less" : "-")) << " | "
                   << (tie || index == winner ? "&#128994;" : "") << " |\n";
        }
    }
    report << '\n';
}

static void writeIntroduction(std::ostringstream& report, const Config& config, const BenchmarkResults& results,
                              const BenchmarkOptions& options)
{
    report << "# " << markdownText(config.title) << "\n\n";
    report << "*Automatically generated by [ProcessBenchmark]"
              "(https://github.com/StefanJohnsen/ProcessBenchmark), version "
           << cmd::VersionNumber << " on " << localReportTime() << " local time.*\n\n<br>\n\n";
    if (results.completed)
    {
        std::vector<const FileResult*> files;
        files.reserve(results.files.size());
        for (const auto& file : results.files)
            files.emplace_back(&file);
        writeOverallPunchline(report, config, aggregate(config, files), options);
    }
    writeHardware(report);
    const auto totalPlannedRuns = config.files.size() * config.groups.size() * config.runsPerFile;
    report << "## Benchmark Overview\n\n";
    report << "This benchmark compares **" << config.groups.size() << " process groups** across **"
           << config.files.size() << " input files**. Each configured process is run **" << config.runsPerFile << ' '
           << (config.runsPerFile == 1 ? "time" : "times") << "** for each process group, for a total of **"
           << totalPlannedRuns << " planned process runs**. Processes execute sequentially, one at a time, so they "
              "do not compete with another benchmarked process for CPU or memory during measurement.\n\n";
    if (!results.completed)
        report << "> **In progress - results are incomplete.**\n\n";

    report << "## Process Engines\n\n| Name | Executable |\n|---|---|\n";
    for (const auto& [name, executable] : config.engines)
        report << "| " << markdownText(name) << " | " << markdownCode(pathToUtf8(executable.filename())) << " |\n";
    report << '\n';

    report << "## Test Files\n\n| Index | File | Size |\n|---:|---|---:|\n";
    for (size_t index = 0; index < config.files.size(); ++index)
    {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(config.files[index], error);
        report << "| " << index << " | " << markdownCode(pathToUtf8(config.files[index].filename())) << " | "
               << (error ? "N/A" : formatBytes(bytes)) << " |\n";
    }
    report << '\n';

    for (const auto& group : config.groups)
    {
        report << "## Processes - " << markdownText(group.name) << "\n\n";
        report << "| Index | Engine | Command Arguments |\n|---:|---|---|\n";
        for (size_t index = 0; index < group.processes.size(); ++index)
            report << "| " << index << " | " << markdownText(group.processes[index].engineName) << " | "
                   << markdownCode(reportSafeText(config, reportArguments(group.processes[index].expandedArguments)))
                   << " |\n";
        report << '\n';
    }

    report << "## Test Method\n\n";
    report << "- Conversions run sequentially, one process at a time. For each file, the first listed group completes "
              "all repetitions before the second group.\n";
    report << "- Per-file results are medians of the " << config.runsPerFile
           << " runs. A file is compared only when all runs succeed for both groups.\n";
    if (options.measureTime)
        report << "- Type and overall run times are sums of comparable per-file medians.\n";
    if (options.measureMemory)
        report << "- Type and overall memory values are the highest comparable per-file medians; memory is not "
                  "summed.\n";
    report << '\n';

    if (options.measureTime)
    {
        report << "<br>\n\n### Run Time Measurement\n\n";
        report << "Run time uses a monotonic clock from immediately before the suspended process is "
                  "resumed until the main process terminates. Executable initialization and all work performed by "
                  "the process are "
                  "included; benchmark preflight and cleanup are excluded.\n\n";
    }

    if (options.measureMemory)
    {
        report << "<br>\n\n### Memory Measurement\n\n";
        report << "RAM columns in result tables show the highest resident physical memory usage. It is read with Microsoft's "
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
    }
}

static void writeFileType(std::ostringstream& report, const Config& config, const std::string& extension,
                          const std::vector<const FileResult*>& files, const BenchmarkOptions& options)
{
    const std::array<std::string, GroupCount> names = {config.groups[0].name, config.groups[1].name};
    report << "<br>\n\n## File type " << markdownCode(extension) << "\n\n";
    report << "<br>\n\n### Per-file results\n\n";
    report << "| File name | Ext | ";
    if (options.measureTime)
    {
        report << markdownText(names[0]) << " time | " << markdownText(names[1])
               << " time | Time comp. | ";
    }
    if (options.measureMemory)
    {
        report << markdownText(names[0]) << " RAM | " << markdownText(names[1])
               << " RAM | RAM comp. | ";
    }
    report << "Status |\n|---|:---:|";
    if (options.measureTime)
        report << ":---:|:---:|---|";
    if (options.measureMemory)
        report << "---:|---:|---|";
    report << ":---:|\n";

    for (const auto* file : files)
    {
        const auto values = representatives(config, *file);
        report << "| " << markdownCode(pathToUtf8(pathWithoutExtension(file->input.relativeSource.filename())))
               << " | " << markdownCode(extensionWithoutDot(file->input.extension)) << " | ";
        if (!comparable(values))
        {
            if (options.measureTime)
            {
                for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
                {
                    if (values[groupIndex].state == ResultState::Valid)
                        report << formatDuration(values[groupIndex].elapsedMilliseconds);
                    else
                        report << "N/A";
                    report << " | ";
                }
                report << "N/A | ";
            }
            if (options.measureMemory)
            {
                for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
                {
                    if (values[groupIndex].state == ResultState::Valid)
                        report << formatMiB(values[groupIndex].peakWorkingSetBytes);
                    else
                        report << "N/A";
                    report << " | ";
                }
                report << "N/A | ";
            }
            report << stateText(values) << " |\n";
            continue;
        }

        const std::array<double, GroupCount> runTimes = {values[0].elapsedMilliseconds, values[1].elapsedMilliseconds};
        const std::array<uint64_t, GroupCount> memory = {values[0].peakWorkingSetBytes, values[1].peakWorkingSetBytes};
        if (options.measureTime)
        {
            for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
            {
                const auto formatted = formatDuration(runTimes[groupIndex]);
                report << (runTimes[groupIndex] < runTimes[1 - groupIndex] ? bestValue(formatted) : formatted) << " | ";
            }
            report << fasterComparison(names, runTimes) << " | ";
        }
        if (options.measureMemory)
        {
            for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
            {
                const auto formatted = formatMiB(memory[groupIndex]);
                report << (memory[groupIndex] < memory[1 - groupIndex] ? bestValue(formatted) : formatted) << " | ";
            }
            report << lessComparison(names, memory) << " | ";
        }
        report << "OK |\n";
    }
    report << '\n';

    report << "<br>\n\n### Individual runs\n\n";
    report << "| File name | Ext | Group | Run | ";
    if (options.measureTime)
        report << "Time | ";
    if (options.measureMemory)
        report << "RAM | ";
    report << "Exit | Status | Best |\n|---|:---:|:---:|:---:|";
    if (options.measureTime)
        report << ":---:|";
    if (options.measureMemory)
        report << "---:|";
    report << ":---:|:---:|:---:|\n";

    const RunResult* fastestRun = nullptr;
    const RunResult* lowestMemoryRun = nullptr;
    for (const auto* file : files)
    {
        for (const auto& groupRuns : file->groupRuns)
        {
            for (const auto& run : groupRuns)
            {
                if (!run.success)
                    continue;
                if (options.measureTime &&
                    (fastestRun == nullptr || run.elapsedMilliseconds < fastestRun->elapsedMilliseconds))
                {
                    fastestRun = &run;
                }
                if (options.measureMemory &&
                    (lowestMemoryRun == nullptr || run.peakWorkingSetBytes < lowestMemoryRun->peakWorkingSetBytes))
                {
                    lowestMemoryRun = &run;
                }
            }
        }
    }
    const auto fastestDisplayed = fastestRun == nullptr ? std::string{} : formatDuration(fastestRun->elapsedMilliseconds);
    const auto lowestMemoryDisplayed =
        lowestMemoryRun == nullptr ? std::string{} : formatMiB(lowestMemoryRun->peakWorkingSetBytes);

    for (const auto* file : files)
    {
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            for (const auto& run : file->groupRuns[groupIndex])
            {
                const auto displayedTime = formatDuration(run.elapsedMilliseconds);
                const auto displayedMemory = formatMiB(run.peakWorkingSetBytes);
                const bool hasBestTime = run.success && options.measureTime && displayedTime == fastestDisplayed;
                const bool hasBestMemory =
                    run.success && options.measureMemory && displayedMemory == lowestMemoryDisplayed;
                const bool isBestRow = hasBestTime || hasBestMemory;
                const auto fileName =
                    markdownCode(pathToUtf8(pathWithoutExtension(file->input.relativeSource.filename())));
                const auto extensionText = markdownCode(extensionWithoutDot(file->input.extension));
                const auto groupName = markdownText(names[groupIndex]);
                const auto runNumber = std::to_string(run.runNumber);

                report << "| " << (isBestRow ? bestValue(fileName) : fileName) << " | "
                       << (isBestRow ? bestValue(extensionText) : extensionText) << " | "
                       << (isBestRow ? bestValue(groupName) : groupName) << " | "
                       << (isBestRow ? bestValue(runNumber) : runNumber) << " | ";
                if (options.measureTime)
                {
                    report << (hasBestTime ? bestValue(displayedTime) : displayedTime) << " | ";
                }
                if (options.measureMemory)
                {
                    report << (hasBestMemory ? bestValue(displayedMemory) : displayedMemory) << " | ";
                }
                report << exitCodeText(run) << " | " << (run.success ? "OK" : "Failed") << " | "
                       << (isBestRow ? "&#128994;" : "") << " |\n";
            }
        }
    }
    report << '\n';

    report << "<br>\n\n### File type summary\n\n";
    writeAggregate(report, config, aggregate(config, files), options, false);
}

static void writeFailures(std::ostringstream& report, const Config& config, const BenchmarkResults& results)
{
    bool wroteHeading = false;
    for (const auto& file : results.files)
    {
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            for (const auto& run : file.groupRuns[groupIndex])
            {
                if (run.success)
                    continue;
                if (!wroteHeading)
                {
                    report << "## Failures\n\n";
                    wroteHeading = true;
                }

                report << "- " << markdownCode(pathToUtf8(file.input.relativeSource)) << ", group **"
                       << markdownText(config.groups[groupIndex].name) << "**, run " << run.runNumber << ": "
                       << reportSafeText(config, run.error.empty() ? "Unknown error." : run.error);
                report << '\n';
            }
        }
    }

    if (!results.fatalError.empty())
    {
        if (!wroteHeading)
            report << "## Failures\n\n";
        report << "- Fatal error: " << reportSafeText(config, results.fatalError) << '\n';
        wroteHeading = true;
    }

    if (wroteHeading)
        report << '\n';
}

static void writeConfigurationUsed(std::ostringstream& report, const Config& config)
{
    std::ifstream stream(config.configPath, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Could not reopen configuration for the Markdown report.");
    std::ostringstream source;
    source << stream.rdbuf();
    if (!stream.good() && !stream.eof())
        throw std::runtime_error("Could not read configuration for the Markdown report.");

    const auto text = source.str();
    if (std::any_of(text.begin(), text.end(), [](const unsigned char character) { return character > 0x7f; }))
        throw std::runtime_error("Configuration appendix must contain only ASCII characters.");

    std::string fence = "```";
    while (text.find(fence) != std::string::npos) fence.push_back('`');
    report << "<br>\n<br>\n\n# Configuration Used\n\n" << fence << "text\n" << text;
    if (text.empty() || text.back() != '\n') report << '\n';
    report << fence << '\n';
}

static void writeAtomic(const std::filesystem::path& reportPath, const std::string& text)
{
    auto temporary = reportPath;
    temporary += L".tmp";

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("Could not create Markdown report: " + pathToUtf8(temporary));
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream)
            throw std::runtime_error("Could not write Markdown report: " + pathToUtf8(temporary));
    }

    if (!MoveFileExW(temporary.c_str(), reportPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto error = GetLastError();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("Could not finalize Markdown report: " + windowsErrorMessage(error));
    }
}
} // namespace

void writeMarkdownReport(const Config& config, const BenchmarkResults& results, const std::filesystem::path& reportPath,
                         const BenchmarkOptions& options)
{
    std::ostringstream report;
    writeIntroduction(report, config, results, options);

    std::map<std::string, std::vector<const FileResult*>> byExtension;
    std::vector<const FileResult*> allFiles;
    allFiles.reserve(results.files.size());
    for (const auto& file : results.files)
    {
        byExtension[file.input.extension].emplace_back(&file);
        allFiles.emplace_back(&file);
    }

    for (const auto& [extension, files] : byExtension)
        writeFileType(report, config, extension, files, options);

    report << "<br>\n<br>\n\n# Overall Performance\n\n";
    const auto overall = aggregate(config, allFiles);
    report << "Comparable files: **" << overall.comparableFiles << '/' << overall.totalFiles << "**\n\n";
    if (overall.invalidFiles[0] != 0 || overall.invalidFiles[1] != 0)
    {
        report << "Invalid files: " << markdownText(config.groups[0].name) << " **" << overall.invalidFiles[0]
               << "**, " << markdownText(config.groups[1].name) << " **" << overall.invalidFiles[1] << "**\n\n";
    }
    writeVisualComparison(report, config, overall, options);
    writeOverallPunchline(report, config, overall, options);
    writeFailures(report, config, results);
    writeConfigurationUsed(report, config);
    writeAtomic(reportPath, report.str());
}

size_t comparablePairCount(const Config& config, const BenchmarkResults& results)
{
    return static_cast<size_t>(std::count_if(results.files.begin(), results.files.end(), [&](const FileResult& file)
                                             { return comparable(representatives(config, file)); }));
}

bool hasRunFailures(const Config& config, const BenchmarkResults& results)
{
    if (!results.fatalError.empty())
        return true;

    return std::any_of(results.files.begin(), results.files.end(),
                       [&](const FileResult& file)
                       {
                           const auto values = representatives(config, file);
                           return values[0].state == ResultState::Invalid || values[1].state == ResultState::Invalid;
                       });
}
} // namespace benchmark
