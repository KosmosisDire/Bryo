cmake_minimum_required(VERSION 3.14)
cmake_policy(SET CMP0135 NEW) # For file(GLOB CONFIGURE_DEPENDS)

project(MyceliumUI CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- FetchContent for TinyCC ---
include(FetchContent)

FetchContent_Declare(
  tinycc
  URL      https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip
  URL_HASH MD5=d73cf66cec8c761de38c7a3d16c9eb0d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(tinycc) # Populates tinycc_SOURCE_DIR

# --- tcc_interface library (INTERFACE type) ---
add_library(tcc_interface INTERFACE)

target_include_directories(tcc_interface INTERFACE
  "${tinycc_SOURCE_DIR}/libtcc"
)

set(TCC_DLL_FILE_PATH "${tinycc_SOURCE_DIR}/libtcc.dll")
set(TCC_DEF_FILE_PATH "${tinycc_SOURCE_DIR}/libtcc/libtcc.def")

if(MSVC)
    if(NOT CMAKE_AR)
        message(FATAL_ERROR "CMAKE_AR (MSVC Librarian 'lib.exe') is not defined. Ensure Visual Studio generator is used.")
    endif()

    set(TCC_IMPORT_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/tcc_msvc_import")
    set(TCC_IMPORT_LIB_FILE_PATH "${TCC_IMPORT_LIB_DIR}/libtcc.lib")

    add_custom_command(
        OUTPUT ${TCC_IMPORT_LIB_FILE_PATH}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TCC_IMPORT_LIB_DIR}"
        COMMAND ${CMAKE_AR} /DEF:${TCC_DEF_FILE_PATH} /OUT:${TCC_IMPORT_LIB_FILE_PATH} /MACHINE:X64 # Corrected line
        DEPENDS
            "${TCC_DEF_FILE_PATH}"
            "${TCC_DLL_FILE_PATH}"
        COMMENT "Generating MSVC import library libtcc.lib from ${TCC_DEF_FILE_PATH}"
        VERBATIM
    )

    add_custom_target(generate_tcc_import_lib ALL
        DEPENDS ${TCC_IMPORT_LIB_FILE_PATH}
    )

    add_dependencies(tcc_interface generate_tcc_import_lib)

    target_link_libraries(tcc_interface INTERFACE ${TCC_IMPORT_LIB_FILE_PATH})
else()
    message(STATUS "Non-MSVC build, attempting to link directly against ${TCC_DLL_FILE_PATH}")
    target_link_libraries(tcc_interface INTERFACE "${TCC_DLL_FILE_PATH}")
endif()

# --- Main Executable: MyceliumUI ---
file(GLOB_RECURSE MYCELIUM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(MyceliumUI main.cpp ${MYCELIUM_SOURCES})

target_include_directories(MyceliumUI PUBLIC include)
target_link_libraries(MyceliumUI PRIVATE tcc_interface)

include_directories(MyceliumUI PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/sharpie"
)

target_include_directories(MyceliumUI PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/sharpie"
)

add_custom_command(
  TARGET MyceliumUI POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${TCC_DLL_FILE_PATH}"
          $<TARGET_FILE_DIR:MyceliumUI>
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different 
          "${tinycc_SOURCE_DIR}/include"
          "$<TARGET_FILE_DIR:MyceliumUI>/include"
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different 
          "${tinycc_SOURCE_DIR}/lib"
          "$<TARGET_FILE_DIR:MyceliumUI>/lib"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/mui_lib.c"
          "$<TARGET_FILE_DIR:MyceliumUI>/lib/mui_lib.c"

  COMMENT "Copying libtcc.dll to output directory"
)

if(MSVC)
    target_compile_options(MyceliumUI PRIVATE /W4)
else()
    target_compile_options(MyceliumUI PRIVATE -Wall -Wextra -pedantic)
endif()