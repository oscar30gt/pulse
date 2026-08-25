cmake_minimum_required(VERSION 3.14)
project(Pulse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# 1. Core Static Library
# -----------------------------------------------------------------------------
# Automatically gather all core source files
file(GLOB_RECURSE PULSE_SOURCES CONFIGURE_DEPENDS
    "core/engine/*.cc"
    "core/parser/*.cc"
    "core/debugger/*.cc"
)
# Exclude test files from core library
list(FILTER PULSE_SOURCES EXCLUDE REGEX ".*/tests/.*")

add_library(pulse_app STATIC
    ${PULSE_SOURCES}
)

target_include_directories(pulse_app PUBLIC
    core/engine/include
    core/engine/lib
    core/parser/include
    core/parser/lib
    core/debugger/include
    core/debugger/lib
)

# -----------------------------------------------------------------------------
# 2. Main Application Executable (pulse.exe)
# -----------------------------------------------------------------------------
add_executable(pulse
    core/main.cc
)

target_link_libraries(pulse PRIVATE
    pulse_app
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
    )

    if(TEST_SOURCES)
        add_executable(pulse_tests
            ${TEST_SOURCES}
        )

        target_link_libraries(pulse_tests PRIVATE
            pulse_app
            GTest::gtest_main
        )

        # Silence [[nodiscard]] warnings only for unit tests
        if(MSVC)
            target_compile_options(pulse_tests PRIVATE /wd4834)
        else()
            target_compile_options(pulse_tests PRIVATE -Wno-unused-result)
        endif()

        include(GoogleTest)
        gtest_discover_tests(pulse_tests)
    endif()
endif()
