#pragma once

#include <filesystem>

#include "Types.h"

namespace benchmark
{
void writeMarkdownReport(const Config& config, const BenchmarkResults& results,
                         const std::filesystem::path& reportPath, const BenchmarkOptions& options);
size_t comparablePairCount(const Config& config, const BenchmarkResults& results);
bool hasRunFailures(const Config& config, const BenchmarkResults& results);
} // namespace benchmark
