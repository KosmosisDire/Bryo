cmake_minimum_required(VERSION 3.16)
project(MyceliumUI C CXX)

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
    
    # Semantic Analyzer
    src/semantic/symbol_table.cpp
    
    # Code Generator
    src/codegen/codegen.cpp
    src/codegen/ir_builder.cpp
    src/codegen/command_processor.cpp
    
    # Common Utilities
    src/common/logger.cpp
)

set(RUNTIME_FILES
    # lib/mycelium_runtime.cpp
    # lib/runtime_binding.cpp
)

add_executable(MyceliumUI ${SOURCE_FILES} ${RUNTIME_FILES} main.cpp)

if(LLVM_AVAILABLE)
    target_link_libraries(MyceliumUI PRIVATE ${LLVM_LIBS})
endif()

target_include_directories(MyceliumUI PRIVATE "include" "lib")