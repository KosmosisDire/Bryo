cmake_minimum_required(VERSION 3.10)

project(MyceliumUI CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all source files
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_executable(MyceliumUI ${SOURCES} main.cpp)

target_include_directories(MyceliumUI PUBLIC include)

if(MSVC)
    target_compile_options(MyceliumUI PRIVATE /W4)
else()
    target_compile_options(MyceliumUI PRIVATE -Wall -Wextra -pedantic)
endif()