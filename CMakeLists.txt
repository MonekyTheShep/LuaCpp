cmake_minimum_required(VERSION 4.1)
project(LuaCpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/include/")

add_executable(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wimplicit-fallthrough
    -fsanitize=address
    -O3
)

target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address )

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE})