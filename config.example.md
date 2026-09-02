# PROCESS BENCHMARK EXAMPLE - TEST 1 VS TEST 2

> **Example report with fictional measurements.**

## Test System

| Component | Value |
|---|---|
| CPU | Example 12-Core Processor |
| CPU vendor | ExampleVendor |
| Physical cores | 12 |
| Logical processors | 24 |
| Reported CPU clock | 3600 MHz |
| Installed memory | 64.00 GiB |
| Usable physical memory | 63.75 GiB |
| Available physical memory at report time | 48.20 GiB |
| Total page file limit | 73.75 GiB |
| Memory load at report time | 24% |
| Native architecture | x64 |

Memory values are collected with Microsoft's Windows APIs: [GetPhysicallyInstalledSystemMemory](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getphysicallyinstalledsystemmemory) and [GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex).

## Benchmark Overview

This benchmark compares **2 process groups** across **4 input files**. Each configured process is run **3 times** for each group, for a total of **24 planned process runs**. Processes execute sequentially, one at a time, so they do not compete with another tested process for CPU or memory during measurement.

## Engines

| Name | Executable |
|---|---|
| EngineA | `EngineA.exe` |
| EngineB | `EngineB.exe` |
| EngineC | `EngineC.exe` |

## Files

| Index | File | Input size |
|---:|---|---:|
| 0 | `factory.fbx` | 182.40 MiB |
| 1 | `topside.nwd` | 1.34 GiB |
| 2 | `pump.obj` | 48.75 MiB |
| 3 | `building.rvm` | 612.30 MiB |

## Processes - Test 1

| Index | Engine | Command Arguments |
|---:|---|---|
| 0 | EngineA | `factory.fbx factory.obj -async` |
| 1 | EngineA | `topside.nwd topside.obj -async` |
| 2 | EngineA | `pump.obj pump.obj -async` |
| 3 | EngineB | `building.rvm building.obj -async` |

## Processes - Test 2

| Index | Engine | Command Arguments |
|---:|---|---|
| 0 | EngineC | `factory.fbx factory.obj` |
| 1 | EngineC | `topside.nwd topside.obj` |
| 2 | EngineC | `pump.obj pump.obj` |
| 3 | EngineC | `building.rvm building.obj` |

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

Peak RAM is read with Microsoft's [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo). `PeakWorkingSetSize` is the highest resident physical working set reported for the direct process. See [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).

<br>

## File type `.fbx`

<br>

### Per-file results

| File | Extension | Input size | Test 1 run time | Test 2 run time | Run time comparison | Test 1 peak RAM | Test 2 peak RAM | Memory comparison | Status |
|---|---|---:|---:|---:|---|---:|---:|---|---|
| `factory` | `fbx` | 182.40 MiB | 00:20.000 | 00:09.524 | Test 2: 52.4% lower (2.10x speedup) | 700.0 MiB | 770.0 MiB | Test 1: 9.1% less peak RAM | Comparable |

<br>

### Individual runs

| File | Group | Run | Order | Run time | Peak RAM | Output | Exit | Status |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `factory.fbx` | Test 1 | 1 | 1 | 00:20.120 | 699.0 MiB | 96.20 MiB | 0 | OK |
| `factory.fbx` | Test 1 | 2 | 2 | 00:20.000 | 700.0 MiB | 96.20 MiB | 0 | OK |
| `factory.fbx` | Test 1 | 3 | 3 | 00:19.930 | 701.0 MiB | 96.20 MiB | 0 | OK |
| `factory.fbx` | Test 2 | 1 | 4 | 00:09.600 | 768.0 MiB | 96.20 MiB | 0 | OK |
| `factory.fbx` | Test 2 | 2 | 5 | 00:09.524 | 770.0 MiB | 96.20 MiB | 0 | OK |
| `factory.fbx` | Test 2 | 3 | 6 | 00:09.480 | 772.0 MiB | 96.20 MiB | 0 | OK |

<br>

## File type `.nwd`

<br>

### Per-file results

| File | Extension | Input size | Test 1 run time | Test 2 run time | Run time comparison | Test 1 peak RAM | Test 2 peak RAM | Memory comparison | Status |
|---|---|---:|---:|---:|---|---:|---:|---|---|
| `topside` | `nwd` | 1.34 GiB | 00:21.000 | 00:10.000 | Test 2: 52.4% lower (2.10x speedup) | 680.0 MiB | 760.0 MiB | Test 1: 10.5% less peak RAM | Comparable |

<br>

### Individual runs

| File | Group | Run | Order | Run time | Peak RAM | Output | Exit | Status |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `topside.nwd` | Test 1 | 1 | 7 | 00:21.140 | 681.0 MiB | 410.50 MiB | 0 | OK |
| `topside.nwd` | Test 1 | 2 | 8 | 00:21.000 | 680.0 MiB | 410.50 MiB | 0 | OK |
| `topside.nwd` | Test 1 | 3 | 9 | 00:20.910 | 679.0 MiB | 410.50 MiB | 0 | OK |
| `topside.nwd` | Test 2 | 1 | 10 | 00:10.080 | 762.0 MiB | 410.50 MiB | 0 | OK |
| `topside.nwd` | Test 2 | 2 | 11 | 00:10.000 | 760.0 MiB | 410.50 MiB | 0 | OK |
| `topside.nwd` | Test 2 | 3 | 12 | 00:09.940 | 758.0 MiB | 410.50 MiB | 0 | OK |

<br>

