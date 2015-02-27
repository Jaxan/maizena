cmake_minimum_required(VERSION 3.0)
project(maizena)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread")

find_package(Boost REQUIRED COMPONENTS program_options filesystem system serialization)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(libs ${libs} ${Boost_LIBRARIES})

file(GLOB sources "*.cpp")
file(GLOB headers "*.hpp")

add_executable(main ${sources})
target_link_libraries(main ${libs})
