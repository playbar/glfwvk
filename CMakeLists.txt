cmake_minimum_required(VERSION 3.4.1)
project(glfwvk LANGUAGES CXX C)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fexceptions")

add_subdirectory(glfw)
add_subdirectory(examples)
add_subdirectory(drawtriangle)