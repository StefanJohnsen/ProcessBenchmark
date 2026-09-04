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
           "  -time-only    Measure and report run time only\n"
           "  -ram-only     Measure and report peak RAM only\n"
           "  -noreport     Do not create a Markdown report\n"
           "  -help         Show this help message\n"
           "  -version      Show version information\n\n"
           "Options also accept the double-dash form, for example --time-only.\n"
           "The text configuration path must be absolute.\n"
           "The Markdown report is written beside it with the .md extension.\n";
}

Arguments parse(const std::vector<std::string>& arguments)
{
    Arguments result;
    bool timeOnly = false;
    bool ramOnly = false;
    bool noReport = false;

    for (size_t index = 1; index < arguments.size(); ++index)
    {
        const auto& argument = arguments[index];
        if (argument.empty())
            continue;

        if (isFlag(argument, "help") || argument == "-h" || argument == "--h")
            result.showHelp = true;
        else if (isFlag(argument, "version"))
            result.showVersion = true;
        else if (isFlag(argument, "time-only"))
        {
            if (timeOnly)
                throw std::runtime_error("Option -time-only was specified more than once.");
            timeOnly = true;
        }
        else if (isFlag(argument, "ram-only"))
        {
            if (ramOnly)
                throw std::runtime_error("Option -ram-only was specified more than once.");
            ramOnly = true;
        }
        else if (isFlag(argument, "noreport"))
        {
            if (noReport)
                throw std::runtime_error("Option -noreport was specified more than once.");
            noReport = true;
        }
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
        if (informationFlags != 1 || timeOnly || ramOnly || noReport || !result.configPath.empty())
            throw std::runtime_error("Help and version options must be used alone.");
        return result;
    }
    if (timeOnly && ramOnly)
        throw std::runtime_error("Options -time-only and -ram-only cannot be combined.");
    if (result.configPath.empty())
        throw std::runtime_error("Missing text configuration path.");

    result.benchmarkOptions.measureTime = !ramOnly;
    result.benchmarkOptions.measureMemory = !timeOnly;
    result.benchmarkOptions.createReport = !noReport;
    return result;
}
} // namespace benchmark::cmd
