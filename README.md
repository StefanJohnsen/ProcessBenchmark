# ProcessBenchmark

`ProcessBenchmark` compares exactly two process groups. It runs one Windows process at a time, measures run time
and RAM usage, and writes a Markdown report. An example report is shown at the end of this README.

## Build

Open `ProcessBenchmark.sln` in Visual Studio 2022 and build the `Release | x64` configuration.

<br>
<br>

# Configuration

Copy the ASCII [`config.example.txt`](config.example.txt) file, give the copy a descriptive name for your test, and
adapt it to the processes you want to benchmark. For example, rename it to `testProcess.txt`. The configuration
tells ProcessBenchmark what to run:

1. Write the report title on the first line.
2. Set `RUNS` to the number of repetitions, from 1 to 100. Using 3 to 5 runs is recommended for most benchmarks.
   Repeated runs reduce the influence of a cold first start, system activity and temporary timing variations. The
   report uses the median result, so one unusually slow or fast run has less influence on the comparison.
3. Under `ENGINES`, add as many process engines as needed. Give each executable a short name and its absolute path.
4. Under `FILES`, add as many test files as needed, using consecutive indices starting at `0`.
5. Add exactly two `PROCESSES` groups. Give every input file one process row in each group, using the same indices.

Each process row contains an index, an engine name and `Command Arguments`. Its index connects the row to the `FILES`
entry with the same index. `Command Arguments` is free text and may contain any arguments, options, flags or paths
accepted by the selected process. It may also be empty. File placeholders are optional. ProcessBenchmark replaces
placeholders when present, then passes the complete expanded string directly to the executable without interpreting
or rearranging its contents. Use double quotes when an individual argument contains spaces.

The matching `FILES` entry is the file identity used for measurements and report comparisons. It does not impose a
particular position or role on the process's command arguments.

ProcessBenchmark does not identify, create, validate or delete output files. A process may have no output argument at
all. If your process requires an output path not to exist, remove old output yourself before starting the benchmark.
A run succeeds when the process starts correctly, enabled measurements succeed and the process returns exit code `0`.

### Example configuration

```text
PROCESS BENCHMARK EXAMPLE - TEST 1 VS TEST 2

RUNS: 3

ENGINES

Name    | Executable
--------|---------------------------------
EngineA | C:\temp\engines+\EngineA.exe
EngineB | C:\temp\engines+\EngineB.exe
EngineC | C:\temp\engines+\EngineC.exe

FILES

Index | File
------|---------------------------
0     | C:\temp\CadFiles\factory.fbx
1     | C:\temp\CadFiles\platform.fbx
2     | C:\temp\CadFiles\building.rvm

PROCESSES - TEST 1

Index | Engine  | Command Arguments
------|---------|--------------------------------------------------------------------------
0     | EngineA | {file} C:\temp\Output\Test1\{file.name}.obj -async
1     | EngineA | {file} C:\temp\Output\Test1\{file.name}.obj -async
2     | EngineB | {file} C:\temp\Output\Test1\{file.name}.obj -async

PROCESSES - TEST 2

Index | Engine  | Command Arguments
------|---------|---------------------------------------------------------------------------------
0     | EngineC | {file} C:\temp\Output\Test2\{file.name}.obj
1     | EngineC | {file} C:\temp\Output\Test2\{file.name}.obj
2     | EngineC | {file} C:\temp\Output\Test2\{file.name}.obj
```

For illustration, the configuration above produces the same command lines you might place in a batch file:

```bat
C:\temp\engines+\EngineA.exe C:\temp\CadFiles\factory.fbx C:\temp\Output\Test1\factory.obj -async
C:\temp\engines+\EngineC.exe C:\temp\CadFiles\factory.fbx C:\temp\Output\Test2\factory.obj
C:\temp\engines+\EngineA.exe C:\temp\CadFiles\platform.fbx C:\temp\Output\Test1\platform.obj -async
C:\temp\engines+\EngineC.exe C:\temp\CadFiles\platform.fbx C:\temp\Output\Test2\platform.obj
C:\temp\engines+\EngineB.exe C:\temp\CadFiles\building.rvm C:\temp\Output\Test1\building.obj -async
C:\temp\engines+\EngineC.exe C:\temp\CadFiles\building.rvm C:\temp\Output\Test2\building.obj
```

