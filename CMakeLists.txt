cmake_minimum_required(VERSION 3.9)

project(quickjs LANGUAGES C)

include(CheckCCompilerFlag)
include(GNUInstallDirs)

# TODO:
#  - Support cross-compilation

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} on ${CMAKE_SYSTEM}")

macro(xcheck_add_c_compiler_flag FLAG)
    string(REPLACE "-" "" FLAG_NO_HYPHEN ${FLAG})
    check_c_compiler_flag(${FLAG} COMPILER_SUPPORTS_${FLAG_NO_HYPHEN})
    if(COMPILER_SUPPORTS_${FLAG_NO_HYPHEN})
        add_compile_options(${FLAG})
    endif()
endmacro()

xcheck_add_c_compiler_flag(-Wall)
xcheck_add_c_compiler_flag(-Werror)
# -Wextra is too spartan on GCC
if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    add_compile_options(-Wextra)
endif()
xcheck_add_c_compiler_flag(-Wno-sign-compare)
xcheck_add_c_compiler_flag(-Wno-missing-field-initializers)
xcheck_add_c_compiler_flag(-Wno-unused-parameter)
xcheck_add_c_compiler_flag(-Wno-unused-variable)
xcheck_add_c_compiler_flag(-Wno-unused-but-set-variable)
xcheck_add_c_compiler_flag(-Wno-array-bounds)
xcheck_add_c_compiler_flag(-Wno-format-truncation)
xcheck_add_c_compiler_flag(-funsigned-char)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O0)
    xcheck_add_c_compiler_flag(-ggdb)
    xcheck_add_c_compiler_flag(-fno-omit-frame-pointer)
else()
    xcheck_add_c_compiler_flag(-g)
endif()

macro(xoption OPTION_NAME OPTION_TEXT OPTION_DEFAULT)
    option(${OPTION_NAME} ${OPTION_TEXT} ${OPTION_DEFAULT})
    if(DEFINED ENV{${OPTION_NAME}})
        # Allow setting the option through an environment variable.
        set(${OPTION_NAME} $ENV{${OPTION_NAME}})
    endif()
    if(${OPTION_NAME})
        add_definitions(-D${OPTION_NAME})
    endif()
    message(STATUS "  ${OPTION_NAME}: ${${OPTION_NAME}}")
endmacro()

xoption(BUILD_SHARED_LIBS "Build a shared library" OFF)
if(BUILD_SHARED_LIBS)
    message(STATUS "Building a shared library")
endif()

xoption(BUILD_EXAMPLES "Build examples" OFF)
xoption(BUILD_STATIC_QJS_EXE "Build a static qjs executable" OFF)
xoption(CONFIG_ASAN "Enable AddressSanitizer (ASan)" OFF)
xoption(CONFIG_MSAN "Enable MemorySanitizer (MSan)" OFF)
xoption(CONFIG_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)

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

xoption(BUILD_QJS_LIBC "Build standard library modules as part of the library" OFF)

set(qjs_sources
    cutils.c
    libbf.c
    libregexp.c
    libunicode.c
    quickjs.c
)

if(BUILD_QJS_LIBC)
    list(APPEND qjs_sources quickjs-libc.c)
endif()
list(APPEND qjs_defines _GNU_SOURCE)
list(APPEND qjs_libs qjs m)
if(NOT MINGW)
    list(APPEND qjs_libs dl pthread)
endif()

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

add_executable(qjsc
    qjsc.c
    quickjs-libc.c
)
target_compile_definitions(qjsc PRIVATE ${qjs_defines})
target_link_libraries(qjsc ${qjs_libs})


# QuickJS CLI
#

add_custom_command(
    OUTPUT repl.c
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/qjsc" -o ./repl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/repl.js
    DEPENDS qjsc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Compile repl.js to bytecode"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/repl.js
)

