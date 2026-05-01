cmake_minimum_required(VERSION 3.16)
project(homelink_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(open62541 REQUIRED)

add_executable(homelink_bridge src/bridge/main.cpp)

target_link_libraries(homelink_bridge PRIVATE open62541::open62541 pthread)