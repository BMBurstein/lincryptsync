cmake_minimum_required(VERSION 3.10)
project(lincryptsync)

set(CMAKE_CXX_STANDARD 17)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  link_libraries(stdc++fs)
endif()

add_executable(lincryptsync main.cpp)