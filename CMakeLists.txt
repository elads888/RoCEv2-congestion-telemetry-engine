# =============================================================================
# CMakeLists.txt — Demo Binary Build
# =============================================================================
# Builds integrated_demo from the following source files:
#   metrics.cpp     — shared global state (g_metrics, g_rate_pps, g_running)
#   bridge.cpp      — zero-copy kernel-bypass RAII wrappers (DeviceHandle, MappedBuffer)
#   simulator.cpp   — DMA engine replacement (SimulatorThread)
#   telemetry.cpp   — congestion telemetry engine (TelemetryThread)
#   http_server.cpp — raw-socket HTTP/1.1 server
#   ui_html.cpp     — embedded UI HTML string
#   main.cpp        — signal handler and program entry point
#
# Build:
#   mkdir -p build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   make -j$(nproc)
#
# The run_demo.sh script handles this automatically.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(rocev2_demo CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release: maximum optimisation, keep frame pointers for profiling
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -fno-omit-frame-pointer")

# Debug: sanitisers catch races, leaks, and undefined behaviour
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address,thread,undefined")

add_executable(integrated_demo
    src/metrics.cpp
    src/bridge.cpp
    src/simulator.cpp
    src/telemetry.cpp
    src/http_server.cpp
    src/ui_html.cpp
    src/main.cpp
)

# All headers live in src/
target_include_directories(integrated_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Threads REQUIRED)
target_link_libraries(integrated_demo PRIVATE Threads::Threads)

target_compile_options(integrated_demo PRIVATE -Wall -Wextra)

add_subdirectory(tests/unit)
