cmake_minimum_required(VERSION 3.16)
project(retro_writer_tv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TV_OPTIMIZE_BUILD ON CACHE BOOL "" FORCE)

add_subdirectory(vendor/tvision)

add_executable(retro_writer_tv retro_writer_tv.cpp)
target_link_libraries(retro_writer_tv PRIVATE tvision::tvision)
