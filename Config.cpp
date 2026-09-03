#include "Config.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "Utility.h"

namespace benchmark
{
namespace
{
enum class Section
{
    None,
    Engines,
    Files,
    Processes
};

static std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

static std::vector<std::string> columns(const std::string& line, const size_t maximumColumns = 0)
{
    std::vector<std::string> result;
    size_t start = 0;
    while (true)
    {
        if (maximumColumns != 0 && result.size() + 1 == maximumColumns)
        {
            result.emplace_back(trim(line.substr(start)));
            return result;
        }
        const auto separator = line.find('|', start);
        result.emplace_back(trim(line.substr(start, separator == std::string::npos ? separator : separator - start)));
        if (separator == std::string::npos)
            return result;
        start = separator + 1;
    }
}

static bool separatorLine(const std::string& line)
{
    return !line.empty() && line.find_first_not_of("-| ") == std::string::npos;
}

static size_t parseIndex(const std::string& text, const std::string& context)
{
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
        throw std::runtime_error(context + " has an invalid index.");
    try
    {
        return static_cast<size_t>(std::stoull(text));
    }
    catch (...)
    {
        throw std::runtime_error(context + " has an invalid index.");
    }
}

static std::filesystem::path absolutePath(const std::string& text, const std::string& context)
{
    auto value = text;
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);
    std::replace(value.begin(), value.end(), '\\', '/');

    auto path = pathFromUtf8(value);
    if (!path.is_absolute())
        throw std::runtime_error(context + " must be an absolute path.");
    return normalizeAbsolutePath(path);
}

static void requireRegularFile(const std::filesystem::path& path, const std::string& context)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error)
        throw std::runtime_error(context + " does not point to an existing file: " + pathToUtf8(path));
}

static std::string replaceAll(std::string value, const std::string& token, const std::string& replacement)
{
    size_t position = 0;
    while ((position = value.find(token, position)) != std::string::npos)
    {
        value.replace(position, token.size(), replacement);
        position += replacement.size();
    }
    return value;
}

} // namespace

std::string expandCommandArguments(const std::string& value, const std::filesystem::path& file)
{
    auto directory = pathToUtf8(file.parent_path());
    if (!directory.empty() && directory.back() != '/')
        directory.push_back('/');
    auto result = replaceAll(value, "{file.dir}", directory);
    result = replaceAll(result, "{file.name}", pathToUtf8(file.stem()));
    result = replaceAll(result, "{file.ext}", pathToUtf8(file.extension()));
    result = replaceAll(result, "{file}", pathToUtf8(file));
    if (result.find("{file.") != std::string::npos)
        throw std::runtime_error("Command Arguments contains an unknown file placeholder: " + value);
    return result;
}

