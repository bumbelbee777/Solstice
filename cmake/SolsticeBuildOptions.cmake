# Shared configure-time paths and search hints (included from top-level CMakeLists.txt after project()).
#
# All SOLSTICE_BUILD_* cache PATHs: relative entries resolve against CMAKE_BINARY_DIR (build root).

# --- Runnables (games, SolsticeEngine DLL, tests default) ---
set(SOLSTICE_BUILD_BINDIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Output directory for executables and shared libraries (default: CMAKE_BINARY_DIR/bin). Relative paths resolve against CMAKE_BINARY_DIR.")

set(_solstice_exe_out "${SOLSTICE_BUILD_BINDIR}")
if(NOT IS_ABSOLUTE "${_solstice_exe_out}")
    cmake_path(ABSOLUTE_PATH _solstice_exe_out BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(SOLSTICE_EXECUTABLE_OUTPUT_DIR "${_solstice_exe_out}")

# --- Static / import libs (archives produced by the compiler toolchain) ---
set(SOLSTICE_BUILD_LIBDIR "${CMAKE_BINARY_DIR}/lib" CACHE PATH
    "Output directory for static libraries and import libs (default: CMAKE_BINARY_DIR/lib). Relative paths resolve against CMAKE_BINARY_DIR.")

set(_solstice_lib_out "${SOLSTICE_BUILD_LIBDIR}")
if(NOT IS_ABSOLUTE "${_solstice_lib_out}")
    cmake_path(ABSOLUTE_PATH _solstice_lib_out BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(SOLSTICE_ARCHIVE_OUTPUT_DIR "${_solstice_lib_out}")

# --- Staging (fetched CLI utilities, generated test media) ---
set(SOLSTICE_BUILD_STAGINGDIR "${CMAKE_BINARY_DIR}/_staging" CACHE PATH
    "Directory for build-tree scratch outputs (e.g. fetched ffmpeg CLI, generated test fixtures). Relative paths resolve against CMAKE_BINARY_DIR.")

set(_solstice_staging_out "${SOLSTICE_BUILD_STAGINGDIR}")
if(NOT IS_ABSOLUTE "${_solstice_staging_out}")
    cmake_path(ABSOLUTE_PATH _solstice_staging_out BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(SOLSTICE_STAGING_DIR "${_solstice_staging_out}")

# --- Post-build utility zips (package_executables.py) ---
set(SOLSTICE_BUILD_PACKAGEDIR "${CMAKE_BINARY_DIR}/packages" CACHE PATH
    "Output directory for utility packaging zips (default: CMAKE_BINARY_DIR/packages). Relative paths resolve against CMAKE_BINARY_DIR.")

set(_solstice_pkg_out "${SOLSTICE_BUILD_PACKAGEDIR}")
if(NOT IS_ABSOLUTE "${_solstice_pkg_out}")
    cmake_path(ABSOLUTE_PATH _solstice_pkg_out BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(SOLSTICE_PACKAGES_DIR "${_solstice_pkg_out}")

# Host tools (shaderc, etc.) and other targets inherit these unless they set per-target properties.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SOLSTICE_ARCHIVE_OUTPUT_DIR}")

# Tests default next to other runnables; override for isolated test output.
set(SOLSTICE_TEST_BINDIR "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}" CACHE PATH
    "Directory for CTest executables (default: same as SOLSTICE_BUILD_BINDIR).")
set(_solstice_test_out "${SOLSTICE_TEST_BINDIR}")
if(NOT IS_ABSOLUTE "${_solstice_test_out}")
    cmake_path(ABSOLUTE_PATH _solstice_test_out BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(SOLSTICE_TEST_BIN_DIR "${_solstice_test_out}")

# Optional dependency roots (vcpkg, custom FFmpeg, protobuf, etc.)
set(SOLSTICE_EXTRA_CMAKE_PREFIX_PATH "" CACHE STRING
    "Extra entries for CMAKE_PREFIX_PATH (semicolon-separated on all platforms).")
if(SOLSTICE_EXTRA_CMAKE_PREFIX_PATH)
    list(PREPEND CMAKE_PREFIX_PATH ${SOLSTICE_EXTRA_CMAKE_PREFIX_PATH})
endif()
