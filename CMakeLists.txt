cmake_minimum_required(VERSION 3.31)
project(transfer-tool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)