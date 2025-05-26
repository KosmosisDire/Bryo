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

file(GLOB_RECURSE MY_SOURCE_FILES src/*.cpp src/*.h src/*.hpp lib/*.cpp)
add_executable(MyceliumUI ${MY_SOURCE_FILES} main.cpp)
target_link_libraries(MyceliumUI PRIVATE ${MY_LLVM_LIBS})

target_include_directories(MyceliumUI PRIVATE "include/sharpie" "include/ui")
