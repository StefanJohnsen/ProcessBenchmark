#include "Benchmark.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
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
static RunResult failedRun(const size_t runNumber, const std::string& error)
{
    RunResult result;
    result.runNumber = runNumber;
    result.error = error;
    return result;
}

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

static std::string exitCodeText(const RunResult& run)
{
    return run.exitCode.has_value() ? std::to_string(run.exitCode.value()) : "N/A";
}

enum class ConsoleAlignment
{
    Left,
    Center,
    Right
};

static size_t displayedWidth(const std::string& value)
{
    return static_cast<size_t>(std::count_if(value.begin(), value.end(),
                                             [](const unsigned char character) { return (character & 0xc0) != 0x80; }));
}

static std::string padded(const std::string& value, const size_t width, const ConsoleAlignment alignment)
{
    const auto visibleWidth = displayedWidth(value);
    if (visibleWidth >= width)
        return value;
    const auto remaining = width - visibleWidth;
    if (alignment == ConsoleAlignment::Left)
        return value + std::string(remaining, ' ');
    if (alignment == ConsoleAlignment::Right)
        return std::string(remaining, ' ') + value;
    const auto left = remaining / 2;
    return std::string(left, ' ') + value + std::string(remaining - left, ' ');
}

static void printConsoleTable(const std::vector<std::string>& headers, const std::vector<ConsoleAlignment>& alignments,
                              const std::vector<std::vector<std::string>>& rows)
{
    std::vector<size_t> widths;
    widths.reserve(headers.size());
    for (const auto& header : headers)
        widths.emplace_back(displayedWidth(header));
    for (const auto& row : rows)
        for (size_t index = 0; index < row.size(); ++index)
            widths[index] = std::max(widths[index], displayedWidth(row[index]));

    const auto printRow = [&](const std::vector<std::string>& row)
    {
        for (size_t index = 0; index < row.size(); ++index)
        {
            if (index != 0)
                std::cout << " | ";
            std::cout << padded(row[index], widths[index], alignments[index]);
        }
        std::cout << '\n';
    };

    printRow(headers);
    for (size_t index = 0; index < widths.size(); ++index)
    {
        if (index != 0)
            std::cout << "-+-";
        std::cout << std::string(widths[index], '-');
    }
    std::cout << '\n';
    for (const auto& row : rows)
        printRow(row);
}

struct ConsoleAggregate final
{
    size_t totalFiles = 0;
    size_t comparableFiles = 0;
    std::array<double, GroupCount> elapsedMilliseconds{};
    std::array<uint64_t, GroupCount> peakWorkingSetBytes{};
};

static ConsoleAggregate consoleAggregate(const Config& config, const BenchmarkResults& results)
{
    ConsoleAggregate aggregate;
    aggregate.totalFiles = results.files.size();
    for (const auto& file : results.files)
    {
        std::array<double, GroupCount> elapsed{};
        std::array<uint64_t, GroupCount> memory{};
        bool comparable = true;
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            const auto& runs = file.groupRuns[groupIndex];
            if (runs.size() != config.runsPerFile ||
                std::any_of(runs.begin(), runs.end(), [](const RunResult& run) { return !run.success; }))
            {
                comparable = false;
                break;
            }

            std::vector<double> times;
            std::vector<uint64_t> workingSets;
            times.reserve(runs.size());
            workingSets.reserve(runs.size());
            for (const auto& run : runs)
            {
                times.emplace_back(run.elapsedMilliseconds);
                workingSets.emplace_back(run.peakWorkingSetBytes);
            }
            elapsed[groupIndex] = median(std::move(times));
            memory[groupIndex] = median(std::move(workingSets));
        }

        if (!comparable)
            continue;
        ++aggregate.comparableFiles;
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            aggregate.elapsedMilliseconds[groupIndex] += elapsed[groupIndex];
            aggregate.peakWorkingSetBytes[groupIndex] =
                std::max(aggregate.peakWorkingSetBytes[groupIndex], memory[groupIndex]);
        }
    }
    return aggregate;
}

static std::string consoleBar(const double value, const double maximum)
{
    constexpr size_t width = 24;
    if (value <= 0.0 || maximum <= 0.0)
        return "N/A";
    const auto filled = std::clamp<size_t>(static_cast<size_t>(std::llround(value / maximum * width)), 1, width);
    std::string result;
    for (size_t index = 0; index < width; ++index)
        result += index < filled ? "\xe2\x96\x88" : "\xe2\x96\x91";
    return result;
}

static std::string ratio(const double worst, const double best)
{
    if (best <= 0.0 || worst <= 0.0)
        return "N/A";
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << worst / best << 'x';
    return stream.str();
}

static std::string upperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char character)
                   {
                       return character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A')
                                                                   : static_cast<char>(character);
                   });
    return value;
}

