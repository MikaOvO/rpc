cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 17) 

project(demo)

find_package( Boost REQUIRED )

# add_subdirectory(../third_party/json json_binary_dir)
include_directories(../third_party/msgpack-c/include)
include_directories( ${Boost_INCLUDE_DIR} )

# find_package(msgpack REQUIRED)

ADD_EXECUTABLE(demo main.cpp)

# set_target_properties(demo PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries( demo PRIVATE ${Boost_LIBRARIES} )
# target_link_libraries(demo PRIVATE msgpackc-cxx)
# target_include_directories(demo PRIVATE ../third_party/msgpack-c)