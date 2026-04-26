# Post-build .zip of utility targets when SOLSTICE_PACKAGE_UTILITIES_POSTBUILD is ON.
# See docs/Packaging.md. Requires find_package(Python3) in the top-level project first.

function(solstice_utility_postbuild_package _target _fst_relpath)
    if(NOT SOLSTICE_PACKAGE_UTILITIES_POSTBUILD)
        return()
    endif()
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "solstice_utility_postbuild_package: no target \"${_target}\"")
    endif()
    set(_fst "${CMAKE_SOURCE_DIR}/${_fst_relpath}")
    if(NOT EXISTS "${_fst}")
        message(FATAL_ERROR "solstice_utility_postbuild_package: FST not found: ${_fst}")
    endif()
    if(NOT Python3_EXECUTABLE)
        message(FATAL_ERROR "solstice_utility_postbuild_package: Python3_EXECUTABLE is not set")
    endif()
    set(_script "${CMAKE_SOURCE_DIR}/tools/package_executables.py")
    set(_out "${CMAKE_BINARY_DIR}/packages/${_target}-$<CONFIG>.zip")
    add_custom_command(
        TARGET ${_target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/packages"
        COMMAND
            ${Python3_EXECUTABLE}
            "${_script}"
            --fst
            "${_fst}"
            --build-dir
            "${CMAKE_BINARY_DIR}"
            --format
            zip
            --out
            "${_out}"
            --quiet
        COMMENT "Package ${_target} (post-build zip)"
    )
endfunction()