## File type `.obj`

<br>

### Per-file results

| File | Extension | Input size | Test 1 run time | Test 2 run time | Run time comparison | Test 1 peak RAM | Test 2 peak RAM | Memory comparison | Status |
|---|---|---:|---:|---:|---|---:|---:|---|---|
| `pump` | `obj` | 48.75 MiB | 00:20.500 | 00:09.800 | Test 2: 52.2% lower (2.09x speedup) | 690.0 MiB | 780.0 MiB | Test 1: 11.5% less peak RAM | Comparable |

<br>

### Individual runs

| File | Group | Run | Order | Run time | Peak RAM | Output | Exit | Status |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `pump.obj` | Test 1 | 1 | 13 | 00:20.620 | 688.0 MiB | 30.40 MiB | 0 | OK |
| `pump.obj` | Test 1 | 2 | 14 | 00:20.500 | 690.0 MiB | 30.40 MiB | 0 | OK |
| `pump.obj` | Test 1 | 3 | 15 | 00:20.410 | 692.0 MiB | 30.40 MiB | 0 | OK |
| `pump.obj` | Test 2 | 1 | 16 | 00:09.880 | 782.0 MiB | 30.40 MiB | 0 | OK |
| `pump.obj` | Test 2 | 2 | 17 | 00:09.800 | 780.0 MiB | 30.40 MiB | 0 | OK |
| `pump.obj` | Test 2 | 3 | 18 | 00:09.740 | 778.0 MiB | 30.40 MiB | 0 | OK |

<br>

## File type `.rvm`

<br>

### Per-file results

| File | Extension | Input size | Test 1 run time | Test 2 run time | Run time comparison | Test 1 peak RAM | Test 2 peak RAM | Memory comparison | Status |
|---|---|---:|---:|---:|---|---:|---:|---|---|
| `building` | `rvm` | 612.30 MiB | 00:21.500 | 00:10.200 | Test 2: 52.6% lower (2.11x speedup) | 720.0 MiB | 800.0 MiB | Test 1: 10.0% less peak RAM | Comparable |

<br>

### Individual runs

| File | Group | Run | Order | Run time | Peak RAM | Output | Exit | Status |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `building.rvm` | Test 1 | 1 | 19 | 00:21.610 | 718.0 MiB | 201.80 MiB | 0 | OK |
| `building.rvm` | Test 1 | 2 | 20 | 00:21.500 | 720.0 MiB | 201.80 MiB | 0 | OK |
| `building.rvm` | Test 1 | 3 | 21 | 00:21.420 | 722.0 MiB | 201.80 MiB | 0 | OK |
| `building.rvm` | Test 2 | 1 | 22 | 00:10.280 | 802.0 MiB | 201.80 MiB | 0 | OK |
| `building.rvm` | Test 2 | 2 | 23 | 00:10.200 | 800.0 MiB | 201.80 MiB | 0 | OK |
| `building.rvm` | Test 2 | 3 | 24 | 00:10.140 | 798.0 MiB | 201.80 MiB | 0 | OK |

<br>

# Overall Performance

Comparable files: **4/4**

| Metric | Test 1 | Test 2 | Comparison |
|---|---:|---:|---|
| Total run time | 01:23.000 | 00:39.524 | Test 2: 52.4% lower run time (2.10x speedup) |
| Highest median peak RAM | 720.0 MiB | 800.0 MiB | Test 1: 10.0% less peak RAM |

<p><strong>TEST 2 IS 2.10x FASTER</strong></p>

<p><strong>TEST 1 USES 10.0% LESS PEAK RAM</strong></p>

<br>

## Visual Comparison

Lower is better. Bars are normalized independently for each metric.

| Metric | Group | Usage | Value | Comp. | BEST |
|---|---|---|---:|---:|:---:|
| Total run time | Test 1 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608; | 01:23.000 | - |  |
| Total run time | Test 2 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617;&#9617; | 00:39.524 | 2.10x | **x** |
| Highest median peak RAM | Test 1 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9617;&#9617; | 720.0 MiB | 10.0% less | **x** |
| Highest median peak RAM | Test 2 | &#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608;&#9608; | 800.0 MiB | - |  |

<br>

# Configuration Used

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
1     | C:\temp\CadFiles\topside.nwd
2     | C:\temp\CadFiles\pump.obj
3     | C:\temp\CadFiles\building.rvm

PROCESSES - TEST 1

Index | Engine  | Command Arguments
------|---------|--------------------------------------------------------------------------
0     | EngineA | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test1\{file.name}.obj" -async
1     | EngineA | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test1\{file.name}.obj" -async
2     | EngineA | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test1\{file.name}.obj" -async
3     | EngineB | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test1\{file.name}.obj" -async

PROCESSES - TEST 2

Index | Engine  | Command Arguments
------|---------|---------------------------------------------------------------------------------
0     | EngineC | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test2\{file.name}.obj"
1     | EngineC | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test2\{file.name}.obj"
2     | EngineC | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test2\{file.name}.obj"
3     | EngineC | "{file.dir}{file.name}{file.ext}" "C:\temp\Output\Test2\{file.name}.obj"

FILE PLACEHOLDERS

Placeholders are inspired by Visual Basic .NET.
Example file: C:/temp/test.dat

Placeholder | Description                           | Value
------------|---------------------------------------|----------
{file.dir}  | Directory, including trailing slash   | C:/temp/
{file.name} | File name without the final extension | test
{file.ext}  | Final extension, including the dot    | .dat
```