static void printOverallPerformance(const Config& config, const BenchmarkResults& results,
                                    const BenchmarkOptions& options)
{
    const auto aggregate = consoleAggregate(config, results);
    std::cout << "\nOverall Performance\n\nComparable files: " << aggregate.comparableFiles << '/'
              << aggregate.totalFiles << "\n\n";
    if (aggregate.comparableFiles == 0)
    {
        std::cout << "No comparable files.\n";
        return;
    }

    std::cout << "Lower is better. Bars are normalized independently for each metric.\n\n";
    const std::vector<std::string> headers = {"Metric", "Group", "Usage", "Value", "Comp.", "BEST"};
    const std::vector<ConsoleAlignment> alignments = {ConsoleAlignment::Left,  ConsoleAlignment::Left,
                                                      ConsoleAlignment::Left,  ConsoleAlignment::Right,
                                                      ConsoleAlignment::Right, ConsoleAlignment::Center};
    std::vector<std::vector<std::string>> rows;

    size_t timeWinner = 0;
    bool timeTie = false;
    std::string timeFactor;
    if (options.measureTime)
    {
        const auto maximum = std::max(aggregate.elapsedMilliseconds[0], aggregate.elapsedMilliseconds[1]);
        timeTie = aggregate.elapsedMilliseconds[0] == aggregate.elapsedMilliseconds[1];
        timeWinner = aggregate.elapsedMilliseconds[0] < aggregate.elapsedMilliseconds[1] ? 0 : 1;
        timeFactor = ratio(maximum, aggregate.elapsedMilliseconds[timeWinner]);
        for (size_t index = 0; index < GroupCount; ++index)
            rows.push_back({"Total time", config.groups[index].name,
                            consoleBar(aggregate.elapsedMilliseconds[index], maximum),
                            formatDuration(aggregate.elapsedMilliseconds[index]),
                            timeTie ? "1.00x" : (index == timeWinner ? timeFactor : "-"),
                            timeTie || index == timeWinner ? "\xf0\x9f\x9f\xa2" : ""});
    }

    size_t memoryWinner = 0;
    bool memoryTie = false;
    std::string memoryImprovement;
    if (options.measureMemory)
    {
        const auto maximum = std::max(aggregate.peakWorkingSetBytes[0], aggregate.peakWorkingSetBytes[1]);
        memoryTie = aggregate.peakWorkingSetBytes[0] == aggregate.peakWorkingSetBytes[1];
        memoryWinner = aggregate.peakWorkingSetBytes[0] < aggregate.peakWorkingSetBytes[1] ? 0 : 1;
        const auto improvement = memoryTie || maximum == 0
                                     ? 0.0
                                     : static_cast<double>(maximum - aggregate.peakWorkingSetBytes[memoryWinner]) /
                                           static_cast<double>(maximum) * 100.0;
        memoryImprovement = formatPercent(improvement) + " less";
        for (size_t index = 0; index < GroupCount; ++index)
            rows.push_back(
                {"Highest median RAM", config.groups[index].name,
                 consoleBar(static_cast<double>(aggregate.peakWorkingSetBytes[index]), static_cast<double>(maximum)),
                 formatMiB(aggregate.peakWorkingSetBytes[index]),
                 memoryTie ? "0.0%" : (index == memoryWinner ? memoryImprovement : "-"),
                 memoryTie || index == memoryWinner ? "\xf0\x9f\x9f\xa2" : ""});
    }

    printConsoleTable(headers, alignments, rows);
    if (options.measureTime)
        std::cout << '\n'
                  << (timeTie ? "SPEED: TIE"
                              : upperAscii(config.groups[timeWinner].name) + " IS " + timeFactor + " FASTER")
                  << '\n';
    if (options.measureMemory)
        std::cout << (memoryTie ? "RAM: TIE"
                                : upperAscii(config.groups[memoryWinner].name) + " USES " +
                                      upperAscii(memoryImprovement) + " RAM")
                  << '\n';
}

struct ConsoleRunTable final
{
    std::vector<std::string> headers;
    std::vector<ConsoleAlignment> alignments;
    std::vector<size_t> widths;
};

