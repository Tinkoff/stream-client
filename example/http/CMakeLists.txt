cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

find_package(Threads)

add_executable(example_connector example_connector.cpp)
target_link_libraries(example_connector PRIVATE stream-client::stream-client)
target_link_libraries(example_connector PRIVATE Threads::Threads)

add_executable(example_pool example_pool.cpp)
target_link_libraries(example_pool PRIVATE stream-client::stream-client)
target_link_libraries(example_pool PRIVATE Threads::Threads)
