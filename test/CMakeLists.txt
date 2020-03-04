cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
set(CMAKE_MACOSX_RPATH 1)

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
    COMMAND openssl dhparam -noout -out ${SSL_DH_PARAMS} 2048 2>/dev/null
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})

add_custom_target(ssl_certificates ALL
    DEPENDS ${SSL_ROOT_CRT} ${SSL_SERVER_CRT} ${SSL_DH_PARAMS}
    WORKING_DIRECTORY ${SSL_CERTS_FOLDER})
add_definitions(-DSSL_ROOT_CA="${SSL_CERTS_FOLDER}/${SSL_ROOT_CRT}")
add_definitions(-DSSL_USER_CERT="${SSL_CERTS_FOLDER}/${SSL_SERVER_CRT}")
add_definitions(-DSSL_USER_KEY="${SSL_CERTS_FOLDER}/${SSL_SERVER_KEY}")
add_definitions(-DSSL_DH_PARAMS="${SSL_CERTS_FOLDER}/${SSL_DH_PARAMS}")

add_executable(general general.cpp)
add_executable(timeout timeout.cpp)
add_executable(transmission transmission.cpp)
add_executable(resolver resolver.cpp)
add_executable(pool pool.cpp)

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    set(TEST_LINK_LIBRARIES gtest_main stream-client::stream-client)
else()
    set(TEST_LINK_LIBRARIES stream-client::stream-client gtest_main)
endif()

target_link_libraries(general ${TEST_LINK_LIBRARIES})
target_link_libraries(timeout ${TEST_LINK_LIBRARIES})
target_link_libraries(transmission ${TEST_LINK_LIBRARIES})
target_link_libraries(resolver ${TEST_LINK_LIBRARIES})
target_link_libraries(pool ${TEST_LINK_LIBRARIES})

add_test(NAME general COMMAND general)
add_test(NAME timeout COMMAND timeout)
add_test(NAME transmission COMMAND transmission)
add_test(NAME resolver COMMAND resolver)
add_test(NAME pool COMMAND pool)
