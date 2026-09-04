#pragma once

#include <filesystem>
#include <string>

namespace benchmark::detail
{
std::string localReportTime();
std::string formatArgumentsForReport(const std::string& arguments);
void renameReportFile(const std::filesystem::path& temporary, const std::filesystem::path& reportPath);
std::string hardwareMemorySourceNote();
std::string memoryMeasurementNote();
} // namespace benchmark::detail
