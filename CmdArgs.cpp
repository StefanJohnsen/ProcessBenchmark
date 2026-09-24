#include "CmdArgs.h"

#include <stdexcept>
#include <string>

#include "Utility.h"

namespace benchmark::cmd
{
namespace
{
bool isFlag(const std::string& argument, const char* name)
{
    return argument == std::string("-") + name || argument == std::string("--") + name;
}
} // namespace

std::string helpText()
{
    return "ProcessBenchmark\n\n"
           "Usage:\n"
           "  ProcessBenchmark [options] \"/full/path/processTest.txt\"\n\n"
           "Options:\n"
           "  -help         Show this help message\n"
           "  -version      Show version information\n\n"
           "The text configuration path must be absolute.\n"
           "All configuration is done in the config file, including:\n"
           "  - Measurement types (time and RAM) via MEASURE TIME and MEASURE RAM\n"
           "  - Report generation via CREATE REPORT\n";
}

Arguments parse(const std::vector<std::string>& arguments)
{
    Arguments result;

    for (size_t index = 1; index < arguments.size(); ++index)
    {
        const auto& argument = arguments[index];
        if (argument.empty())
            continue;

        if (isFlag(argument, "help") || argument == "-h" || argument == "--h")
            result.showHelp = true;
        else if (isFlag(argument, "version"))
            result.showVersion = true;
        else if (argument.front() == '-')
            throw std::runtime_error("Unknown option " + argument + ".");
        else
        {
            if (!result.configPath.empty())
                throw std::runtime_error("Only one text configuration path can be specified.");
            result.configPath = pathFromUtf8(argument);
        }
    }

    if (result.showHelp || result.showVersion)
    {
        const auto informationFlags = static_cast<int>(result.showHelp) + static_cast<int>(result.showVersion);
        if (informationFlags != 1 || !result.configPath.empty())
            throw std::runtime_error("Help and version options must be used alone.");
        return result;
    }
    if (result.configPath.empty())
        throw std::runtime_error("Missing text configuration path.");

    result.benchmarkOptions.measureTime = true;
    result.benchmarkOptions.measureMemory = true;
    result.benchmarkOptions.createReport = true;
    return result;
}
} // namespace benchmark::cmd
