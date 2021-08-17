include(FindPackageHandleStandardArgs)

macro(_Sphinx_find_executable _exe)
    string(TOUPPER "${_exe}" _uc)
    # sphinx-(build|quickstart)-3 x.x.x
    find_program(
        SPHINX_${_uc}_EXECUTABLE
        NAMES "sphinx-${_exe}-3" "sphinx-${_exe}" "sphinx-${_exe}.exe"
    )
    if(SPHINX_${_uc}_EXECUTABLE)
        execute_process(
            COMMAND "${SPHINX_${_uc}_EXECUTABLE}" --version
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _output
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_result EQUAL 0 AND _output MATCHES " ([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(SPHINX_${_uc}_VERSION "${CMAKE_MATCH_1}")
        endif()

        add_executable(Sphinx::${_exe} IMPORTED GLOBAL)
        set_target_properties(Sphinx::${_exe} PROPERTIES
                              IMPORTED_LOCATION "${SPHINX_${_uc}_EXECUTABLE}"
        )
        set(Sphinx_${_exe}_FOUND TRUE)
    else()
        set(Sphinx_${_exe}_FOUND FALSE)
    endif()
    unset(_uc)
endmacro()

macro(_Sphinx_find_extension _ext)
    if(_SPHINX_PYTHON_EXECUTABLE)
        execute_process(
            COMMAND ${_SPHINX_PYTHON_EXECUTABLE} -c "import ${_ext}"
            RESULT_VARIABLE _result
        )
        if(_result EQUAL 0)
            set(Sphinx_${_ext}_FOUND TRUE)
        else()
            set(Sphinx_${_ext}_FOUND FALSE)
        endif()
    endif()
endmacro()

# Find sphinx-build and sphinx-quickstart.
_Sphinx_find_executable(build)
_Sphinx_find_executable(quickstart)

# Verify both executables are part of the Sphinx distribution.
if(SPHINX_BUILD_EXECUTABLE AND SPHINX_QUICKSTART_EXECUTABLE)
    if(NOT SPHINX_BUILD_VERSION STREQUAL SPHINX_QUICKSTART_VERSION)
        message(FATAL_ERROR " Versions for sphinx-build (${SPHINX_BUILD_VERSION}) "
                            " and sphinx-quickstart (${SPHINX_QUICKSTART_VERSION}) "
                            " do not match"
        )
    endif()
endif()

# To verify the required Sphinx extensions are available,
# the right Python installation must be queried (2 vs 3).
if(SPHINX_BUILD_EXECUTABLE)
    file(READ "${SPHINX_BUILD_EXECUTABLE}" _contents)
    if(_contents MATCHES "^#!([^\n]+)")
        string(STRIP "${CMAKE_MATCH_1}" _shebang)
        if(EXISTS "${_shebang}")
            set(_SPHINX_PYTHON_EXECUTABLE "${_shebang}")
        endif()
    endif()
endif()

foreach(_comp IN LISTS Sphinx_FIND_COMPONENTS)
    if(_comp STREQUAL "build")
        # Do nothing, sphinx-build is always required.
    elseif(_comp STREQUAL "quickstart")
        # Do nothing, sphinx-quickstart is optional, but looked up by default.
    elseif(_comp STREQUAL "breathe" OR _comp STREQUAL "exhale")
        _Sphinx_find_extension(${_comp})
    else()
        message(WARNING "${_comp} is not a valid or supported Sphinx extension")
        set(Sphinx_${_comp}_FOUND FALSE)
        continue()
    endif()
endforeach()

find_package_handle_standard_args(
    Sphinx
    VERSION_VAR SPHINX_VERSION
    REQUIRED_VARS SPHINX_BUILD_EXECUTABLE SPHINX_BUILD_VERSION
    HANDLE_COMPONENTS
)
