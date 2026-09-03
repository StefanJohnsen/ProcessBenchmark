#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace benchmark
{
inline constexpr size_t GroupCount = 2;
inline constexpr unsigned long ProcessPollIntervalMilliseconds = 50;
struct ProcessConfig final
{
    std::string engineName;
    std::string commandArguments;
};

struct ConverterGroup final
{
    std::string name;
    std::vector<ProcessConfig> processes;
};

struct Config final
{
    std::filesystem::path configPath;
    std::string title;
    size_t runsPerFile = 0;
    std::map<std::string, std::filesystem::path> engines;
    std::vector<std::filesystem::path> files;
    std::vector<ConverterGroup> groups;
};

struct BenchmarkOptions final
{
    bool measureTime = true;
    bool measureMemory = true;
    bool createReport = true;
};

struct InputFile final
{
    std::filesystem::path source;
    std::string extension;
    uint64_t sourceBytes = 0;
};

struct InputPlan final
{
    std::vector<InputFile> files;
};

struct RunResult final
{
    size_t runNumber = 0;
    double elapsedMilliseconds = 0.0;
    uint64_t peakWorkingSetBytes = 0;
    std::optional<uint32_t> exitCode;
    bool success = false;
    std::string error;
};

struct FileResult final
{
    InputFile input;
    std::array<std::vector<RunResult>, GroupCount> groupRuns;
};

struct BenchmarkResults final
{
    std::vector<FileResult> files;
    bool completed = false;
    std::string fatalError;
};
} // namespace benchmark
