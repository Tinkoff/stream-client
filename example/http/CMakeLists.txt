cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

find_package(Threads)

add_executable(example_client example_client.cpp)

target_link_libraries(example_client PRIVATE stream-client::stream-client)
target_link_libraries(example_client PRIVATE Threads::Threads)
