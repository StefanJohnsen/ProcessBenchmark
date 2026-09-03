#pragma once

#include <filesystem>

#include "Types.h"

namespace benchmark
{
Config loadConfig(const std::filesystem::path& configPath);
BenchmarkPlan buildBenchmarkPlan(const Config& config);
std::string expandCommandArguments(const std::string& value, const std::filesystem::path& file);
} // namespace benchmark
