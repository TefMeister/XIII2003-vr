cmake_minimum_required(VERSION 3.20)
project(XIII2003VR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(pose_math)
add_subdirectory(capture_core)
add_subdirectory(tests)
add_subdirectory(proxy)