add_executable(qjs_exe
    qjs.c
    quickjs-libc.c
    ${CMAKE_CURRENT_BINARY_DIR}/repl.c
)
set_target_properties(qjs_exe PROPERTIES
    OUTPUT_NAME "qjs"
)
target_compile_definitions(qjs_exe PRIVATE ${qjs_defines})
target_link_libraries(qjs_exe ${qjs_libs})
if(BUILD_STATIC_QJS_EXE OR MINGW)
    target_link_options(qjs_exe PRIVATE -static)
    if(MINGW)
        target_link_options(qjs_exe PRIVATE -static-libgcc)
    endif()
endif()
if(NOT MINGW)
    set_target_properties(qjs_exe PROPERTIES ENABLE_EXPORTS TRUE)
endif()


# Test262 runner
#

add_executable(run-test262
    quickjs-libc.c
    run-test262.c
)
target_compile_definitions(run-test262 PRIVATE ${qjs_defines})
target_link_libraries(run-test262 ${qjs_libs})


# Unicode generator
#

add_executable(unicode_gen EXCLUDE_FROM_ALL
    cutils.c
    libunicode.c
    unicode_gen.c
)
target_compile_definitions(unicode_gen PRIVATE ${qjs_defines})


# Examples
#

if(BUILD_EXAMPLES AND NOT MINGW)
    add_custom_command(
        OUTPUT hello.c
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/qjsc" -e -o hello.c ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.js
        DEPENDS qjsc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Compile hello.js to a C file with bytecode embeddee"
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.js
    )
    add_executable(hello
        ${CMAKE_CURRENT_BINARY_DIR}/hello.c
        quickjs-libc.c
    )
    target_include_directories(hello PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(hello PRIVATE ${qjs_defines})
    target_link_libraries(hello ${qjs_libs})

    add_custom_command(
        OUTPUT hello_module.c
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/qjsc" -e -o hello_module.c -m ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello_module.js
        DEPENDS qjsc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Compile hello_module.js to a C file with bytecode embeddee"
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello_module.js
    )
    add_executable(hello_module
        ${CMAKE_CURRENT_BINARY_DIR}/hello_module.c
        quickjs-libc.c
    )
    target_include_directories(hello_module PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(hello_module PRIVATE ${qjs_defines})
    target_link_libraries(hello_module ${qjs_libs})

    if(NOT MINGW)
        add_library(fib MODULE examples/fib.c)
        set_target_properties(fib PROPERTIES
            PREFIX ""
        )
        target_compile_definitions(fib PRIVATE JS_SHARED_LIBRARY)
        if(APPLE)
            target_link_options(fib PRIVATE -undefined dynamic_lookup)
        endif()

        add_library(point MODULE examples/point.c)
        set_target_properties(point PROPERTIES
            PREFIX ""
        )
        target_compile_definitions(point PRIVATE JS_SHARED_LIBRARY)
        if(APPLE)
            target_link_options(point PRIVATE -undefined dynamic_lookup)
        endif()

        add_library(bjson MODULE tests/bjson.c)
        set_target_properties(bjson PROPERTIES
            PREFIX ""
        )
        target_compile_definitions(bjson PRIVATE JS_SHARED_LIBRARY)
        if(APPLE)
            target_link_options(bjson PRIVATE -undefined dynamic_lookup)
        endif()
    endif()

    add_custom_command(
        OUTPUT test_fib.c
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/qjsc" -e -o test_fib.c -M ${CMAKE_CURRENT_SOURCE_DIR}/examples/fib.so,fib -m ${CMAKE_CURRENT_SOURCE_DIR}/examples/test_fib.js
        DEPENDS qjsc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Compile test_fib.js to a C file with bytecode embedded"
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/test_fib.js
    )
    add_executable(test_fib
        ${CMAKE_CURRENT_BINARY_DIR}/test_fib.c
        examples/fib.c
        quickjs-libc.c
    )
    target_include_directories(test_fib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_fib PRIVATE ${qjs_defines})
    target_link_libraries(test_fib ${qjs_libs})
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
if(BUILD_QJS_LIBC)
    install(FILES quickjs-libc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(TARGETS qjs_exe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS qjs EXPORT qjsConfig
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT qjsConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_DOCDIR})
