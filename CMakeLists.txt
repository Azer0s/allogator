cmake_minimum_required(VERSION 3.22)
project(allogator C)

set(CMAKE_C_STANDARD 23)

add_library(allogator SHARED library.c main.c)

# add main as executable
project(allogator_exe)
add_executable(allogator_exe main.c)
target_link_libraries(allogator_exe allogator)
