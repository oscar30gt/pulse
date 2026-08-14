cmake_minimum_required(VERSION 3.14)
project(Pulse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# 1. Core Engine Static Library
# -----------------------------------------------------------------------------
# Automatically gather all engine source files
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "core/engine/*.cc"
)
# Exclude test files from engine library
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/tests/.*")

add_library(pulse_engine STATIC
    ${ENGINE_SOURCES}
)

target_include_directories(pulse_engine PUBLIC
    core/engine/include
)

# -----------------------------------------------------------------------------
# 2. Main Application Executable (pulse.exe)
# -----------------------------------------------------------------------------
add_executable(pulse
    core/main.cc
)

target_link_libraries(pulse PRIVATE
    pulse_engine
)

# -----------------------------------------------------------------------------
# 3. Automatic Unit Tests Discovery
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Automatically find all test files matching *test*.cc or *.test.cpp in any tests/ directory
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "core/**/tests/*.cc"
        "core/**/tests/*.cpp"
    )

    if(TEST_SOURCES)
        add_executable(pulse_tests
            ${TEST_SOURCES}
        )

        target_link_libraries(pulse_tests PRIVATE
            pulse_engine
            GTest::gtest_main
        )

        include(GoogleTest)
        gtest_discover_tests(pulse_tests)
    endif()
endif()
