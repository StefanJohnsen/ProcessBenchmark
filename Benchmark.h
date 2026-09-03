#pragma once

#include <filesystem>

#include "Types.h"

namespace benchmark
{
int runBenchmark(const Config& config, const BenchmarkPlan& plan, const std::filesystem::path& reportPath,
                 const BenchmarkOptions& options);
}