Because `RUNS` is `3`, ProcessBenchmark executes each command line three times. The processes run sequentially in the
order shown: Test 1 followed by Test 2 for each file.

## Report and measurements

The example report below shows the complete generated output, including hardware information, processes, measurements
and comparisons. Regular report tables hide directory paths to make the results easier to read. The final
`Configuration Used` section preserves the original configuration so the benchmark can be reproduced without having
to archive every configuration file separately.

Run time uses `std::chrono::steady_clock`. RAM is the direct process `PeakWorkingSetSize`; child-process memory
and GPU/VRAM are excluded. Per-file comparisons use the median. Use `--time-only` or `--ram-only` for one metric.

The memory implementation follows Microsoft's Windows API documentation:

- [`GetProcessMemoryInfo`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo)
  reads process memory statistics.
- [`PROCESS_MEMORY_COUNTERS_EX.PeakWorkingSetSize`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex)
  supplies the process's highest physical working set.
- [`GlobalMemoryStatusEx`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)
  supplies usable and available physical memory, page-file limit, and memory load.
- [`GetPhysicallyInstalledSystemMemory`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getphysicallyinstalledsystemmemory)
  supplies physically installed RAM from SMBIOS.

Exit codes: `0` success, `1` usage/configuration/preflight/report error, `2` completed with failed runs or no comparison.

<br>

# Example Markdown Report

> **The following example shows the Markdown report (`.md`) generated by ProcessBenchmark. All measurements are
> fictional. The same example is available as [`config.example.md`](config.example.md) in this repository.**

# PROCESS BENCHMARK EXAMPLE - TEST 1 VS TEST 2

### **TEST 2 IS 2.07x FASTER**

### **TEST 1 USES 10.0% LESS RAM**

<br>

## Benchmark Hardware

| Component | Value |
|---|---|
| CPU | Example 12-Core Processor |
| CPU vendor | ExampleVendor |
| Physical cores | 12 |
| Logical processors | 24 |
| Reported CPU clock | 3600 MHz |
| Installed memory | 64 GiB |
| Usable physical memory | 64 GiB |
| Available physical memory at report time | 48 GiB |
| Total page file limit | 74 GiB |
| Memory load at report time | 24% |
| Native architecture | x64 |

Memory values are collected with Microsoft's Windows APIs: [GetPhysicallyInstalledSystemMemory](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getphysicallyinstalledsystemmemory) and [GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex).

## Benchmark Overview

This benchmark compares **2 process groups** across **3 input files**. Each configured process is run **3 times** for each group, for a total of **18 planned process runs**. Processes execute sequentially, one at a time, so they do not compete with another tested process for CPU or memory during measurement.

## Process Engines

| Name | Executable |
|---|---|
| EngineA | `EngineA.exe` |
| EngineB | `EngineB.exe` |
| EngineC | `EngineC.exe` |

## Test Files

| Index | File | Size |
|---:|---|---:|
| 0 | `factory.fbx` | 210.0 MiB |
| 1 | `platform.fbx` | 1.2 GiB |
| 2 | `building.rvm` | 612.0 MiB |

## Processes - Test 1

| Index | Engine | Command Arguments |
|---:|---|---|
| 0 | EngineA | `factory.fbx factory.obj -async` |
| 1 | EngineA | `platform.fbx platform.obj -async` |
| 2 | EngineB | `building.rvm building.obj -async` |

## Processes - Test 2

| Index | Engine | Command Arguments |
|---:|---|---|
| 0 | EngineC | `factory.fbx factory.obj` |
| 1 | EngineC | `platform.fbx platform.obj` |
| 2 | EngineC | `building.rvm building.obj` |

## Test Method

- Conversions run sequentially, one process at a time.
- Per-file results are medians of three runs. A file is compared only when all runs succeed for both groups.
- Type and overall run times are sums of comparable per-file medians.
- Type and overall memory values are the highest comparable per-file medians; memory is not summed.

<br>

### Run Time Measurement

Run time uses a monotonic clock from immediately before the suspended process is resumed until it terminates. Executable initialization and output writing are included.

<br>

### Memory Measurement

RAM columns in result tables show the highest resident physical memory usage. It is read with Microsoft's [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo). `PeakWorkingSetSize` is the highest resident physical working set reported for the direct process. See [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).

<br>

## File type `.fbx`

<br>

