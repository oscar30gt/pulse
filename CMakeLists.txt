cmake_minimum_required(VERSION 3.14)
project(Pulse VERSION 2.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

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
add_library(Pulse::pulse_app ALIAS pulse_app)

# Consumers of the installed library compile its headers too, so they need C++20 as well
target_compile_features(pulse_app PUBLIC cxx_std_20)

# Header directories are discovered by name instead of being listed one by one: any folder called
# include/ or impl/ is public, any folder called lib/ is private to the library. This covers both the
# module layout (src/<module>/include) and the compiler's deeper feature layout
# (src/compiler/<feature>/{include,lib}), so a new module or compiler feature only has to exist on
# disk — re-run `cmake -B build` to pick it up. Includes therefore stay bare (#include "ast.h") no
# matter which module or feature owns the header. The public headers are installed flat into
# include/pulse/ (their names are unique), so the same bare includes work against an installation.
set(PULSE_HEADER_DIR_GLOBS "")
set(PULSE_PUBLIC_HEADERS "")
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
        target_include_directories(pulse_app PUBLIC "$<BUILD_INTERFACE:${HEADER_DIR}>")
        file(GLOB HEADERS_OF_DIR CONFIGURE_DEPENDS "${HEADER_DIR}/*.h")
        list(APPEND PULSE_PUBLIC_HEADERS ${HEADERS_OF_DIR})
    endif()
endforeach()
target_include_directories(pulse_app PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pulse>")

# -----------------------------------------------------------------------------
# 2. Main Application Executable (pulse.exe)
# -----------------------------------------------------------------------------
add_executable(pulse
    src/main.cc
)

target_link_libraries(pulse PRIVATE
    pulse_app
)

# --version prints the project version
target_compile_definitions(pulse PRIVATE PULSE_VERSION="${PROJECT_VERSION}")

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
    # GoogleTest is only used to build pulse_tests: keep it out of `cmake --install`
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
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

# -----------------------------------------------------------------------------
# 4. Installation
# -----------------------------------------------------------------------------
# `cmake --install build [--prefix <dir>]` installs the pulse executable to bin/, the pulse_app library
# to lib/, the public headers to include/pulse/ and a CMake package to lib/cmake/Pulse/, so other
# projects can use `find_package(Pulse)` and link `Pulse::pulse_app`. The unit tests are not installed.
install(TARGETS pulse pulse_app EXPORT PulseTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${PULSE_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pulse)

set(PULSE_PACKAGE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Pulse)
install(EXPORT PulseTargets NAMESPACE Pulse:: DESTINATION ${PULSE_PACKAGE_DIR})

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/PulseConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/PulseConfig.cmake"
    INSTALL_DESTINATION ${PULSE_PACKAGE_DIR}
)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/PulseConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PulseConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PulseConfigVersion.cmake"
    DESTINATION ${PULSE_PACKAGE_DIR}
)

# Opt-in: on Windows, add the installed bin directory to the user PATH; elsewhere, print the line to add
# to the shell profile (cmake/PulseAddToPath.cmake). Off by default, as installs normally leave PATH alone.
option(PULSE_ADD_TO_PATH "Add the installed bin directory to the user PATH when installing" OFF)
if(PULSE_ADD_TO_PATH)
    install(CODE "
        set(PULSE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")
        set(PULSE_ADD_TO_PATH_PS1 \"${CMAKE_CURRENT_SOURCE_DIR}/cmake/PulseAddToPath.ps1\")
    ")
    install(SCRIPT cmake/PulseAddToPath.cmake)
endif()