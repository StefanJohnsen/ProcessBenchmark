#include "Hardware.h"

#include <sys/utsname.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include "Utility.h"

namespace benchmark
{
namespace
{
std::string trimmed(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string readFile(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::optional<std::string> colonSeparatedField(const std::string& text, const std::string& key)
{
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        const auto separator = line.find(':');
        if (separator == std::string::npos)
            continue;
        if (trimmed(line.substr(0, separator)) == key)
            return trimmed(line.substr(separator + 1));
    }
    return std::nullopt;
}

std::optional<uint64_t> meminfoValueKb(const std::string& memInfo, const std::string& key)
{
    const auto field = colonSeparatedField(memInfo, key);
    if (!field.has_value())
        return std::nullopt;
    try
    {
        return static_cast<uint64_t>(std::stoull(*field));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

size_t physicalCoreCount(const std::string& cpuInfo)
{
    std::istringstream stream(cpuInfo);
    std::string line;
    std::string currentPhysicalId = "0";
    std::set<std::pair<std::string, std::string>> cores;
    bool sawTopology = false;
    while (std::getline(stream, line))
    {
        const auto separator = line.find(':');
        if (separator == std::string::npos)
            continue;
        const auto key = trimmed(line.substr(0, separator));
        const auto value = trimmed(line.substr(separator + 1));
        if (key == "physical id")
            currentPhysicalId = value;
        else if (key == "core id")
        {
            cores.emplace(currentPhysicalId, value);
            sawTopology = true;
        }
    }
    return sawTopology ? cores.size() : 0;
}

std::string architectureName()
{
    utsname info{};
    if (uname(&info) != 0)
        return "Other";
    const std::string machine = info.machine;
    return machine == "x86_64" ? "x64" : machine;
}
} // namespace

HardwareInfo collectHardwareInfo()
{
    const auto cpuInfo = readFile("/proc/cpuinfo");
    const auto memInfo = readFile("/proc/meminfo");

    HardwareInfo result;
    result.cpuName = colonSeparatedField(cpuInfo, "model name").value_or("N/A");
    result.cpuVendor = colonSeparatedField(cpuInfo, "vendor_id").value_or("N/A");
    result.physicalCores = physicalCoreCount(cpuInfo);
    result.logicalProcessors = static_cast<size_t>(std::max<long>(1, sysconf(_SC_NPROCESSORS_ONLN)));

    const auto clockField = colonSeparatedField(cpuInfo, "cpu MHz");
    if (clockField.has_value())
    {
        try
        {
            result.cpuClock = std::to_string(static_cast<uint64_t>(std::llround(std::stod(*clockField)))) + " MHz";
        }
        catch (...)
        {
            result.cpuClock = "N/A";
        }
    }
    else
    {
        result.cpuClock = "N/A";
    }

    const auto memTotalKb = meminfoValueKb(memInfo, "MemTotal");
    const auto memAvailableKb = meminfoValueKb(memInfo, "MemAvailable");
    const auto commitLimitKb = meminfoValueKb(memInfo, "CommitLimit");

    result.installedMemory = "N/A";
    result.usableMemory = memTotalKb.has_value() ? formatBytes(memTotalKb.value() * 1024ULL) : "N/A";
    result.availableMemory = memAvailableKb.has_value() ? formatBytes(memAvailableKb.value() * 1024ULL) : "N/A";
    result.pageFileLimit = commitLimitKb.has_value() ? formatBytes(commitLimitKb.value() * 1024ULL) : "N/A";
    result.memoryLoad = (memTotalKb.has_value() && memAvailableKb.has_value() && memTotalKb.value() > 0)
                            ? std::to_string(static_cast<unsigned>(
                                  100.0 * static_cast<double>(memTotalKb.value() - memAvailableKb.value()) /
                                  static_cast<double>(memTotalKb.value()))) +
                                  '%'
                            : "N/A";
    result.architecture = architectureName();
    return result;
}
} // namespace benchmark
