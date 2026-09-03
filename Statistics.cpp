#include "Statistics.h"

#include <algorithm>
#include <utility>

#include "Utility.h"

namespace benchmark
{
RunSummary summarizeRuns(const std::vector<RunResult>& runs, const size_t expectedRuns)
{
    RunSummary summary;
    if (runs.size() < expectedRuns)
        return summary;

    if (runs.size() != expectedRuns ||
        std::any_of(runs.begin(), runs.end(), [](const RunResult& run) { return !run.success; }))
    {
        summary.state = ResultState::Invalid;
        return summary;
    }

    std::vector<double> elapsed;
    std::vector<uint64_t> workingSets;
    elapsed.reserve(runs.size());
    workingSets.reserve(runs.size());
    for (const auto& run : runs)
    {
        elapsed.emplace_back(run.elapsedMilliseconds);
        workingSets.emplace_back(run.peakWorkingSetBytes);
    }

    summary.state = ResultState::Valid;
    summary.elapsedMilliseconds = median(std::move(elapsed));
    summary.peakWorkingSetBytes = median(std::move(workingSets));
    return summary;
}

GroupSummaries summarizeGroups(const Config& config, const FileResult& file)
{
    return {summarizeRuns(file.groupRuns[0], config.runsPerFile), summarizeRuns(file.groupRuns[1], config.runsPerFile)};
}

bool isComparable(const GroupSummaries& summaries)
{
    return summaries[0].state == ResultState::Valid && summaries[1].state == ResultState::Valid;
}

AggregateResult calculateAggregate(const Config& config, const std::vector<const FileResult*>& files)
{
    AggregateResult result;
    result.totalFiles = files.size();

    for (const auto* file : files)
    {
        const auto summaries = summarizeGroups(config, *file);
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
            if (summaries[groupIndex].state == ResultState::Invalid)
                ++result.invalidFiles[groupIndex];

        if (!isComparable(summaries))
            continue;

        ++result.comparableFiles;
        for (size_t groupIndex = 0; groupIndex < GroupCount; ++groupIndex)
        {
            result.elapsedMilliseconds[groupIndex] += summaries[groupIndex].elapsedMilliseconds;
            result.peakWorkingSetBytes[groupIndex] =
                std::max(result.peakWorkingSetBytes[groupIndex], summaries[groupIndex].peakWorkingSetBytes);
        }
    }
    return result;
}

AggregateResult calculateAggregate(const Config& config, const BenchmarkResults& results)
{
    std::vector<const FileResult*> files;
    files.reserve(results.files.size());
    for (const auto& file : results.files)
        files.emplace_back(&file);
    return calculateAggregate(config, files);
}

LowerIsBetter compareLower(const double first, const double second)
{
    const std::array values = {first, second};
    LowerIsBetter result;
    result.tie = first == second;
    result.winner = first < second ? 0 : 1;
    if (result.tie)
        return result;

    const auto loser = 1 - result.winner;
    if (values[result.winner] > 0.0)
        result.factor = values[loser] / values[result.winner];
    if (values[loser] > 0.0)
        result.percentLess = (values[loser] - values[result.winner]) / values[loser] * 100.0;
    return result;
}

size_t comparablePairCount(const Config& config, const BenchmarkResults& results)
{
    return static_cast<size_t>(std::count_if(results.files.begin(), results.files.end(), [&](const FileResult& file)
                                             { return isComparable(summarizeGroups(config, file)); }));
}

bool hasRunFailures(const Config& config, const BenchmarkResults& results)
{
    if (!results.fatalError.empty())
        return true;

    return std::any_of(results.files.begin(), results.files.end(),
                       [&](const FileResult& file)
                       {
                           const auto summaries = summarizeGroups(config, file);
                           return summaries[0].state == ResultState::Invalid ||
                                  summaries[1].state == ResultState::Invalid;
                       });
}
} // namespace benchmark
