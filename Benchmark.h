#pragma once

#include <filesystem>

#include "Types.h"

namespace benchmark
{
int runBenchmark(const Config& config, const InputPlan& plan, const std::filesystem::path& reportPath,
                 const std::filesystem::path& logDirectory, const BenchmarkOptions& options);
}
