#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "Types.h"
#include "Utility.h"

namespace benchmark::cmd
{
inline constexpr char Version[] = "ProcessBenchmark version 0.1.0";

struct Arguments final
{
    BenchmarkOptions benchmarkOptions;
    std::filesystem::path configPath;
    bool showHelp = false;
    bool showVersion = false;
};

inline std::string helpText()
{
    return "ProcessBenchmark\n\n"
           "Usage:\n"
           "  ProcessBenchmark.exe [options] \"C:\\full\\path\\config.txt\"\n\n"
           "Options:\n"
           "  -time-only    Measure and report run time only\n"
           "  -ram-only     Measure and report peak RAM only\n"
           "  -help         Show this help message\n"
           "  -version      Show version information\n\n"
           "Options also accept the double-dash form, for example --time-only.\n"
           "The text configuration path must be absolute.\n"
           "The Markdown report is written beside it with the .md extension.\n";
}

inline bool isFlag(const std::wstring& argument, const wchar_t* name)
{
    return argument == std::wstring(L"-") + name || argument == std::wstring(L"--") + name;
}

inline Arguments parse(const int argc, wchar_t* argv[])
{
    Arguments result;
    bool timeOnly = false;
    bool ramOnly = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::wstring argument = argv[index] != nullptr ? argv[index] : L"";
        if (argument.empty()) continue;

        if (isFlag(argument, L"help") || argument == L"-h" || argument == L"--h")
        {
            result.showHelp = true;
            continue;
        }
        if (isFlag(argument, L"version"))
        {
            result.showVersion = true;
            continue;
        }
        if (isFlag(argument, L"time-only"))
        {
            if (timeOnly) throw std::runtime_error("Option -time-only was specified more than once.");
            timeOnly = true;
            continue;
        }
        if (isFlag(argument, L"ram-only"))
        {
            if (ramOnly) throw std::runtime_error("Option -ram-only was specified more than once.");
            ramOnly = true;
            continue;
        }
        if (argument.front() == L'-')
            throw std::runtime_error("Unknown option " + pathToUtf8(std::filesystem::path(argument)) + ".");
        if (!result.configPath.empty())
            throw std::runtime_error("Only one text configuration path can be specified.");
        result.configPath = argument;
    }

    if (result.showHelp || result.showVersion)
    {
        const auto informationFlags = static_cast<int>(result.showHelp) + static_cast<int>(result.showVersion);
        if (informationFlags != 1 || timeOnly || ramOnly || !result.configPath.empty())
            throw std::runtime_error("Help and version options must be used alone.");
        return result;
    }
    if (timeOnly && ramOnly)
        throw std::runtime_error("Options -time-only and -ram-only cannot be combined.");
    if (result.configPath.empty())
        throw std::runtime_error("Missing text configuration path.");

    result.benchmarkOptions.measureTime = !ramOnly;
    result.benchmarkOptions.measureMemory = !timeOnly;
    return result;
}
} // namespace benchmark::cmd
