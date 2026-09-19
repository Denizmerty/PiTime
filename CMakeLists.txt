cmake_minimum_required(VERSION 3.21)
project(PiTime VERSION 2.0.0 LANGUAGES CXX)

include(CTest)
include(CheckIPOSupported)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(GMP REQUIRED)
find_package(Threads REQUIRED)

option(PITIME_BUILD_BENCHMARKS "Enable the optional benchmark target" ON)
option(PITIME_ENABLE_IPO "Enable interprocedural optimization in Release builds" ON)
option(PITIME_NATIVE "Tune for this CPU (binaries may not run on other CPUs)" OFF)

add_library(pitime_build_options INTERFACE)
target_compile_features(pitime_build_options INTERFACE cxx_std_17)
if(MSVC)
    target_compile_options(pitime_build_options INTERFACE /W4 /permissive-)
else()
    target_compile_options(pitime_build_options INTERFACE -Wall -Wextra -Wpedantic)
    if(PITIME_NATIVE)
        target_compile_options(pitime_build_options INTERFACE $<$<CONFIG:Release>:-march=native>)
    endif()
endif()
if(PITIME_ENABLE_IPO)
    check_ipo_supported(RESULT pitime_ipo_supported OUTPUT pitime_ipo_error)
    if(pitime_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(STATUS "IPO unavailable: ${pitime_ipo_error}")
    endif()
endif()

add_library(pitime_core src/pi.cpp)
target_include_directories(pitime_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pitime_core PUBLIC pitime_build_options PRIVATE GMP::GMP Threads::Threads)

add_executable(pitime src/main.cpp)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT pitime)
target_link_libraries(pitime PRIVATE pitime_core)
target_compile_definitions(pitime PRIVATE PITIME_VERSION="${PROJECT_VERSION}")

if(BUILD_TESTING)
    add_executable(pitime_tests EXCLUDE_FROM_ALL tests/test_pi.cpp)
    set_target_properties(pitime_tests PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_link_libraries(pitime_tests PRIVATE pitime_core GMP::GMP Threads::Threads)
    add_test(NAME pi_correctness COMMAND pitime_tests)
    add_test(
        NAME baseline_integrity
        COMMAND "${CMAKE_COMMAND}"
        -DSOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_baselines.cmake"
    )
    add_test(
        NAME cli_contract
        COMMAND "${CMAKE_COMMAND}"
        -DPITIME_EXECUTABLE=$<TARGET_FILE:pitime>
        -DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/tests/cli
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake"
    )
endif()

if(PITIME_BUILD_BENCHMARKS)
    add_executable(
        pitime_benchmark EXCLUDE_FROM_ALL
        benchmarks/benchmark.cpp
        benchmarks/upstream_wrapper.cpp
        benchmarks/local_wrapper.cpp
    )
    set_target_properties(pitime_benchmark PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_link_libraries(pitime_benchmark PRIVATE pitime_core GMP::GMP)
endif()

install(TARGETS pitime RUNTIME DESTINATION bin)
