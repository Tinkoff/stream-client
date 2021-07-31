cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

set(STREAMCLIENT_VERSION_MAJOR "1")
set(STREAMCLIENT_VERSION_MINOR "1")
set(STREAMCLIENT_VERSION_RELEASE "9")
set(STREAMCLIENT_VERSION_STRING "${STREAMCLIENT_VERSION_MAJOR}.${STREAMCLIENT_VERSION_MINOR}.${STREAMCLIENT_VERSION_RELEASE}")
set(STREAMCLIENT_LIB_VERSION ${STREAMCLIENT_VERSION_STRING})
mark_as_advanced(STREAMCLIENT_VERSION_MAJOR STREAMCLIENT_VERSION_MINOR STREAMCLIENT_VERSION_RELEASE STREAMCLIENT_VERSION_STRING STREAMCLIENT_LIB_VERSION)

project(stream-client VERSION "${STREAMCLIENT_VERSION_STRING}" LANGUAGES CXX)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 14) # todo: upgrade to 17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_UUID_RANDOM_GENERATOR_COMPAT")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unneeded-internal-declaration")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef")

# compile in release with debug info mode by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared library" ON)
option(STREAMCLIENT_BUILD_EXAMPLES "Build stream-client examples" OFF)

# If streamclient is being bundled in another project, we don't want to
# install anything.
if(NOT DEFINED STREAMCLIENT_BUNDLED_MODE)
  if("${PROJECT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    set(STREAMCLIENT_BUNDLED_MODE OFF)
  else()
    set(STREAMCLIENT_BUNDLED_MODE ON)
  endif()
endif()
set(STREAMCLIENT_BUNDLED_MODE ${STREAMCLIENT_BUNDLED_MODE} CACHE BOOL "" FORCE)
mark_as_advanced(STREAMCLIENT_BUNDLED_MODE)

# Allow people to choose whether to build shared or static libraries
# via the BUILD_SHARED_LIBS option unless we are in bundled mode, in
# which case we always use static libraries.
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON "NOT STREAMCLIENT_BUNDLED_MODE" OFF)

# libstream-client
add_library(stream-client INTERFACE)
add_library(${PROJECT_NAME}::stream-client ALIAS stream-client)

if (NOT BUILD_SHARED_LIBS)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    find_program(HOMEBREW brew)
    if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
        execute_process(COMMAND brew --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()
find_package(Threads)
find_package(OpenSSL 1.0 REQUIRED)
find_package(Boost 1.65 COMPONENTS system REQUIRED)

target_include_directories(stream-client
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    INTERFACE ${Boost_INCLUDE_DIRS}
)

target_link_libraries(stream-client INTERFACE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(stream-client INTERFACE Threads::Threads)
target_link_libraries(stream-client INTERFACE Boost::system)
if (NOT BUILD_SHARED_LIBS)
    target_link_libraries(stream-client INTERFACE ${CMAKE_DL_LIBS})
endif()

if(NOT STREAMCLIENT_BUNDLED_MODE)
    include(GNUInstallDirs)

    install(TARGETS stream-client
        EXPORT streamclientTargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

    install(DIRECTORY include/stream-client
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    include(CMakePackageConfigHelpers)

    set(streamclient_VERSION_CONFIG "${PROJECT_BINARY_DIR}/streamclientConfigVersion.cmake")
    set(streamclient_PROJECT_CONFIG "${PROJECT_BINARY_DIR}/streamclientConfig.cmake")
    set(streamclient_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/streamclient/")
    write_basic_package_version_file(${streamclient_VERSION_CONFIG}
        VERSION ${STREAMCLIENT_VERSION_STRING}
        COMPATIBILITY AnyNewerVersion)
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/streamclientConfig.cmake.in
        ${streamclient_PROJECT_CONFIG}
        INSTALL_DESTINATION ${streamclient_CONFIG_INSTALL_DIR})


    install(FILES ${streamclient_PROJECT_CONFIG} ${streamclient_VERSION_CONFIG}
        DESTINATION ${streamclient_CONFIG_INSTALL_DIR})
    install(EXPORT streamclientTargets
        DESTINATION ${streamclient_CONFIG_INSTALL_DIR}
        NAMESPACE ${PROJECT_NAME}::)
endif(NOT STREAMCLIENT_BUNDLED_MODE)

set(streamclient_TARGETS_CONFIG "${PROJECT_BINARY_DIR}/streamclientTargets.cmake")
export(EXPORT streamclientTargets
    FILE ${streamclient_TARGETS_CONFIG}
    NAMESPACE ${PROJECT_NAME}::)

if (STREAMCLIENT_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
endif()
