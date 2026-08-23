cmake_minimum_required(VERSION 3.20)

project(ConditionalRange
    VERSION 0.1.0
    LANGUAGES CXX
)

# ------------------------------------------------------------
# Library
# ------------------------------------------------------------

add_library(ConditionalRange INTERFACE)

target_include_directories(ConditionalRange
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ConditionalRange
    INTERFACE
        cxx_std_17
)

# GCC / Clang warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ConditionalRange
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ------------------------------------------------------------
# Example
# ------------------------------------------------------------

add_executable(conditional_range_example
    examples/basic.cpp
)

target_link_libraries(conditional_range_example
    PRIVATE
        ConditionalRange
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

include(CTest)

if (BUILD_TESTING)

    # --------------------------------------------------------
    # Main tests
    # --------------------------------------------------------

    add_executable(conditional_range_tests
        tests/test_ConditionalRange.cpp
    )

    target_link_libraries(conditional_range_tests
        PRIVATE
            ConditionalRange
    )

    add_test(
        NAME ConditionalRangeTests
        COMMAND conditional_range_tests
    )


    # --------------------------------------------------------
    # Corner-case tests
    # --------------------------------------------------------

    add_executable(conditional_range_tests2
        tests/test2_ConditionalRange.cpp
    )

    target_link_libraries(conditional_range_tests2
        PRIVATE
            ConditionalRange
    )

    add_test(
        NAME ConditionalRangeCornerCaseTests
        COMMAND conditional_range_tests2
    )

endif()