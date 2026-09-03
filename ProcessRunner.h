#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Types.h"

namespace benchmark
{
RunResult runConverter(const std::filesystem::path& executable, const std::string& commandArguments, size_t runNumber,
                       const BenchmarkOptions& options);
}
