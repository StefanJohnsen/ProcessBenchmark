#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "Benchmark.h"
#include "CmdArgs.h"
#include "Config.h"
#include "Utility.h"

namespace
{
static std::filesystem::path createReportPath(const std::filesystem::path& configPath)
{
    auto report = configPath;
    report.replace_extension(L".md");
    return report;
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
        const auto plan = benchmark::buildBenchmarkPlan(config);
        const auto reportPath = createReportPath(config.configPath);

        std::cout << "Configuration validated successfully.\n"
                  << "Supported input files: " << plan.files.size() << '\n'
                  << "Runs per file and group: " << config.runsPerFile << '\n'
                  << "Measurements: "
                  << (options.measureTime && options.measureMemory
                          ? "run time and peak RAM"
                          : (options.measureTime ? "run time only" : "peak RAM only"))
                  << '\n';
        if (options.createReport)
            std::cout << "Markdown report: " << benchmark::pathToUtf8(reportPath) << '\n';
        std::cout << '\n';

        const auto exitCode = benchmark::runBenchmark(config, plan, reportPath, options);
        if (exitCode == 0)
            std::cout << "\nBenchmark completed successfully.\n";
        else
            std::cout << "\nBenchmark completed with errors.\n";
        if (options.createReport)
            std::cout << "Report: " << benchmark::pathToUtf8(reportPath) << '\n';
        return exitCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
