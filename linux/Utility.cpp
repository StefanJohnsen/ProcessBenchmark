#include "Utility.h"

#include <system_error>

namespace benchmark
{
std::filesystem::path pathFromUtf8(const std::string& value)
{
    return std::filesystem::path(value);
}

std::string pathToUtf8(const std::filesystem::path& value)
{
    return value.generic_string();
}

std::string systemErrorMessage(const uint32_t errorCode)
{
    return std::system_category().message(static_cast<int>(errorCode)) + " (errno " + std::to_string(errorCode) + ")";
}
} // namespace benchmark
