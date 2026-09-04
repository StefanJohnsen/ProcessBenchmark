#include "ProcessRunner.h"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "Utility.h"

namespace benchmark
{
namespace
{
inline constexpr size_t JobDrainMaxAttempts = 100;
inline constexpr DWORD JobDrainPollIntervalMilliseconds = 10;

class UniqueHandle final
{
  public:
    UniqueHandle() = default;
    explicit UniqueHandle(const HANDLE value) noexcept : value_(value)
    {
    }

    ~UniqueHandle() noexcept
    {
        reset();
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release())
    {
    }

    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other)
            reset(other.release());
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept
    {
        return value_;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

    HANDLE release() noexcept
    {
        const auto result = value_;
        value_ = nullptr;
        return result;
    }

    void reset(const HANDLE value = nullptr) noexcept
    {
        if (valid())
            CloseHandle(value_);
        value_ = value;
    }

  private:
    HANDLE value_ = nullptr;
};

class ThreadAttributeList final
{
  public:
    explicit ThreadAttributeList(const std::vector<HANDLE>& inheritedHandles)
    {
        SIZE_T byteCount = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &byteCount);
        if (byteCount == 0)
            throw std::runtime_error("Could not size the process handle attribute list.");

        storage_.resize(byteCount);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(list_, 1, 0, &byteCount))
            throw std::runtime_error("Could not initialize process attributes: " + systemErrorMessage(GetLastError()));

        if (!UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       const_cast<HANDLE*>(inheritedHandles.data()),
                                       inheritedHandles.size() * sizeof(HANDLE), nullptr, nullptr))
        {
            throw std::runtime_error("Could not configure inherited process handles: " +
                                     systemErrorMessage(GetLastError()));
        }
    }

    ~ThreadAttributeList() noexcept
    {
        if (list_ != nullptr)
            DeleteProcThreadAttributeList(list_);
    }

    ThreadAttributeList(const ThreadAttributeList&) = delete;
    ThreadAttributeList& operator=(const ThreadAttributeList&) = delete;

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept
    {
        return list_;
    }

  private:
    std::vector<std::byte> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

static bool samplePeakWorkingSet(const HANDLE process, RunResult& result)
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return false;

    result.peakWorkingSetBytes =
        std::max(result.peakWorkingSetBytes, static_cast<uint64_t>(counters.PeakWorkingSetSize));
    return true;
}

static void appendError(RunResult& result, const std::string& error)
{
    if (!result.error.empty())
        result.error += " ";
    result.error += error;
}

static bool waitForJobToBecomeEmpty(const HANDLE job)
{
    for (size_t attempt = 0; attempt < JobDrainMaxAttempts; ++attempt)
    {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION information{};
        if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &information, sizeof(information),
                                       nullptr))
        {
            return false;
        }
        if (information.ActiveProcesses == 0)
            return true;
        Sleep(JobDrainPollIntervalMilliseconds);
    }

    return false;
}

static std::wstring argumentsToWide(const std::string& value)
{
    if (value.empty())
        return {};

    const auto length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        throw std::runtime_error("Command Arguments is not valid UTF-8.");

    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                            length) != length)
    {
        throw std::runtime_error("Could not convert Command Arguments from UTF-8.");
    }
    return result;
}

static std::wstring buildCommandLine(const std::filesystem::path& executable, const std::string& commandArguments)
{
    auto commandLine = quoteWindowsArgument(executable.wstring());
    if (!commandArguments.empty())
        commandLine += L" " + argumentsToWide(commandArguments);
    return commandLine;
}
} // namespace

