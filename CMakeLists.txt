cmake_minimum_required(VERSION 3.16)
project(MyceliumUI CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# When using vcpkg toolchain file, CMAKE_PREFIX_PATH is less critical for LLVM
# as vcpkg handles finding the package.
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} via vcpkg.")
message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} include)

set(LLVM_LINK_COMPONENTS
    # --- Core components you likely already have ---
    Support
    Core
    IRReader          # For reading IR (not strictly used in this JIT example but often included)
    BitReader         # For reading bitcode
    BitWriter         # For writing bitcode
    AsmParser         # For parsing LLVM assembly
    Analysis          # Various analyses
    Object            # For object file handling
    ProfileData
    Symbolize
    DebugInfoDWARF    # Or PDB/CodeView depending on what you need/vcpkg provides
    DebugInfoMSF
    Remarks
    BitstreamReader
    MCParser
    MC                # Machine Code library
    BinaryFormat
    TargetParser      # For parsing target triples, etc.
    Demangle
    MCJIT             

    # --- CRUCIAL JIT and Target Libraries to ADD ---
    ExecutionEngine   # Core JIT infrastructure, ExecutionSession
    OrcJIT            # For LLJIT itself
    OrcTargetProcess  # Support for out-of-process JITing, often linked
    OrcShared         # Shared ORC utilities
    Target            # Generic Target abstractions
    ipo               # Interprocedural optimizations (often used by JIT passes)

    # --- Native Target Specific Libraries (X86 example) ---
    # These are needed for InitializeNativeTarget(), etc.
    X86CodeGen        # Code generation for X86
    X86AsmParser      # Assembly parser for X86
    X86Desc           # X86 Target Descriptors
    X86Info           # X86 Target Information
    # Add other targets if you need them, e.g., AArch64CodeGen, etc.
    # For just InitializeNativeTarget(), you only need your host's target components.
)


llvm_map_components_to_libnames(MY_LLVM_LIBS ${LLVM_LINK_COMPONENTS})

set(MY_SOURCE_FILES
    # AST Implementation
    src/sharpie/ast/ast_base.cpp
    src/sharpie/ast/primitive_structs.cpp
    
    # Parser System
    src/sharpie/parser/parser_helpers.cpp
    src/sharpie/parser/parser_lexer.cpp
    src/sharpie/parser/parser_rules.cpp
    src/sharpie/parser/script_parser.cpp
    
    # Compiler System
    src/sharpie/compiler/compiler_visitors.cpp
    src/sharpie/compiler/scope_manager.cpp
    src/sharpie/compiler/script_compiler.cpp
    
    # Semantic Analyzer
    src/sharpie/semantic_analyzer/declaration_pass.cpp
    src/sharpie/semantic_analyzer/semantic_analyzer.cpp
    src/sharpie/semantic_analyzer/semantic_logging.cpp
    src/sharpie/semantic_analyzer/symbol_table.cpp
    src/sharpie/semantic_analyzer/uml_generator.cpp
    src/sharpie/semantic_analyzer/usage_collection_pass.cpp
    
    # Common Utilities
    src/sharpie/common/logger.cpp
    
    # UI System
    src/ui/ui_ast.cpp
    src/ui/ui_lexer.cpp
    src/ui/ui_parser.cpp
    src/ui/ui_token.cpp
    
    # Hot Reload
    src/hot_reload.cpp
)

set(MY_RUNTIME_FILES
    lib/mycelium_runtime.cpp
    lib/runtime_binding.cpp
)

add_executable(MyceliumUI ${MY_SOURCE_FILES} ${MY_RUNTIME_FILES} main.cpp)

target_link_libraries(MyceliumUI PRIVATE ${MY_LLVM_LIBS})

target_include_directories(MyceliumUI PRIVATE "include/sharpie" "include/ui" "lib")
