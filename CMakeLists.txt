cmake_minimum_required(VERSION 3.9)

project(quickjs LANGUAGES C)

include(GNUInstallDirs)

# TODO:
#  - Support cross-compilation

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
else()
    add_compile_options(-g)
endif()

option(BUILD_SHARED_LIBS "Build a shared library" OFF)
if(BUILD_SHARED_LIBS)
    message(STATUS "Building a shared library")
endif()

option(CONFIG_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(CONFIG_MSAN "Enable MemorySanitizer (MSan)" OFF)
option(CONFIG_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)

if(CONFIG_ASAN)
message(STATUS "Building with ASan")
add_compile_definitions(
    __ASAN__=1
)
add_compile_options(
    -fsanitize=address
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
add_link_options(
    -fsanitize=address
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
elseif(CONFIG_MSAN)
message(STATUS "Building with MSan")
add_compile_options(
    -fsanitize=memory
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
add_link_options(
    -fsanitize=memory
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
elseif(CONFIG_UBSAN)
message(STATUS "Building with UBSan")
add_compile_options(
    -fsanitize=undefined
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
add_link_options(
    -fsanitize=undefined
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
)
endif()


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

add_library(qjs ${qjs_sources})
target_compile_definitions(qjs PRIVATE ${qjs_defines})
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(qjs PRIVATE
        DUMP_LEAKS
    )
endif()
target_include_directories(qjs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})


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


# Install target
#

file(STRINGS quickjs.h quickjs_h REGEX QJS_VERSION)
string(REGEX MATCHALL "([0-9])" QJS_VERSION "${quickjs_h}")
list(GET QJS_VERSION 0 QJS_VERSION_MAJOR)
list(GET QJS_VERSION 1 QJS_VERSION_MINOR)
list(GET QJS_VERSION 2 QJS_VERSION_PATCH)
set_target_properties(qjs PROPERTIES
    VERSION ${QJS_VERSION_MAJOR}.${QJS_VERSION_MINOR}.${QJS_VERSION_PATCH}
    SOVERSION ${QJS_VERSION_MAJOR}
)
install(FILES quickjs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS qjs_exe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS qjs EXPORT qjsConfig
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT qjsConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_DOCDIR})
