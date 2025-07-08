cmake_minimum_required(VERSION 3.16)
project(Myre C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Try to find LLVM, but make it optional for basic functionality
find_package(LLVM CONFIG)

if(LLVM_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")
    include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
    set(LLVM_AVAILABLE TRUE)
else()
    message(STATUS "LLVM not found - building without JIT functionality")
    set(LLVM_AVAILABLE FALSE)
endif()

include_directories(include)

if(LLVM_AVAILABLE)
    # Check if we should use monolithic LLVM library
    if(UNIX AND NOT APPLE)
        # On Linux, check if component libraries exist
        find_library(LLVM_CORE_LIB LLVMCore PATHS ${LLVM_LIBRARY_DIR} NO_DEFAULT_PATH)
        if(NOT LLVM_CORE_LIB)
            # Use monolithic library
            message(STATUS "Using monolithic LLVM library")
            set(LLVM_LIBS LLVM)
        else()
            # Use component libraries
            set(LLVM_LINK_COMPONENTS
                Core
                Support
                IRReader
                Target
                MC
                CodeGen
                ExecutionEngine
                MCJIT
                native
            )
            llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_LINK_COMPONENTS})
            message(STATUS "Using LLVM component libraries: ${LLVM_LIBS}")
        endif()
    else()
        # On Windows and macOS, try component libraries first
        set(LLVM_LINK_COMPONENTS
            Core
            Support
            IRReader
            Target
            MC
            CodeGen
            ExecutionEngine
            MCJIT
            native
        )
        llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_LINK_COMPONENTS})
        if(NOT LLVM_LIBS)
            message(STATUS "Using monolithic LLVM library")
            set(LLVM_LIBS LLVM)
        else()
            message(STATUS "Using LLVM component libraries: ${LLVM_LIBS}")
        endif()
    endif()
endif()

set(SOURCE_FILES
    # AST Implementation
    src/ast/ast.cpp
    src/ast/ast_allocator.cpp
    
    # Parser Implementation
    src/parser/lexer.cpp
    src/parser/token_stream.cpp
    
    # Semantic Analyzer
    src/semantic/symbol_table.cpp
    
    # Code Generator
    src/codegen/codegen.cpp
    src/codegen/ir_builder.cpp
    src/codegen/command_processor.cpp
    src/codegen/jit_engine.cpp
    
    # Common Utilities
    src/common/logger.cpp
    src/common/token.cpp
)

set(RUNTIME_FILES
    # lib/mycelium_runtime.cpp
    # lib/runtime_binding.cpp
)

set(TEST_FILES
    tests/test_lexer.cpp
    tests/test_command_generation.cpp
    tests/test_ir_generation.cpp
    tests/test_jit_execution.cpp
)

# Main executable
add_executable(Myre ${SOURCE_FILES} ${RUNTIME_FILES} main.cpp)

# Test runner executable
add_executable(TestRunner ${SOURCE_FILES} ${TEST_FILES} test_runner.cpp)

if(LLVM_AVAILABLE)
    target_link_libraries(Myre PRIVATE ${LLVM_LIBS})
    target_link_libraries(TestRunner PRIVATE ${LLVM_LIBS})
endif()

target_include_directories(Myre PRIVATE "include" "lib")
target_include_directories(TestRunner PRIVATE "include" "lib")