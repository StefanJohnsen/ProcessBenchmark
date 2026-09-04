cmake_minimum_required(VERSION 3.16)
project(ProcessBenchmark CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ProcessBenchmark
    Benchmark.cpp
    CmdArgs.cpp
    Config.cpp
    Report.cpp
    Statistics.cpp
    Utility.cpp
)

target_include_directories(ProcessBenchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_sources(ProcessBenchmark PRIVATE
        windows/Hardware.cpp
        windows/main.cpp
        windows/ProcessRunner.cpp
        windows/Report.cpp
        windows/Utility.cpp
    )
    target_link_libraries(ProcessBenchmark PRIVATE Psapi)
else()
    target_sources(ProcessBenchmark PRIVATE
        linux/Hardware.cpp
        linux/main.cpp
        linux/ProcessRunner.cpp
        linux/Report.cpp
        linux/Utility.cpp
    )
endif()

target_compile_features(ProcessBenchmark PRIVATE cxx_std_20)
