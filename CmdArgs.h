#pragma once

#include <filesystem>
#include <string>

#include "Types.h"

namespace benchmark::cmd
{
#define PROCESS_BENCHMARK_VERSION "1.0.0"
inline constexpr char VersionNumber[] = PROCESS_BENCHMARK_VERSION;
inline constexpr char Version[] = "ProcessBenchmark version " PROCESS_BENCHMARK_VERSION;
#undef PROCESS_BENCHMARK_VERSION

struct Arguments final
{
    BenchmarkOptions benchmarkOptions;
    std::filesystem::path configPath;
    bool showHelp = false;
    bool showVersion = false;
};

std::string helpText();
Arguments parse(int argc, wchar_t* argv[]);
} // namespace benchmark::cmd
