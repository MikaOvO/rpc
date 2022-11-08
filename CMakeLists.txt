cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 17) 

project(server)

include_directories("./src")

add_executable(server server.cpp)

set_target_properties(server PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")