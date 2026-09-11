cmake_minimum_required(VERSION 3.20)
project(ConditionalRange CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC / Clang 向けの基本コンパイルフラグ
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only target
add_library(ConditionalRange INTERFACE)
target_include_directories(ConditionalRange INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Tests
enable_testing()

# メインテスト
add_executable(unit_tests tests/test_ConditionalRange.cpp)
target_link_libraries(unit_tests PRIVATE ConditionalRange)
add_test(NAME unit_tests COMMAND unit_tests)

# 追加するテスト (test2)
add_executable(corner_case_tests tests/test2_ConditionalRange.cpp)
target_link_libraries(corner_case_tests PRIVATE ConditionalRange)
add_test(NAME corner_case_tests COMMAND corner_case_tests)

add_executable(planning_tests tests/test_planning.cpp)
target_link_libraries(planning_tests PRIVATE ConditionalRange)
add_test(NAME planning_tests COMMAND planning_tests)

# ベンチマーク用のターゲット追加
add_executable(benchmark_ConditionalRange benchmarks/benchmark_ConditionalRange.cpp)
target_link_libraries(benchmark_ConditionalRange PRIVATE ConditionalRange)

add_executable(benchmark_planning benchmarks/benchmark_planning.cpp)
target_link_libraries(benchmark_planning PRIVATE ConditionalRange)
