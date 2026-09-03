#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Types.h"

namespace benchmark
{
enum class ResultState
{
    Pending,
    Valid,
    Invalid
};

struct RunSummary final
{
    ResultState state = ResultState::Pending;
    double elapsedMilliseconds = 0.0;
    uint64_t peakWorkingSetBytes = 0;
};

using GroupSummaries = std::array<RunSummary, GroupCount>;

struct AggregateResult final
{
    size_t totalFiles = 0;
    size_t comparableFiles = 0;
    std::array<size_t, GroupCount> invalidFiles{};
    std::array<double, GroupCount> elapsedMilliseconds{};
    std::array<uint64_t, GroupCount> peakWorkingSetBytes{};
};

struct LowerIsBetter final
{
    bool tie = false;
    size_t winner = 0;
    double factor = 0.0;
    double percentLess = 0.0;
};

RunSummary summarizeRuns(const std::vector<RunResult>& runs, size_t expectedRuns);
GroupSummaries summarizeGroups(const Config& config, const FileResult& file);
bool isComparable(const GroupSummaries& summaries);
AggregateResult calculateAggregate(const Config& config, const std::vector<const FileResult*>& files);
AggregateResult calculateAggregate(const Config& config, const BenchmarkResults& results);
LowerIsBetter compareLower(double first, double second);
size_t comparablePairCount(const Config& config, const BenchmarkResults& results);
bool hasRunFailures(const Config& config, const BenchmarkResults& results);
} // namespace benchmark
