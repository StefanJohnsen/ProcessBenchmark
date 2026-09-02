#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Types.h"

namespace benchmark
{
RunResult runConverter(const std::filesystem::path& executable, const std::filesystem::path& input,
                       const std::filesystem::path& output, const std::string& commandArguments,
                       const std::filesystem::path& logFile, size_t runNumber, size_t executionOrder,
                       const BenchmarkOptions& options);
}
