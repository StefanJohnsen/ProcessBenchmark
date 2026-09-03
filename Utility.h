#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace benchmark
{
std::string lowerAscii(std::string value);
std::filesystem::path pathFromUtf8(const std::string& value);
std::string pathToUtf8(const std::filesystem::path& value);
std::filesystem::path pathWithoutExtension(std::filesystem::path value);
std::string extensionWithoutDot(std::string value);
std::filesystem::path normalizeAbsolutePath(const std::filesystem::path& value);
std::wstring quoteWindowsArgument(const std::wstring& value);
std::string windowsErrorMessage(uint32_t errorCode);
std::string formatDuration(double milliseconds);
std::string formatMiB(uint64_t bytes);
std::string formatBytes(uint64_t bytes);
std::string formatPercent(double value);
double median(std::vector<double> values);
uint64_t median(std::vector<uint64_t> values);
std::string markdownCode(std::string value);
} // namespace benchmark
