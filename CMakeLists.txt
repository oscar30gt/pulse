cmake_minimum_required(VERSION 3.14)
project(Pulse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force all binary outputs to build/bin across all OS and generators
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# -----------------------------------------------------------------------------
# 1. Core Static Library
# -----------------------------------------------------------------------------
# Automatically gather all core source files
file(GLOB_RECURSE PULSE_SOURCES CONFIGURE_DEPENDS
    "src/shared/*.cc"
    "src/engine/*.cc"
    "src/compiler/*.cc"
    "src/debugger/*.cc"
)
# Exclude test files from core library
list(FILTER PULSE_SOURCES EXCLUDE REGEX ".*/tests/.*")

add_library(pulse_app STATIC
    ${PULSE_SOURCES}
)

# Header directories are discovered by name instead of being listed one by one: any folder called
# include/ or impl/ is public, any folder called lib/ is private to the library. This covers both the
# module layout (src/<module>/include) and the compiler's deeper feature layout
# (src/compiler/<feature>/{include,lib}), so a new module or compiler feature only has to exist on
# disk — re-run `cmake -B build` to pick it up. Includes therefore stay bare (#include "ast.h") no
# matter which module or feature owns the header.
set(PULSE_HEADER_DIR_GLOBS "")
foreach(DEPTH "src/*" "src/*/*" "src/*/*/*")
    list(APPEND PULSE_HEADER_DIR_GLOBS "${DEPTH}/include" "${DEPTH}/impl" "${DEPTH}/lib")
endforeach()
file(GLOB PULSE_HEADER_DIRS LIST_DIRECTORIES true CONFIGURE_DEPENDS ${PULSE_HEADER_DIR_GLOBS})

foreach(HEADER_DIR ${PULSE_HEADER_DIRS})
    if(NOT IS_DIRECTORY "${HEADER_DIR}")
        continue()
    endif()
    get_filename_component(HEADER_DIR_NAME "${HEADER_DIR}" NAME)
    if(HEADER_DIR_NAME STREQUAL "lib")
        target_include_directories(pulse_app PRIVATE "${HEADER_DIR}")
    else()
        target_include_directories(pulse_app PUBLIC "${HEADER_DIR}")
    endif()
endforeach()

# -----------------------------------------------------------------------------
# 2. Main Application Executable (pulse.exe)
# -----------------------------------------------------------------------------
add_executable(pulse
    src/main.cc
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

    # Every .cc file inside a tests/ directory of any module or compiler feature is a test source
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "src/*.cc")
    list(FILTER TEST_SOURCES INCLUDE REGEX ".*/tests/[^/]*[.]cc$")

    if(TEST_SOURCES)
        add_executable(pulse_tests
            ${TEST_SOURCES}
        )

        # Test utilities shared by all compiler features (src/compiler/tests/test_helpers.h)
        target_include_directories(pulse_tests PRIVATE src/compiler/tests)

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