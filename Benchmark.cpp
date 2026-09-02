#include "Benchmark.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "Config.h"
#include "ProcessRunner.h"
#include "Report.h"
#include "Utility.h"

namespace benchmark
{
namespace
{
static void createDirectory(const std::filesystem::path& path, const std::string& description)
{
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error)
        throw std::runtime_error("Could not create " + description + ": " + pathToUtf8(path) + ": " + error.message());
    if (!std::filesystem::is_directory(path, error) || error)
        throw std::runtime_error(description + " is not a directory: " + pathToUtf8(path));
}

static std::filesystem::path runLogPath(const std::filesystem::path& logDirectory, const size_t fileIndex,
                                        const size_t groupIndex, const size_t runNumber)
{
    return logDirectory / ("file-" + std::to_string(fileIndex + 1) + "-group-" + std::to_string(groupIndex + 1) +
                           "-run-" + std::to_string(runNumber) + ".log");
}

static RunResult failedRun(const std::filesystem::path& logFile, const size_t runNumber,
                           const size_t executionOrder, const std::string& error)
{
    RunResult result;
    result.logFile = logFile;
    result.runNumber = runNumber;
    result.executionOrder = executionOrder;
    result.error = error;
    return result;
}

struct ConsoleColumnWidths final
{
    size_t progress = 0;
    size_t file = 0;
    size_t extension = 0;
    size_t inputSize = 10;
    size_t group = 0;
    size_t run = 0;
    size_t status = 6;
    size_t runTime = 9;
    size_t peakRam = 10;
};

inline constexpr size_t MaxConsoleFileNameWidth = 40;

static std::string fitConsoleFileName(const std::filesystem::path& path)
{
    auto value = pathToUtf8(path);
    if (value.size() <= MaxConsoleFileNameWidth)
        return value;
    if constexpr (MaxConsoleFileNameWidth <= 3)
        return value.substr(0, MaxConsoleFileNameWidth);
    return value.substr(0, MaxConsoleFileNameWidth - 3) + "...";
}

static ConsoleColumnWidths calculateConsoleColumnWidths(const Config& config, const InputPlan& plan,
                                                        const size_t totalRuns)
{
    ConsoleColumnWidths widths;
    widths.progress = 3 + (std::to_string(totalRuns).size() * 2);
    widths.run = 4 + (std::to_string(config.runsPerFile).size() * 2);

    for (const auto& file : plan.files)
    {
        widths.file = std::max(widths.file, fitConsoleFileName(pathWithoutExtension(file.relativeSource)).size());
        widths.extension = std::max(widths.extension, extensionWithoutDot(file.extension).size());
        widths.inputSize = std::max(widths.inputSize, formatBytes(file.sourceBytes).size());
    }
    for (const auto& group : config.groups)
        widths.group = std::max(widths.group, group.name.size());

    return widths;
}

static std::string progressText(const size_t completedRuns, const size_t totalRuns)
{
    return '[' + std::to_string(completedRuns) + '/' + std::to_string(totalRuns) + ']';
}

static std::string runText(const RunResult& run, const size_t runsPerFile)
{
    return "run " + std::to_string(run.runNumber) + '/' + std::to_string(runsPerFile);
}

static void printRunResult(const Config& config, const FileResult& file, const size_t groupIndex, const RunResult& run,
                           const size_t completedRuns, const size_t totalRuns, const ConsoleColumnWidths& widths,
                           const BenchmarkOptions& options)
{
    const auto status = run.success ? "OK" : "FAILED";
    const auto fileName = fitConsoleFileName(pathWithoutExtension(file.input.relativeSource));
    const auto extension = extensionWithoutDot(file.input.extension);

    std::cout << std::left << std::setw(static_cast<int>(widths.progress)) << progressText(completedRuns, totalRuns)
              << " | " << std::setw(static_cast<int>(widths.file)) << fileName << " | "
              << std::setw(static_cast<int>(widths.extension)) << extension << " | " << std::right
              << std::setw(static_cast<int>(widths.inputSize)) << formatBytes(file.input.sourceBytes) << std::left
              << " | " << std::setw(static_cast<int>(widths.group)) << config.groups[groupIndex].name << " | "
              << std::setw(static_cast<int>(widths.run)) << runText(run, config.runsPerFile) << " | "
              << std::setw(static_cast<int>(widths.status)) << status << " | ";

    if (run.success)
    {
        bool wroteMetric = false;
        if (options.measureTime)
        {
            std::cout << std::setw(static_cast<int>(widths.runTime)) << formatDuration(run.elapsedMilliseconds);
            wroteMetric = true;
        }
        if (options.measureMemory)
        {
            if (wroteMetric)
                std::cout << " | ";
            std::cout << "peak RAM " << std::right << std::setw(static_cast<int>(widths.peakRam))
                      << formatMiB(run.peakWorkingSetBytes) << std::left;
        }
        std::cout << '\n';
    }
    else
    {
        std::cout << run.error << '\n';
    }
}
} // namespace

int runBenchmark(const Config& config, const InputPlan& plan, const std::filesystem::path& reportPath,
                 const std::filesystem::path& logDirectory, const BenchmarkOptions& options)
{
    BenchmarkResults results;
    results.unsupportedFileCount = plan.unsupportedFileCount;
    results.files.reserve(plan.files.size());
    for (const auto& input : plan.files)
        results.files.emplace_back(FileResult{.input = input});

    createDirectory(logDirectory, "process log directory");
    writeMarkdownReport(config, results, reportPath, options);

    const auto totalRuns = plan.files.size() * GroupCount * config.runsPerFile;
    const auto consoleWidths = calculateConsoleColumnWidths(config, plan, totalRuns);
    size_t completedRuns = 0;
    size_t executionOrder = 0;

    for (size_t fileIndex = 0; fileIndex < results.files.size(); ++fileIndex)
    {
        auto& file = results.files[fileIndex];
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            for (size_t runIndex = 0; runIndex < config.runsPerFile; ++runIndex)
            {
                const auto runNumber = runIndex + 1;
                const auto logFile = runLogPath(logDirectory, fileIndex, groupIndex, runNumber);
                ++executionOrder;

                RunResult run;
                try
                {
                    const auto& group = config.groups[groupIndex];
                    const auto& process = group.processes[fileIndex];
                    const auto& executable = config.engines.at(process.engineName);
                    run = runConverter(executable, process.expandedArguments, logFile, runNumber,
                                       executionOrder, options);
                }
                catch (const std::exception& error)
                {
                    run = failedRun(logFile, runNumber, executionOrder, error.what());
                }

                file.groupRuns[groupIndex].emplace_back(std::move(run));
                ++completedRuns;
                printRunResult(config, file, groupIndex, file.groupRuns[groupIndex].back(), completedRuns, totalRuns,
                               consoleWidths, options);
            }
        }
    }

    results.completed = true;
    if (comparablePairCount(config, results) == 0)
        results.fatalError = "No input file produced a valid comparable result for both groups.";

    writeMarkdownReport(config, results, reportPath, options);

    if (!results.fatalError.empty() || hasRunFailures(config, results))
        return 2;
    return 0;
}
} // namespace benchmark
