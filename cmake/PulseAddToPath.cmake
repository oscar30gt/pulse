# Run by `cmake --install` when Pulse is configured with -DPULSE_ADD_TO_PATH=ON (see CMakeLists.txt, which
# sets PULSE_INSTALL_BINDIR and PULSE_ADD_TO_PATH_PS1 before including this script).
# On Windows, the installed bin directory is added to the user PATH, unless it is already there. On other
# platforms, the line to add to the shell profile is printed instead: editing shell rc files is fragile,
# and under sudo it would edit root's.

# A staged install (DESTDIR, as packagers do) does not install to the final prefix yet
if(NOT "$ENV{DESTDIR}" STREQUAL "")
    message(STATUS "DESTDIR is set: not adding Pulse to PATH")
    return()
endif()

if(IS_ABSOLUTE "${PULSE_INSTALL_BINDIR}")
    set(PULSE_BIN_DIR "${PULSE_INSTALL_BINDIR}")
else()
    set(PULSE_BIN_DIR "${CMAKE_INSTALL_PREFIX}/${PULSE_INSTALL_BINDIR}")
endif()
get_filename_component(PULSE_BIN_DIR "${PULSE_BIN_DIR}" ABSOLUTE)

if(CMAKE_HOST_WIN32)
    file(TO_NATIVE_PATH "${PULSE_BIN_DIR}" PULSE_BIN_DIR_NATIVE)
    execute_process(
        COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
                -File "${PULSE_ADD_TO_PATH_PS1}" -Directory "${PULSE_BIN_DIR_NATIVE}"
        RESULT_VARIABLE PULSE_PATH_RESULT
        OUTPUT_VARIABLE PULSE_PATH_OUTPUT
        ERROR_VARIABLE PULSE_PATH_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT PULSE_PATH_RESULT EQUAL 0)
        message(WARNING "Could not add ${PULSE_BIN_DIR_NATIVE} to the user PATH: ${PULSE_PATH_ERROR}")
    else()
        message(STATUS "${PULSE_PATH_OUTPUT}")
    endif()
else()
    string(REPLACE ":" ";" PULSE_CURRENT_PATH "$ENV{PATH}")
    if("${PULSE_BIN_DIR}" IN_LIST PULSE_CURRENT_PATH)
        message(STATUS "${PULSE_BIN_DIR} is already on PATH")
    else()
        message(STATUS "To run pulse from anywhere, add this line to your shell profile (~/.profile, ~/.bashrc or ~/.zshrc):\n"
                       "    export PATH=\"${PULSE_BIN_DIR}:\$PATH\"")
    endif()
endif()
