#define _DEFAULT_SOURCE

#include "ProcessRunner.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

#include "Utility.h"

namespace benchmark
{
namespace
{
inline constexpr size_t JobDrainMaxAttempts = 100;
inline constexpr int JobDrainPollIntervalMilliseconds = 10;

class UniqueFd final
{
  public:
    UniqueFd() = default;
    explicit UniqueFd(const int value) noexcept : value_(value)
    {
    }

    ~UniqueFd() noexcept
    {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    [[nodiscard]] int get() const noexcept
    {
        return value_;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return value_ >= 0;
    }

    void reset(const int value = -1) noexcept
    {
        if (valid())
            close(value_);
        value_ = value;
    }

  private:
    int value_ = -1;
};

std::string quotePosixArgument(const std::string& value)
{
    std::string result = "'";
    for (const char character : value)
    {
        if (character == '\'')
            result += "'\\''";
        else
            result.push_back(character);
    }
    result.push_back('\'');
    return result;
}

void appendError(RunResult& result, const std::string& error)
{
    if (!result.error.empty())
        result.error += " ";
    result.error += error;
}

bool processGroupHasMembers(const pid_t group)
{
    return killpg(group, 0) == 0;
}

// Descendants that outlive the direct child normally stay in its process group
// (they only leave it if they explicitly call setsid/setpgid themselves), so
// polling and then signalling the whole group approximates the containment a
// Windows Job Object gives us.
bool waitForProcessGroupToBecomeEmpty(const pid_t group)
{
    for (size_t attempt = 0; attempt < JobDrainMaxAttempts; ++attempt)
    {
        if (!processGroupHasMembers(group))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(JobDrainPollIntervalMilliseconds));
    }
    return false;
}
} // namespace

RunResult runProcess(const std::filesystem::path& executable, const std::string& commandArguments,
                     const size_t runNumber, const BenchmarkOptions& options)
{
    RunResult result;
    result.runNumber = runNumber;

    UniqueFd nullFd(open("/dev/null", O_RDWR));
    if (!nullFd.valid())
    {
        result.error = "Could not open /dev/null: " + systemErrorMessage(static_cast<uint32_t>(errno));
        return result;
    }

    const auto script = "exec " + quotePosixArgument(pathToUtf8(executable)) +
                        (commandArguments.empty() ? "" : " " + commandArguments);

    const pid_t child = fork();
    if (child < 0)
    {
        result.error = "Could not start process: " + systemErrorMessage(static_cast<uint32_t>(errno));
        return result;
    }

    if (child == 0)
    {
        setpgid(0, 0);
        dup2(nullFd.get(), STDIN_FILENO);
        dup2(nullFd.get(), STDOUT_FILENO);
        dup2(nullFd.get(), STDERR_FILENO);
        raise(SIGSTOP);
        execl("/bin/sh", "sh", "-c", script.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    nullFd.reset();

    int stopStatus = 0;
    if (waitpid(child, &stopStatus, WUNTRACED) < 0 || !WIFSTOPPED(stopStatus))
    {
        result.error = "Could not start process.";
        if (WIFEXITED(stopStatus))
            appendError(result, "Process exited with code " + std::to_string(WEXITSTATUS(stopStatus)) + ".");
        waitForProcessGroupToBecomeEmpty(child);
        return result;
    }

    const auto started = std::chrono::steady_clock::now();
    kill(child, SIGCONT);

    int status = 0;
    struct rusage usage{};
    const pid_t waited = wait4(child, &status, 0, &usage);
    const auto stopped = std::chrono::steady_clock::now();

    if (waited < 0)
    {
        const auto error = errno;
        killpg(child, SIGKILL);
        appendError(result, "Could not wait for process: " + systemErrorMessage(static_cast<uint32_t>(error)));
    }
    else
    {
        if (WIFEXITED(status))
            result.exitCode = static_cast<uint32_t>(WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            appendError(result, "Process was terminated by signal " + std::to_string(WTERMSIG(status)) + ".");
    }

    if (options.measureTime)
        result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(stopped - started).count();
    if (options.measureMemory)
    {
        if (waited < 0)
            appendError(result, "Could not measure process memory usage.");
        else
            result.peakWorkingSetBytes = static_cast<uint64_t>(usage.ru_maxrss) * 1024ULL;
    }

    if (!waitForProcessGroupToBecomeEmpty(child))
    {
        killpg(child, SIGKILL);
        appendError(result, "Process left child processes running or job accounting failed.");
    }

    if (result.exitCode.has_value() && result.exitCode.value() != 0)
        appendError(result, "Process exited with code " + std::to_string(result.exitCode.value()) + ".");

    result.success = result.error.empty() && result.exitCode.has_value() && result.exitCode.value() == 0;
    return result;
}
} // namespace benchmark
