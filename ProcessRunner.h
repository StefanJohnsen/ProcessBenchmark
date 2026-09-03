#pragma once

#include <filesystem>
#include <string>

#include "Types.h"

namespace benchmark
{
RunResult runProcess(const std::filesystem::path& executable, const std::string& commandArguments, size_t runNumber,
                     const BenchmarkOptions& options);
}
