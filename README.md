# ProcessBenchmark

`ProcessBenchmark` compares exactly two process groups. It runs one Windows process at a time, measures run time
and peak RAM, and writes a Markdown report beside the text configuration (`benchmark.txt` becomes `benchmark.md`).

## Build and usage

```text
MSBuild.exe ProcessBenchmark.sln /m:1 /nr:false /p:Configuration=Release /p:Platform=x64 /v:minimal
ProcessBenchmark.exe [--time-only|--ram-only] "C:\full\path\benchmark.txt"
```

Run `ProcessBenchmark.exe -help` for the authoritative option list. Benchmark options accept either one or two
leading dashes. Unknown options, duplicate measurement options, incompatible measurement modes, and combinations
with `-help` or `-version` are rejected before the configuration is loaded.

All executable, input, output, and configuration paths must be absolute. The report uses the same directory and base
name as the configuration, replacing `.txt` with `.md` (`C:\Benchmark\config.txt` becomes
`C:\Benchmark\config.md`). See [`config.example.txt`](config.example.txt) and the fictional generated-style
[`config.example.md`](config.example.md).

## Configuration contract

- The first non-empty line is the report title; `RUNS` must be between 1 and 100.
- `ENGINES` maps unique names to existing executables.
- `FILES` contains ordered inputs with consecutive indices starting at zero.
- Exactly two `PROCESSES` groups are required, each with one row per input in identical index order.
- A process names an engine and supplies complete Visual Studio-style Command Arguments.
- `{file.dir}`, `{file.name}`, and `{file.ext}` expand from the matching input.
- The first absolute file argument must be that input; the second is the expected output.

The exact expected output is removed before every run. Success requires process exit code zero and a non-empty output.

## Report and measurements

The report includes CPU, core and logical processor counts, clock, installed/usable/available memory, page-file limit,
memory load, architecture, engines, files, and expanded processes. Normal report tables show file arguments and
executables only as filenames with extensions. The final `Configuration Used` appendix intentionally preserves the
complete ASCII input configuration, including full paths and placeholders, so the benchmark can be reproduced.

Run time uses `std::chrono::steady_clock`. Peak RAM is the direct process `PeakWorkingSetSize`; child-process memory
and GPU/VRAM are excluded. Per-file comparisons use the median. Use `--time-only` or `--ram-only` for one metric.

The memory implementation follows Microsoft's Windows API documentation:

- [`GetProcessMemoryInfo`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo)
  reads process memory statistics.
- [`PROCESS_MEMORY_COUNTERS_EX.PeakWorkingSetSize`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex)
  supplies the converter's peak physical working set.
- [`GlobalMemoryStatusEx`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)
  supplies usable and available physical memory, page-file limit, and memory load.
- [`GetPhysicallyInstalledSystemMemory`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getphysicallyinstalledsystemmemory)
  supplies physically installed RAM from SMBIOS.

Exit codes: `0` success, `1` usage/configuration/preflight/report error, `2` completed with failed runs or no comparison.
