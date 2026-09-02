#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "Benchmark.h"
#include "CmdArgs.h"
#include "Config.h"
#include "Utility.h"

namespace
{
struct OutputPaths final
{
    std::filesystem::path report;
    std::filesystem::path logs;
};

static OutputPaths createOutputPaths(const std::filesystem::path& configPath)
{
    auto stem = configPath.stem().wstring();
    if (stem.empty())
        stem = L"benchmark";

    auto report = configPath;
    report.replace_extension(L".md");

    const auto timestamp = benchmark::pathFromUtf8(benchmark::timestampForFileName()).wstring();
    const auto logBase = stem + L"-" + timestamp + L"-logs";

    for (size_t suffix = 0; suffix < 1000; ++suffix)
    {
        const auto unique = suffix == 0 ? logBase : logBase + L"-" + std::to_wstring(suffix + 1);
        const auto logs = configPath.parent_path() / unique;
        if (!std::filesystem::exists(logs))
            return {report, logs};
    }

    throw std::runtime_error("Could not create a unique converter log directory name.");
}
} // namespace

int wmain(const int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    try
    {
        const auto arguments = benchmark::cmd::parse(argc, argv);
        if (arguments.showHelp)
        {
            std::cout << benchmark::cmd::helpText();
            return 0;
        }
        if (arguments.showVersion)
        {
            std::cout << benchmark::cmd::Version << '\n';
            return 0;
        }

        const auto& options = arguments.benchmarkOptions;
        const auto config = benchmark::loadConfig(arguments.configPath);
        const auto plan = benchmark::buildInputPlan(config);
        const auto outputPaths = createOutputPaths(config.configPath);

        std::cout << "Configuration validated successfully.\n"
                  << "Supported input files: " << plan.files.size() << '\n'
                  << "Unsupported input files skipped: " << plan.unsupportedFileCount << '\n'
                  << "Runs per file and group: " << config.runsPerFile << '\n'
                  << "Execution mode: sequential\n"
                  << "Measurements: "
                  << (options.measureTime && options.measureMemory
                          ? "run time and peak RAM"
                          : (options.measureTime ? "run time only" : "peak RAM only"))
                  << '\n'
                  << "Markdown report: " << benchmark::pathToUtf8(outputPaths.report) << "\n\n";

        const auto exitCode = benchmark::runBenchmark(config, plan, outputPaths.report, outputPaths.logs, options);
        if (exitCode == 0)
            std::cout << "\nBenchmark completed successfully.\n";
        else
            std::cout << "\nBenchmark completed with errors.\n";
        std::cout << "Report: " << benchmark::pathToUtf8(outputPaths.report) << '\n';
        return exitCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
