cmake_minimum_required(VERSION 3.16)
project(agv_system CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/lib/logger
)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)