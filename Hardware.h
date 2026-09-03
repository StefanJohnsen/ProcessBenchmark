#pragma once

#include <cstddef>
#include <string>

namespace benchmark
{
struct HardwareInfo final
{
    std::string cpuName;
    std::string cpuVendor;
    size_t physicalCores = 0;
    size_t logicalProcessors = 0;
    std::string cpuClock;
    std::string installedMemory;
    std::string usableMemory;
    std::string availableMemory;
    std::string pageFileLimit;
    std::string memoryLoad;
    std::string architecture;
};

HardwareInfo collectHardwareInfo();
} // namespace benchmark
