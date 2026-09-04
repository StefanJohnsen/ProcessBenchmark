#include "Benchmark.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "ProcessRunner.h"
#include "Report.h"
#include "Statistics.h"
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

class ConsoleTable final
{
  public:
    ConsoleTable(std::vector<std::string> headers, std::vector<ConsoleAlignment> alignments)
        : headers_(std::move(headers)), alignments_(std::move(alignments))
    {
        if (headers_.size() != alignments_.size())
            throw std::logic_error("Console table columns and alignments must have the same size.");
        for (const auto& header : headers_)
            widths_.emplace_back(displayedWidth(header));
    }

    void include(const size_t column, const std::string_view value)
    {
        widths_.at(column) = std::max(widths_[column], displayedWidth(std::string(value)));
    }

    void include(const std::vector<std::string>& row)
    {
        requireValidRow(row);
        for (size_t index = 0; index < row.size(); ++index)
            include(index, row[index]);
    }

    void printHeader() const
    {
        printRow(headers_);
        printSeparator();
    }

    void printSeparator() const
    {
        for (size_t index = 0; index < widths_.size(); ++index)
        {
            if (index != 0)
                std::cout << "-+-";
            std::cout << std::string(widths_[index], '-');
        }
        std::cout << '\n' << std::flush;
    }

    void printRow(const std::vector<std::string>& row) const
    {
        requireValidRow(row);
        for (size_t index = 0; index < row.size(); ++index)
        {
            if (index != 0)
                std::cout << " | ";
            std::cout << padded(row[index], widths_[index], alignments_[index]);
        }
        std::cout << '\n' << std::flush;
    }

  private:
    void requireValidRow(const std::vector<std::string>& row) const
    {
        if (row.size() != headers_.size())
            throw std::logic_error("Console table row has an unexpected number of columns.");
    }

    std::vector<std::string> headers_;
    std::vector<ConsoleAlignment> alignments_;
    std::vector<size_t> widths_;
};

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
    const auto aggregate = calculateAggregate(config, results);
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
        const auto comparison = compareLower(aggregate.elapsedMilliseconds[0], aggregate.elapsedMilliseconds[1]);
        const auto maximum = std::max(aggregate.elapsedMilliseconds[0], aggregate.elapsedMilliseconds[1]);
        timeTie = comparison.tie;
        timeWinner = comparison.winner;
        timeFactor = formatRatio(comparison.factor);
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
        const auto comparison = compareLower(static_cast<double>(aggregate.peakWorkingSetBytes[0]),
                                             static_cast<double>(aggregate.peakWorkingSetBytes[1]));
        memoryTie = comparison.tie;
        memoryWinner = comparison.winner;
        memoryImprovement = formatPercent(comparison.percentLess) + " less";
        for (size_t index = 0; index < GroupCount; ++index)
            rows.push_back(
                {"Highest median RAM", config.groups[index].name,
                 consoleBar(static_cast<double>(aggregate.peakWorkingSetBytes[index]), static_cast<double>(maximum)),
                 formatMiB(aggregate.peakWorkingSetBytes[index]),
                 memoryTie ? "0.0%" : (index == memoryWinner ? memoryImprovement : "-"),
                 memoryTie || index == memoryWinner ? "\xf0\x9f\x9f\xa2" : ""});
    }

    ConsoleTable table(headers, alignments);
    for (const auto& row : rows)
        table.include(row);
    table.printHeader();
    for (const auto& row : rows)
        table.printRow(row);
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

static ConsoleTable createConsoleRunTable(const Config& config, const BenchmarkResults& results,
                                          const BenchmarkOptions& options)
{
    std::vector<std::string> headers = {"File name", "Ext", "Size", "Group", "Run"};
    std::vector<ConsoleAlignment> alignments = {ConsoleAlignment::Left, ConsoleAlignment::Center,
                                                ConsoleAlignment::Right, ConsoleAlignment::Center,
                                                ConsoleAlignment::Center};
    if (options.measureTime)
    {
        headers.emplace_back("Time");
        alignments.emplace_back(ConsoleAlignment::Center);
    }
    if (options.measureMemory)
    {
        headers.emplace_back("RAM");
        alignments.emplace_back(ConsoleAlignment::Right);
    }
    headers.emplace_back("Exit");
    headers.emplace_back("Status");
    alignments.emplace_back(ConsoleAlignment::Center);
    alignments.emplace_back(ConsoleAlignment::Center);

    ConsoleTable table(std::move(headers), std::move(alignments));
    for (const auto& file : results.files)
    {
        table.include(0, fitConsoleFileName(pathWithoutExtension(file.file.path.filename())));
        table.include(1, extensionWithoutDot(file.file.extension));
        table.include(2, formatBytes(file.file.sizeBytes));
    }
    for (const auto& group : config.groups)
        table.include(3, group.name);
    table.include(4, std::to_string(config.runsPerFile));

    size_t metricIndex = 5;
    if (options.measureTime)
    {
        table.include(metricIndex, "00:00.000");
        ++metricIndex;
    }
    if (options.measureMemory)
    {
        table.include(metricIndex, "000000 MiB");
    }
    return table;
}

static void printConsoleRunTableHeader(const ConsoleTable& table)
{
    std::cout << "\nIndividual runs\n\n";
    table.printHeader();
}

static void printConsoleRun(const ConsoleTable& table, const Config& config, const FileResult& file,
                            const size_t groupIndex, const RunResult& run, const BenchmarkOptions& options)
{
    std::vector<std::string> row = {fitConsoleFileName(pathWithoutExtension(file.file.path.filename())),
                                    extensionWithoutDot(file.file.extension), formatBytes(file.file.sizeBytes),
                                    config.groups[groupIndex].name, std::to_string(run.runNumber)};
    if (options.measureTime)
        row.emplace_back(formatDuration(run.elapsedMilliseconds));
    if (options.measureMemory)
        row.emplace_back(formatMiB(run.peakWorkingSetBytes));
    row.emplace_back(exitCodeText(run));
    row.emplace_back(run.success ? "OK" : "Failed");

    table.printRow(row);
}
} // namespace

int runBenchmark(const Config& config, const BenchmarkPlan& plan, const std::filesystem::path& reportPath,
                 const BenchmarkOptions& options)
{
    BenchmarkResults results;
    results.files.reserve(plan.files.size());
    for (const auto& file : plan.files)
        results.files.emplace_back(FileResult{.file = file});

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
                    run = runProcess(executable, process.commandArguments, runNumber, options);
                }
                catch (const std::exception& error)
                {
                    run = failedRun(runNumber, error.what());
                }

                file.groupRuns[groupIndex].emplace_back(std::move(run));
                printConsoleRun(consoleTable, config, file, groupIndex, file.groupRuns[groupIndex].back(), options);
            }
            if (config.runsPerFile >= 2)
                consoleTable.printSeparator();
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
