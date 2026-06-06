cmake_minimum_required(VERSION 3.16)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

enable_testing()

add_executable(unit_tests
    test_header_layout.cpp
    test_metrics_ring.cpp
    test_buffer_model.cpp
    test_telemetry_window.cpp
    test_rate_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/simulator.cpp
    ${CMAKE_SOURCE_DIR}/src/telemetry.cpp
    ${CMAKE_SOURCE_DIR}/src/http_server.cpp
    ${CMAKE_SOURCE_DIR}/src/ui_html.cpp
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(unit_tests PRIVATE cxx_std_17)
target_compile_options(unit_tests PRIVATE -Wall -Wextra)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(unit_tests)
