cmake_minimum_required(VERSION 3.31)
project(transfer-tool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUUID REQUIRED uuid)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUUID_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto)

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)