### Per-file results

| File name | Ext | Test 1 time | Test 2 time | Time comp. | Test 1 RAM | Test 2 RAM | RAM comp. | Status |
|---|:---:|:---:|:---:|---|---:|---:|---|:---:|
| `factory` | `fbx` | 00:20.000 | **00:09.524** | Test 2: 2.10x faster | **700 MiB** | 770 MiB | Test 1: 9.1% less | OK |
| `platform` | `fbx` | 00:24.300 | **00:12.000** | Test 2: 2.03x faster | **710 MiB** | 790 MiB | Test 1: 10.1% less | OK |

<br>

### Individual runs

| File name | Ext | Group | Run | Time | RAM | Exit | Status | Best |
|---|:---:|:---:|:---:|:---:|---:|:---:|:---:|:---:|
| **`factory`** | **`fbx`** | **Test 1** | **1** | 00:20.120 | **699 MiB** | 0 | OK | &#128994; |
| `factory` | `fbx` | Test 1 | 2 | 00:20.000 | 700 MiB | 0 | OK | |
| `factory` | `fbx` | Test 1 | 3 | 00:19.930 | 701 MiB | 0 | OK | |
| `factory` | `fbx` | Test 2 | 1 | 00:09.600 | 768 MiB | 0 | OK | |
| `factory` | `fbx` | Test 2 | 2 | 00:09.524 | 770 MiB | 0 | OK | |
| **`factory`** | **`fbx`** | **Test 2** | **3** | **00:09.480** | 772 MiB | 0 | OK | &#128994; |
| `platform` | `fbx` | Test 1 | 1 | 00:24.480 | 708 MiB | 0 | OK | |
| `platform` | `fbx` | Test 1 | 2 | 00:24.300 | 710 MiB | 0 | OK | |
| `platform` | `fbx` | Test 1 | 3 | 00:24.160 | 712 MiB | 0 | OK | |
| `platform` | `fbx` | Test 2 | 1 | 00:12.140 | 788 MiB | 0 | OK | |
| `platform` | `fbx` | Test 2 | 2 | 00:12.000 | 790 MiB | 0 | OK | |
| `platform` | `fbx` | Test 2 | 3 | 00:11.920 | 792 MiB | 0 | OK | |

<br>

## File type `.rvm`

<br>

### Per-file results

| File name | Ext | Test 1 time | Test 2 time | Time comp. | Test 1 RAM | Test 2 RAM | RAM comp. | Status |
|---|:---:|:---:|:---:|---|---:|---:|---|:---:|
| `building` | `rvm` | 00:18.600 | **00:08.900** | Test 2: 2.09x faster | **720 MiB** | 800 MiB | Test 1: 10.0% less | OK |

<br>

### Individual runs

| File name | Ext | Group | Run | Time | RAM | Exit | Status | Best |
|---|:---:|:---:|:---:|:---:|---:|:---:|:---:|:---:|
| **`building`** | **`rvm`** | **Test 1** | **1** | 00:18.740 | **718 MiB** | 0 | OK | &#128994; |
| `building` | `rvm` | Test 1 | 2 | 00:18.600 | 720 MiB | 0 | OK | |
| `building` | `rvm` | Test 1 | 3 | 00:18.510 | 722 MiB | 0 | OK | |
| `building` | `rvm` | Test 2 | 1 | 00:09.020 | 802 MiB | 0 | OK | |
| `building` | `rvm` | Test 2 | 2 | 00:08.900 | 800 MiB | 0 | OK | |
| **`building`** | **`rvm`** | **Test 2** | **3** | **00:08.830** | 798 MiB | 0 | OK | &#128994; |

<br>
<br>

# Overall Performance

Comparable files: **3/3**

Lower is better. Bars are normalized independently for each metric.

| Metric | Group | Usage | Value | Comp. | BEST |
|---|---|---|---:|---:|:---:|
| Total time | Test 1 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608; | 01:02.900 | - |  |
| Total time | Test 2 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617; | 00:30.424 | 2.07x | &#128994; |
| Highest median RAM | Test 1 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9617;&#9617; | 720 MiB | 10.0% less | &#128994; |
| Highest median RAM | Test 2 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608; | 800 MiB | - |  |

### **TEST 2 IS 2.07x FASTER**

### **TEST 1 USES 10.0% LESS RAM**