static ConsoleRunTable createConsoleRunTable(const Config& config, const BenchmarkResults& results,
                                             const BenchmarkOptions& options)
{
    ConsoleRunTable table;
    table.headers = {"File name", "Ext", "Size", "Group", "Run"};
    table.alignments = {ConsoleAlignment::Left, ConsoleAlignment::Center, ConsoleAlignment::Right,
                        ConsoleAlignment::Center, ConsoleAlignment::Center};
    if (options.measureTime)
    {
        table.headers.emplace_back("Time");
        table.alignments.emplace_back(ConsoleAlignment::Center);
    }
    if (options.measureMemory)
    {
        table.headers.emplace_back("RAM");
        table.alignments.emplace_back(ConsoleAlignment::Right);
    }
    table.headers.emplace_back("Exit");
    table.headers.emplace_back("Status");
    table.alignments.emplace_back(ConsoleAlignment::Center);
    table.alignments.emplace_back(ConsoleAlignment::Center);

    for (const auto& header : table.headers)
        table.widths.emplace_back(displayedWidth(header));
    for (const auto& file : results.files)
    {
        table.widths[0] = std::max(
            table.widths[0], displayedWidth(fitConsoleFileName(pathWithoutExtension(file.input.source.filename()))));
        table.widths[1] = std::max(table.widths[1], displayedWidth(extensionWithoutDot(file.input.extension)));
        table.widths[2] = std::max(table.widths[2], displayedWidth(formatBytes(file.input.sourceBytes)));
    }
    for (const auto& group : config.groups)
        table.widths[3] = std::max(table.widths[3], displayedWidth(group.name));
    table.widths[4] = std::max(table.widths[4], std::to_string(config.runsPerFile).size());

    size_t metricIndex = 5;
    if (options.measureTime)
    {
        table.widths[metricIndex] = std::max<size_t>(table.widths[metricIndex], 9);
        ++metricIndex;
    }
    if (options.measureMemory)
    {
        table.widths[metricIndex] = std::max<size_t>(table.widths[metricIndex], 10);
        ++metricIndex;
    }
    return table;
}

static void printConsoleRunTableHeader(const ConsoleRunTable& table)
{
    std::cout << "\nIndividual runs\n\n";
    for (size_t index = 0; index < table.headers.size(); ++index)
    {
        if (index != 0)
            std::cout << " | ";
        std::cout << padded(table.headers[index], table.widths[index], table.alignments[index]);
    }
    std::cout << '\n';
    for (size_t index = 0; index < table.widths.size(); ++index)
    {
        if (index != 0)
            std::cout << "-+-";
        std::cout << std::string(table.widths[index], '-');
    }
    std::cout << '\n' << std::flush;
}

static void printConsoleRun(const ConsoleRunTable& table, const Config& config, const FileResult& file,
                            const size_t groupIndex, const RunResult& run, const BenchmarkOptions& options)
{
    std::vector<std::string> row = {fitConsoleFileName(pathWithoutExtension(file.input.source.filename())),
                                    extensionWithoutDot(file.input.extension), formatBytes(file.input.sourceBytes),
                                    config.groups[groupIndex].name, std::to_string(run.runNumber)};
    if (options.measureTime)
        row.emplace_back(formatDuration(run.elapsedMilliseconds));
    if (options.measureMemory)
        row.emplace_back(formatMiB(run.peakWorkingSetBytes));
    row.emplace_back(exitCodeText(run));
    row.emplace_back(run.success ? "OK" : "Failed");

    for (size_t index = 0; index < row.size(); ++index)
    {
        if (index != 0)
            std::cout << " | ";
        std::cout << padded(row[index], table.widths[index], table.alignments[index]);
    }
    std::cout << '\n' << std::flush;
}
} // namespace

int runBenchmark(const Config& config, const InputPlan& plan, const std::filesystem::path& reportPath,
                 const BenchmarkOptions& options)
{
    BenchmarkResults results;
    results.files.reserve(plan.files.size());
    for (const auto& input : plan.files)
        results.files.emplace_back(FileResult{.input = input});

    if (options.createReport)
        writeMarkdownReport(config, results, reportPath, options);

    const auto consoleTable = createConsoleRunTable(config, results, options);
    printConsoleRunTableHeader(consoleTable);
    for (size_t fileIndex = 0; fileIndex < results.files.size(); ++fileIndex)
    {
        auto& file = results.files[fileIndex];
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            for (size_t runIndex = 0; runIndex < config.runsPerFile; ++runIndex)
            {
                const auto runNumber = runIndex + 1;
                RunResult run;
                try
                {
                    const auto& group = config.groups[groupIndex];
                    const auto& process = group.processes[fileIndex];
                    const auto& executable = config.engines.at(process.engineName);
                    run = runConverter(executable, process.commandArguments, runNumber, options);
                }
                catch (const std::exception& error)
                {
                    run = failedRun(runNumber, error.what());
                }

                file.groupRuns[groupIndex].emplace_back(std::move(run));
                printConsoleRun(consoleTable, config, file, groupIndex, file.groupRuns[groupIndex].back(), options);
            }
        }
    }

    results.completed = true;
    if (comparablePairCount(config, results) == 0)
        results.fatalError = "No input file produced a valid comparable result for both groups.";

    printOverallPerformance(config, results, options);
    if (options.createReport)
        writeMarkdownReport(config, results, reportPath, options);

    if (!results.fatalError.empty() || hasRunFailures(config, results))
        return 2;
    return 0;
}
} // namespace benchmark