RunResult runProcess(const std::filesystem::path& executable, const std::string& commandArguments,
                     const size_t runNumber, const BenchmarkOptions& options)
{
    RunResult result;
    result.runNumber = runNumber;

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    UniqueHandle nullOutput(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!nullOutput.valid())
    {
        result.error = "Could not open the null output device: " + systemErrorMessage(GetLastError());
        return result;
    }

    UniqueHandle nullInput(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!nullInput.valid())
    {
        result.error = "Could not open the null input device: " + systemErrorMessage(GetLastError());
        return result;
    }

    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job.valid())
    {
        result.error = "Could not create a process job: " + systemErrorMessage(GetLastError());
        return result;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
    {
        result.error = "Could not configure the process job: " + systemErrorMessage(GetLastError());
        return result;
    }

    std::vector<HANDLE> inheritedHandles = {nullInput.get(), nullOutput.get()};
    ThreadAttributeList attributes(inheritedHandles);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullInput.get();
    startup.StartupInfo.hStdOutput = nullOutput.get();
    startup.StartupInfo.hStdError = nullOutput.get();
    startup.lpAttributeList = attributes.get();

    auto commandLine = buildCommandLine(executable, commandArguments);
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    PROCESS_INFORMATION processInformation{};
    const auto created =
        CreateProcessW(executable.c_str(), commandBuffer.data(), nullptr, nullptr, TRUE,
                       CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                       executable.parent_path().c_str(), &startup.StartupInfo, &processInformation);
    if (!created)
    {
        result.error = "Could not start process: " + systemErrorMessage(GetLastError());
        return result;
    }

    UniqueHandle process(processInformation.hProcess);
    UniqueHandle thread(processInformation.hThread);
    nullOutput.reset();
    nullInput.reset();

    if (!AssignProcessToJobObject(job.get(), process.get()))
    {
        const auto error = GetLastError();
        TerminateProcess(process.get(), error);
        WaitForSingleObject(process.get(), INFINITE);
        result.error = "Could not assign process to the process job: " + systemErrorMessage(error);
        return result;
    }

    const auto started = std::chrono::steady_clock::now();
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1))
    {
        const auto error = GetLastError();
        TerminateJobObject(job.get(), error);
        WaitForSingleObject(process.get(), INFINITE);
        result.error = "Could not resume process: " + systemErrorMessage(error);
        return result;
    }
    thread.reset();

    bool sampledMemory = false;
    if (options.measureMemory)
        sampledMemory = samplePeakWorkingSet(process.get(), result);
    while (true)
    {
        const auto wait = WaitForSingleObject(process.get(), ProcessPollIntervalMilliseconds);
        if (wait == WAIT_TIMEOUT)
        {
            if (options.measureMemory)
                sampledMemory = samplePeakWorkingSet(process.get(), result) || sampledMemory;
            continue;
        }
        if (wait == WAIT_OBJECT_0)
            break;

        const auto error = GetLastError();
        TerminateJobObject(job.get(), error);
        WaitForSingleObject(process.get(), INFINITE);
        appendError(result, "Could not wait for process: " + systemErrorMessage(error));
        break;
    }

    const auto stopped = std::chrono::steady_clock::now();
    if (options.measureTime)
        result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(stopped - started).count();
    if (options.measureMemory)
        sampledMemory = samplePeakWorkingSet(process.get(), result) || sampledMemory;

    DWORD exitCode = 0;
    if (GetExitCodeProcess(process.get(), &exitCode))
        result.exitCode = exitCode;
    else
        appendError(result, "Could not read process exit code: " + systemErrorMessage(GetLastError()));

    if (!waitForJobToBecomeEmpty(job.get()))
    {
        TerminateJobObject(job.get(), ERROR_PROCESS_ABORTED);
        appendError(result, "Process left child processes running or job accounting failed.");
    }

    if (options.measureMemory && !sampledMemory)
        appendError(result, "Could not measure process memory usage.");

    if (result.exitCode.has_value() && result.exitCode.value() != 0)
        appendError(result, "Process exited with code " + std::to_string(result.exitCode.value()) + ".");

    result.success = result.error.empty() && result.exitCode.has_value() && result.exitCode.value() == 0;
    return result;
}
} // namespace benchmark
