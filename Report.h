#pragma once

#include <filesystem>

#include "Types.h"

namespace benchmark
{
void writeMarkdownReport(const Config& config, const BenchmarkResults& results, const std::filesystem::path& reportPath,
                         const BenchmarkOptions& options);
} // namespace benchmark
