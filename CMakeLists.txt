cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

set(STREAMCLIENT_VERSION_MAJOR "1")
set(STREAMCLIENT_VERSION_MINOR "1")
set(STREAMCLIENT_VERSION_RELEASE "9")
set(STREAMCLIENT_SUMMARY "C++ library")
set(STREAMCLIENT_REPOSITORY_URL "https://github.com/TinkoffCreditSystems/stream-client")
set(STREAMCLIENT_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(STREAMCLIENT_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(STREAMCLIENT_VERSION_STRING "${STREAMCLIENT_VERSION_MAJOR}.${STREAMCLIENT_VERSION_MINOR}.${STREAMCLIENT_VERSION_RELEASE}")

## Make sure that custom modules are found
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

## Declare project
project("stream-client" LANGUAGES CXX
                  VERSION "${STREAMCLIENT_VERSION_STRING}"
                  DESCRIPTION "Lightweight, header-only, Boost-based socket pool library"
)

##############################################
# Declare options and project-wide parameters

set(CMAKE_CXX_STANDARD 14) # todo: upgrade to 17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

## Compile as RelWithDebInfo info by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

option(STREAMCLIENT_BUILD_TESTING "Build included unit-tests" OFF)
option(STREAMCLIENT_BUILD_DOCS "Build sphinx generated docs" OFF)
option(STREAMCLIENT_BUILD_EXAMPLES "Build stream-client examples" OFF)

##############################################
# Create target and set properties

set(STREAMCLIENT_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(stream-client
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${STREAMCLIENT_INC_DIR}>
    INTERFACE ${Boost_INCLUDE_DIRS}
)

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

target_link_libraries(stream-client INTERFACE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(stream-client INTERFACE Threads::Threads)
target_link_libraries(stream-client INTERFACE Boost::system)
if (NOT BUILD_SHARED_LIBS)
    target_link_libraries(stream-client INTERFACE ${CMAKE_DL_LIBS})
endif()

##############################################
# Installation instructions

include(GNUInstallDirs)

## Install targets
install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

## Install headers
install(DIRECTORY ${STREAMCLIENT_INC_DIR}/${PROJECT_NAME}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

## Configure cmake helpers
include(CMakePackageConfigHelpers)
set(STREAMCLIENT_VERSION_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/stream-client-config-version.cmake")
set(STREAMCLIENT_PROJECT_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/stream-client-config.cmake")
set(STREAMCLIENT_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/stream-client/")

write_basic_package_version_file(${STREAMCLIENT_VERSION_CONFIG}
    VERSION ${STREAMCLIENT_VERSION_STRING}
    COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(${PROJECT_SOURCE_DIR}/stream-client-config.cmake.in
    ${STREAMCLIENT_PROJECT_CONFIG}
    INSTALL_DESTINATION ${STREAMCLIENT_CMAKE_INSTALL_DIR}
)

## Install cmake helpers
install(FILES ${STREAMCLIENT_PROJECT_CONFIG}
              ${STREAMCLIENT_VERSION_CONFIG}
        DESTINATION ${STREAMCLIENT_CMAKE_INSTALL_DIR}
)
install(EXPORT ${PROJECT_NAME}-targets
        DESTINATION ${STREAMCLIENT_CMAKE_INSTALL_DIR}
        NAMESPACE ${PROJECT_NAME}::
)

##############################################
# Unit-tests

if(STREAMCLIENT_BUILD_TESTING)
    include(CTest)

    set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -lgcov")
    endif()

    add_subdirectory(tests)
endif()


##############################################
# Docs

if(STREAMCLIENT_BUILD_DOCS)
    add_subdirectory(docs)
endif()


##############################################
# Examples

if(STREAMCLIENT_BUILD_EXAMPLES)
    add_subdirectory(docs)
endif()
