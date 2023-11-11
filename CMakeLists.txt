cmake_minimum_required(VERSION 3.9)

project(quickjs LANGUAGES C)

# TODO:
#  - LTO
#  - Support cross-compilation
#  - ASAN / MSAN / UBSAN
#  - Install targets
#  - Shared library target

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

add_compile_options(
    -Wall
    -Werror
)
if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    add_compile_options(
        -Wextra
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -funsigned-char
    )
else()
    add_compile_options(
        -Wno-array-bounds
        -Wno-format-truncation
        -Wno-unused-variable
        -Wno-unused-but-set-variable
    )
endif()

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} on ${CMAKE_SYSTEM}")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(
        -ggdb
        -O0
        -fno-omit-frame-pointer
    )
endif()

set(CMAKE_VERBOSE_MAKEFILE TRUE)


# QuickJS library
#

set(qjs_sources
    cutils.c
    libbf.c
    libregexp.c
    libunicode.c
    quickjs.c
)

list(APPEND qjs_defines _GNU_SOURCE)

add_library(qjs STATIC ${qjs_sources})
target_compile_definitions(qjs PRIVATE ${qjs_defines})
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(qjs PRIVATE
        DUMP_LEAKS
    )
endif()
target_include_directories(qjs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


# QuickJS bytecode compiler
#

add_custom_command(
    OUTPUT repl.c
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/qjsc" -c -o ./repl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/repl.js
    DEPENDS qjsc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Compile repl.js to bytecode"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/repl.js
)

add_executable(qjsc
    qjsc.c
    quickjs-libc.c
)
target_compile_definitions(qjsc PRIVATE ${qjs_defines})
target_link_libraries(qjsc qjs m pthread)
if(NOT MINGW)
    target_link_libraries(qjsc dl)
endif()


# QuickJS CLI
#

add_executable(qjs_exe
    qjs.c
    quickjs-libc.c
    ${CMAKE_CURRENT_BINARY_DIR}/repl.c
)
set_target_properties(qjs_exe PROPERTIES
    OUTPUT_NAME "qjs"
)
target_compile_definitions(qjs_exe PRIVATE ${qjs_defines})
target_link_libraries(qjs_exe qjs m pthread)
if(NOT MINGW)
    target_link_libraries(qjs_exe dl)
endif()


# Test262 runner
#

add_executable(run-test262
    quickjs-libc.c
    run-test262.c
)
target_compile_definitions(run-test262 PRIVATE ${qjs_defines})
target_link_libraries(run-test262 qjs m pthread)
if(NOT MINGW)
    target_link_libraries(run-test262 dl)
endif()
