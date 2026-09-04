#include "Hardware.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Utility.h"

namespace benchmark
{
namespace
{
std::string registryString(const HKEY key, const wchar_t* name)
{
    wchar_t value[512]{};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, &type, value, &bytes) != ERROR_SUCCESS)
        return "N/A";
    return pathToUtf8(std::filesystem::path(value));
}

uint64_t registryDword(const HKEY key, const wchar_t* name)
{
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    return RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS ? value : 0;
}

size_t physicalCoreCount()
{
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    std::vector<std::byte> buffer(bytes);
    if (bytes == 0 ||
        !GetLogicalProcessorInformationEx(
            RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &bytes))
        return 0;

    size_t count = 0;
    for (DWORD offset = 0; offset < bytes;)
    {
        const auto* item = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        if (item->Relationship == RelationProcessorCore)
            ++count;
        offset += item->Size;
    }
    return count;
}
} // namespace

HardwareInfo collectHardwareInfo()
{
    HKEY cpu = nullptr;
    RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &cpu);
    const auto cpuName = cpu != nullptr ? registryString(cpu, L"ProcessorNameString") : "N/A";
    const auto cpuVendor = cpu != nullptr ? registryString(cpu, L"VendorIdentifier") : "N/A";
    const auto cpuClockMhz = cpu != nullptr ? registryDword(cpu, L"~MHz") : 0;
    if (cpu != nullptr)
        RegCloseKey(cpu);

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    const bool memoryAvailable = GlobalMemoryStatusEx(&memory) != FALSE;
    ULONGLONG installedKiB = 0;
    const bool installedMemoryAvailable = GetPhysicallyInstalledSystemMemory(&installedKiB) != FALSE;
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);

    HardwareInfo result;
    result.cpuName = cpuName;
    result.cpuVendor = cpuVendor;
    result.physicalCores = physicalCoreCount();
    result.logicalProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    result.cpuClock = cpuClockMhz == 0 ? "N/A" : std::to_string(cpuClockMhz) + " MHz";
    result.installedMemory = installedMemoryAvailable ? formatBytes(installedKiB * 1024ULL) : "N/A";
    result.usableMemory = memoryAvailable ? formatBytes(memory.ullTotalPhys) : "N/A";
    result.availableMemory = memoryAvailable ? formatBytes(memory.ullAvailPhys) : "N/A";
    result.pageFileLimit = memoryAvailable ? formatBytes(memory.ullTotalPageFile) : "N/A";
    result.memoryLoad = memoryAvailable ? std::to_string(memory.dwMemoryLoad) + '%' : "N/A";
    result.architecture = system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "Other";
    return result;
}
} // namespace benchmark
