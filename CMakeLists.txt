cmake_minimum_required(VERSION 3.10)

project(MyceliumUI CXX)

# Require C++17 for std::string_view, std::optional etc.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all source files
add_executable(parser_app
    main.cpp
    lexer.cpp
    parser.cpp
    ast.cpp
    token.cpp
    hot_reload.cpp
    # token.hpp is header-only, no cpp needed unless you move tokenTypeToString impl there
)

# Optional: Enable warnings (recommended)
if(MSVC)
    target_compile_options(parser_app PRIVATE /W4)
else()
    target_compile_options(parser_app PRIVATE -Wall -Wextra -pedantic)
endif()