cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
set(CMAKE_MACOSX_RPATH 1)

if(NOT TARGET testing)
    set(TETING_TARGET_LOCAL "True")
    add_custom_target(testing COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
else()
    set(TETING_TARGET_LOCAL "False")
endif()

# Locate GTest
find_package(GTest)
if(GTest_FOUND)
    set(EXTRA_LIBS ${GTEST_LIBRARIES})
else(GTest_FOUND)
    # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
    if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                     ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                     EXCLUDE_FROM_ALL)

    set(EXTRA_LIBS gtest_main)
endif(GTest_FOUND)

set(SSL_CERTS_FOLDER ${CMAKE_CURRENT_BINARY_DIR})
set(SSL_ROOT_KEY ssl_rootca.key)
set(SSL_ROOT_CRT ssl_rootca.crt)
set(SSL_SERVER_KEY ssl_server.key)
set(SSL_SERVER_CSR ssl_server.csr)
set(SSL_SERVER_CRT ssl_server.crt)
set(SSL_DH_PARAMS ssl_dh2048.pem)

add_custom_command(OUTPUT ${SSL_ROOT_KEY} ${SSL_ROOT_CRT}
    COMMAND openssl req -new -newkey rsa:2048 -days 365 -nodes -x509
        -subj "/C=AU/ST=Some-State/O=Internet Widgits Pty Ltd"
        -keyout ${SSL_ROOT_KEY} -out ${SSL_ROOT_CRT} 2>/dev/null
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})
add_custom_command(OUTPUT ${SSL_SERVER_KEY} ${SSL_SERVER_CSR}
    COMMAND openssl req -new -newkey rsa:2048 -nodes
        -subj "/C=AU/ST=Some-State/O=Internet Widgits Pty Ltd/CN=localhost"
        -keyout ${SSL_SERVER_KEY} -out ${SSL_SERVER_CSR} 2>/dev/null
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})
add_custom_command(OUTPUT ${SSL_SERVER_CRT}
    COMMAND openssl x509 -req -days 365 -in ${SSL_SERVER_CSR}
        -CA ${SSL_ROOT_CRT} -CAkey ${SSL_ROOT_KEY}
        -CAcreateserial -out ${SSL_SERVER_CRT}
    DEPENDS ${SSL_SERVER_CSR} ${SSL_ROOT_CRT} ${SSL_ROOT_KEY}
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})
add_custom_command(OUTPUT ${SSL_DH_PARAMS}
    COMMAND openssl dhparam -dsaparam -out ${SSL_DH_PARAMS} 2048 2>/dev/null
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})

add_custom_target(ssl_certificates ALL
    DEPENDS ${SSL_ROOT_CRT} ${SSL_SERVER_CRT} ${SSL_DH_PARAMS}
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})
if (TETING_TARGET_LOCAL)
    add_dependencies(testing ssl_certificates)
endif()

add_definitions(-DSSL_ROOT_CA="${SSL_CERTS_FOLDER}/${SSL_ROOT_CRT}")
add_definitions(-DSSL_USER_CERT="${SSL_CERTS_FOLDER}/${SSL_SERVER_CRT}")
add_definitions(-DSSL_USER_KEY="${SSL_CERTS_FOLDER}/${SSL_SERVER_KEY}")
add_definitions(-DSSL_DH_PARAMS="${SSL_CERTS_FOLDER}/${SSL_DH_PARAMS}")

function(add_unit_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ${PROJECT_NAME}::${PROJECT_NAME} ${EXTRA_LIBS})
    add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
    if (TETING_TARGET_LOCAL)
        add_dependencies(testing ${name})
    endif()
endfunction()

add_unit_test(general general.cpp)
add_unit_test(timeout timeout.cpp)
add_unit_test(transmission transmission.cpp)
add_unit_test(resolver resolver.cpp)
add_unit_test(pool pool.cpp)