Config loadConfig(const std::filesystem::path& configPath)
{
    if (!configPath.is_absolute())
        throw std::runtime_error("The text configuration path must be absolute.");
    Config config;
    config.configPath = normalizeAbsolutePath(configPath);
    if (lowerAscii(pathToUtf8(config.configPath.extension())) != ".txt")
        throw std::runtime_error("Configuration file must have the .txt extension.");
    requireRegularFile(config.configPath, "Configuration");

    std::ifstream stream(config.configPath, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Could not open text configuration: " + pathToUtf8(config.configPath));

    Section section = Section::None;
    ProcessGroup* currentGroup = nullptr;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (std::any_of(line.begin(), line.end(), [](const unsigned char character) { return character > 0x7f; }))
            throw std::runtime_error("Configuration must contain only ASCII characters; invalid character on line " +
                                     std::to_string(lineNumber) + ".");
        const auto text = trim(line);
        if (text.empty())
            continue;
        if (config.title.empty())
        {
            config.title = text;
            continue;
        }
        if (text.rfind("RUNS:", 0) == 0)
        {
            config.runsPerFile = parseIndex(trim(text.substr(5)), "RUNS");
            if (config.runsPerFile < 1 || config.runsPerFile > 100)
                throw std::runtime_error("RUNS must be between 1 and 100.");
            continue;
        }
        if (text == "ENGINES")
        {
            section = Section::Engines;
            currentGroup = nullptr;
            continue;
        }
        if (text == "FILES")
        {
            section = Section::Files;
            currentGroup = nullptr;
            continue;
        }
        if (text.rfind("PROCESSES - ", 0) == 0)
        {
            if (config.groups.size() >= GroupCount)
                throw std::runtime_error("Configuration can contain at most two process groups.");
            const auto name = trim(text.substr(12));
            if (name.empty())
                throw std::runtime_error("A process group name must not be empty.");
            config.groups.emplace_back(ProcessGroup{.name = name});
            currentGroup = &config.groups.back();
            section = Section::Processes;
            continue;
        }
        if (text.rfind("FILE PLACEHOLDERS", 0) == 0)
            break;
        if (section == Section::None || text == "INSTRUCTIONS" || text[0] == '#' ||
            text.rfind("Placeholders ", 0) == 0 || text.rfind("Example file:", 0) == 0 || separatorLine(text))
            continue;

        const auto values = columns(text, section == Section::Processes ? 3 : 0);
        if ((section == Section::Engines && values.size() >= 2 && values[0] == "Name") ||
            (section == Section::Files && values.size() >= 2 && values[0] == "Index") ||
            (section == Section::Processes && values.size() >= 3 && values[0] == "Index"))
            continue;

        const auto context = "Line " + std::to_string(lineNumber);
        if (section == Section::Engines)
        {
            if (values.size() != 2 || values[0].empty() || values[1].empty())
                throw std::runtime_error(context + " must contain: Name | Executable.");
            if (config.engines.contains(values[0]))
                throw std::runtime_error(context + " contains duplicate engine '" + values[0] + "'.");
            auto path = absolutePath(values[1], context + " executable");
            requireRegularFile(path, context + " executable");
            if (lowerAscii(pathToUtf8(path.extension())) != ".exe")
                throw std::runtime_error(context + " executable must have the .exe extension.");
            config.engines.emplace(values[0], std::move(path));
        }
        else if (section == Section::Files)
        {
            if (values.size() != 2)
                throw std::runtime_error(context + " must contain: Index | File.");
            const auto index = parseIndex(values[0], context);
            if (index != config.files.size())
                throw std::runtime_error(context + " index must be " + std::to_string(config.files.size()) + ".");
            auto path = absolutePath(values[1], context + " file");
            requireRegularFile(path, context + " file");
            config.files.emplace_back(std::move(path));
        }
        else if (section == Section::Processes)
        {
            if (currentGroup == nullptr || values.size() != 3)
                throw std::runtime_error(context + " must contain: Index | Engine | Command Arguments.");
            const auto index = parseIndex(values[0], context);
            if (index != currentGroup->processes.size())
                throw std::runtime_error(context + " index must be " + std::to_string(currentGroup->processes.size()) +
                                         ".");
            currentGroup->processes.emplace_back(ProcessConfig{.engineName = values[1], .commandArguments = values[2]});
        }
    }

    if (config.runsPerFile == 0)
        throw std::runtime_error("Configuration is missing RUNS.");
    if (config.engines.empty())
        throw std::runtime_error("ENGINES must contain at least one engine.");
    if (config.files.empty())
        throw std::runtime_error("FILES must contain at least one file.");
    if (config.groups.size() != GroupCount)
        throw std::runtime_error("Configuration must contain exactly two process groups.");
    if (config.groups[0].name == config.groups[1].name)
        throw std::runtime_error("Process group names must be unique.");
    for (auto& group : config.groups)
    {
        if (group.processes.size() != config.files.size())
            throw std::runtime_error("Group '" + group.name + "' must contain one process for every file.");
        for (size_t index = 0; index < group.processes.size(); ++index)
        {
            auto& process = group.processes[index];
            if (!config.engines.contains(process.engineName))
                throw std::runtime_error("Group '" + group.name + "' references unknown engine '" + process.engineName +
                                         "'.");
            process.commandArguments = expandCommandArguments(process.commandArguments, config.files[index]);
        }
    }
    return config;
}

BenchmarkPlan buildBenchmarkPlan(const Config& config)
{
    BenchmarkPlan plan;
    for (size_t index = 0; index < config.files.size(); ++index)
    {
        TestFile file;
        file.path = config.files[index];
        file.extension = lowerAscii(pathToUtf8(file.path.extension()));
        std::error_code error;
        file.sizeBytes = std::filesystem::file_size(file.path, error);
        if (error)
            throw std::runtime_error("Could not read input file size: " + pathToUtf8(file.path));
        plan.files.emplace_back(std::move(file));
    }
    return plan;
}
} // namespace benchmark
