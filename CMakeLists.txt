cmake_minimum_required(VERSION 3.28.0)

project(HMake LANGUAGES CXX)

# HMake uses explicit 64-bit sizes and offsets throughout its runtime and cache handling.
if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "HMake requires a 64-bit host toolchain")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(HMAKE_DEFAULT_TOOLCHAIN_FAMILY "clang")
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(HMAKE_DEFAULT_TOOLCHAIN_NAME "clang-cl")
        set(HMAKE_DEFAULT_TOOLCHAIN_STYLE "msvc")
    else ()
        set(HMAKE_DEFAULT_TOOLCHAIN_NAME "clang")
        set(HMAKE_DEFAULT_TOOLCHAIN_STYLE "gnu")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(HMAKE_DEFAULT_TOOLCHAIN_NAME "msvc")
    set(HMAKE_DEFAULT_TOOLCHAIN_FAMILY "msvc")
    set(HMAKE_DEFAULT_TOOLCHAIN_STYLE "msvc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(HMAKE_DEFAULT_TOOLCHAIN_NAME "gcc")
    set(HMAKE_DEFAULT_TOOLCHAIN_FAMILY "gcc")
    set(HMAKE_DEFAULT_TOOLCHAIN_STYLE "gnu")
else ()
    message(FATAL_ERROR "HMake requires Clang, GCC, or MSVC to define its built-in toolchain")
endif ()

if (CMAKE_CXX_COMPILER_TARGET)
    set(HMAKE_DEFAULT_TARGET "${CMAKE_CXX_COMPILER_TARGET}")
else ()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}" HMAKE_DEFAULT_TARGET)
endif ()

set(HMAKE_DEFAULT_COMPILER "${CMAKE_CXX_COMPILER}")
set(HMAKE_DEFAULT_LINKER "${CMAKE_CXX_COMPILER}")
set(HMAKE_DEFAULT_ARCHIVER "${CMAKE_AR}")
set(HMAKE_DEFAULT_TOOLCHAIN_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
set(HMAKE_DEFAULT_INCLUDE_DIRS
        ${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}
        ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
set(HMAKE_DEFAULT_LIBRARY_DIRS ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})

if (HMAKE_DEFAULT_TOOLCHAIN_STYLE STREQUAL "msvc")
    cmake_path(CONVERT "$ENV{INCLUDE}" TO_CMAKE_PATH_LIST HMAKE_MSVC_INCLUDE_DIRS NORMALIZE)
    cmake_path(CONVERT "$ENV{LIB}" TO_CMAKE_PATH_LIST HMAKE_MSVC_LIBRARY_DIRS NORMALIZE)
    list(APPEND HMAKE_DEFAULT_INCLUDE_DIRS ${HMAKE_MSVC_INCLUDE_DIRS})
    list(APPEND HMAKE_DEFAULT_LIBRARY_DIRS ${HMAKE_MSVC_LIBRARY_DIRS})
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

list(FILTER HMAKE_DEFAULT_INCLUDE_DIRS EXCLUDE REGEX "^$")
list(FILTER HMAKE_DEFAULT_LIBRARY_DIRS EXCLUDE REGEX "^$")
list(REMOVE_DUPLICATES HMAKE_DEFAULT_INCLUDE_DIRS)
list(REMOVE_DUPLICATES HMAKE_DEFAULT_LIBRARY_DIRS)

foreach (variable IN ITEMS
        HMAKE_DEFAULT_TOOLCHAIN_NAME
        HMAKE_DEFAULT_COMPILER
        HMAKE_DEFAULT_LINKER
        HMAKE_DEFAULT_ARCHIVER
        HMAKE_DEFAULT_TOOLCHAIN_FAMILY
        HMAKE_DEFAULT_TOOLCHAIN_STYLE
        HMAKE_DEFAULT_TOOLCHAIN_VERSION
        HMAKE_DEFAULT_TARGET)
    set(escaped "${${variable}}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    set(${variable}_CPP "\"${escaped}\"")
endforeach ()

foreach (directoryList IN ITEMS HMAKE_DEFAULT_INCLUDE_DIRS HMAKE_DEFAULT_LIBRARY_DIRS)
    set(initializer "{")
    set(separator "")
    foreach (directory IN LISTS ${directoryList})
        set(escaped "${directory}")
        string(REPLACE "\\" "\\\\" escaped "${escaped}")
        string(REPLACE "\"" "\\\"" escaped "${escaped}")
        string(REPLACE "\n" "\\n" escaped "${escaped}")
        string(REPLACE "\r" "\\r" escaped "${escaped}")
        string(APPEND initializer "${separator}\"${escaped}\"")
        set(separator ", ")
    endforeach ()
    string(APPEND initializer "}")
    set(${directoryList}_CPP "${initializer}")
endforeach ()

set(HMAKE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${HMAKE_GENERATED_INCLUDE_DIR}")
file(CONFIGURE
        OUTPUT "${HMAKE_GENERATED_INCLUDE_DIR}/BuiltinToolchain.hpp"
        CONTENT [=[#ifndef HMAKE_BUILTIN_TOOLCHAIN_HPP
#define HMAKE_BUILTIN_TOOLCHAIN_HPP

#define HMAKE_DEFAULT_TOOLCHAIN_NAME @HMAKE_DEFAULT_TOOLCHAIN_NAME_CPP@
#define HMAKE_DEFAULT_COMPILER @HMAKE_DEFAULT_COMPILER_CPP@
#define HMAKE_DEFAULT_LINKER @HMAKE_DEFAULT_LINKER_CPP@
#define HMAKE_DEFAULT_ARCHIVER @HMAKE_DEFAULT_ARCHIVER_CPP@
#define HMAKE_DEFAULT_TOOLCHAIN_FAMILY @HMAKE_DEFAULT_TOOLCHAIN_FAMILY_CPP@
#define HMAKE_DEFAULT_TOOLCHAIN_STYLE @HMAKE_DEFAULT_TOOLCHAIN_STYLE_CPP@
#define HMAKE_DEFAULT_TOOLCHAIN_VERSION @HMAKE_DEFAULT_TOOLCHAIN_VERSION_CPP@
#define HMAKE_DEFAULT_TARGET @HMAKE_DEFAULT_TARGET_CPP@
#define HMAKE_DEFAULT_INCLUDE_DIRS @HMAKE_DEFAULT_INCLUDE_DIRS_CPP@
#define HMAKE_DEFAULT_LIBRARY_DIRS @HMAKE_DEFAULT_LIBRARY_DIRS_CPP@

#endif // HMAKE_BUILTIN_TOOLCHAIN_HPP
]=]
        @ONLY
        NEWLINE_STYLE LF)

set(HCONFIGURE_SOURCES
        hconfigure/src/Features.cpp
        hconfigure/src/BTarget.cpp
        hconfigure/src/Builder.cpp
        hconfigure/src/BuildSystemFunctions.cpp
        hconfigure/src/Cache.cpp
        hconfigure/src/CppMod.cpp
        hconfigure/src/ParseHeaderDeps.cpp
        hconfigure/src/Configuration.cpp
        hconfigure/src/CppTarget.cpp
        hconfigure/src/ObjectFileProducer.cpp
        hconfigure/src/IspcTarget.cpp
        hconfigure/src/LOAT.cpp
        hconfigure/src/BuildTools.cpp
        hconfigure/src/Toolchains.cpp
        hconfigure/src/RunCommand.cpp
        hconfigure/src/Configure.cpp
        hconfigure/src/PLOAT.cpp
        hconfigure/src/Node.cpp
        hconfigure/src/HashValues.cpp
        hconfigure/src/IPCManagerBS.cpp
        hconfigure/src/IPCManagerCompiler.cpp
        hconfigure/src/Manager.cpp
        hconfigure/src/BoostCppTarget.cpp
        hconfigure/src/CustomCodeGenerator.cpp
        hconfigure/src/ue.cpp
        hconfigure/src/CustomAllocator.cpp)

set_source_files_properties(hconfigure/src/Toolchains.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${HMAKE_GENERATED_INCLUDE_DIR}")

find_package(Threads REQUIRED)

foreach (target IN ITEMS hconfigure-c hconfigure-b)
    add_library(${target} STATIC ${HCONFIGURE_SOURCES})
    target_include_directories(${target}
            PUBLIC
            "${PROJECT_SOURCE_DIR}/hconfigure/header"
            "${PROJECT_SOURCE_DIR}/3rdParty"
            PRIVATE
            "${PROJECT_SOURCE_DIR}/3rdParty/rapidjson/include")
    target_link_libraries(${target} PUBLIC Threads::Threads)

    if (HMAKE_DEFAULT_TOOLCHAIN_STYLE STREQUAL "msvc")
        target_compile_options(${target} PUBLIC /GR- /EHs-c-)
        target_compile_definitions(${target} PUBLIC _HAS_EXCEPTIONS=0)
    else ()
        target_compile_options(${target} PUBLIC -fno-exceptions -fno-rtti -fvisibility=hidden)
    endif ()
endforeach ()

target_compile_definitions(hconfigure-b PUBLIC BUILD_MODE)

add_executable(hbuild hbuild/src/main.cpp hbuild/src/Bootstrap.cpp)
target_link_libraries(hbuild PRIVATE hconfigure-c)
add_dependencies(hbuild hconfigure-b)
target_compile_definitions(hbuild PRIVATE
        THIRD_PARTY_HEADER="${PROJECT_SOURCE_DIR}/3rdParty/"
        RAPIDJSON_HEADER="${PROJECT_SOURCE_DIR}/3rdParty/rapidjson/include/"
        HCONFIGURE_HEADER="${PROJECT_SOURCE_DIR}/hconfigure/header/"
        HCONFIGURE_C_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure-c>"
        HCONFIGURE_B_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure-b>")

set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt OFF)
add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_executable(Tests Tests/main.cpp
        Tests/ExamplesTestHelper.cpp
        Tests/Test_Examples.cpp
        Tests/StageTests.cpp
        Tests/Snapshot.cpp)
target_link_libraries(Tests PRIVATE GTest::gtest hconfigure-b)
target_compile_definitions(Tests PRIVATE SOURCE_DIRECTORY="${PROJECT_SOURCE_DIR}/")
add_dependencies(Tests hbuild)

add_executable(copy utils/copy.cpp)
add_executable(CmdAnalysis utils/cmd-analysis.cpp)
add_executable(Analysis2 utils/analysis2.cpp)
add_executable(Dir-Mapping utils/dir-mapping.cpp)
add_executable(PerfTest utils/perf-test.cpp)

target_link_libraries(CmdAnalysis PRIVATE hconfigure-c)
target_link_libraries(Analysis2 PRIVATE hconfigure-c)
target_link_libraries(Dir-Mapping PRIVATE hconfigure-c)

if (WIN32)
    add_executable(monitor utils/monitor.cpp)
    target_link_libraries(monitor PRIVATE Psapi)
endif ()

if (HMAKE_DEFAULT_TOOLCHAIN_STYLE STREQUAL "msvc")
    add_executable(benchmark Examples/benchmark/benchmark.cpp)
endif ()

function(add_dual_executables)
    set(entries ${ARGN})
    while (entries)
        list(POP_FRONT entries baseName sourceFile)
        add_executable("${baseName}Config" "${sourceFile}")
        target_link_libraries("${baseName}Config" PRIVATE hconfigure-c)
        add_executable("${baseName}Build" "${sourceFile}")
        target_link_libraries("${baseName}Build" PRIVATE hconfigure-b)
    endwhile ()
endfunction ()

add_dual_executables(Example1 Examples/Example1/hmake.cpp
        Example2 Examples/Example2/hmake.cpp
        Example3 Examples/Example3/hmake.cpp
        Example4 Examples/Example4/hmake.cpp
        Example5 Examples/Example5/hmake.cpp
        Example6 Examples/Example6/hmake.cpp
        Example7 Examples/Example7/hmake.cpp
        Example8 Examples/Example8/hmake.cpp
        Example9 Examples/Example9/hmake.cpp
        Example10 Examples/Example10/hmake.cpp
        Example11 Examples/Example11/hmake.cpp
        Example-A1 Examples/Example-A1/hmake.cpp
        Example-A2 Examples/Example-A2/hmake.cpp
        Example-A4 Examples/Example-A4/hmake.cpp
        Example-A5 Examples/Example-A5/hmake.cpp
        Example-A6 Examples/Example-A6/hmake.cpp
        Example-A7 Examples/Example-A7/hmake.cpp
        Example-A9 Examples/Example-A9/hmake.cpp
        Example-A10 Examples/Example-A10/hmake.cpp
        Test1 Tests/Stage/Test1/hmake.cpp
        Test2 Tests/Stage/Test2/hmake.cpp
        Test3 Tests/Stage/Test3/hmake.cpp
        Test4 Tests/Stage/Test4/hmake.cpp
        Test6 Tests/Stage/Test6/hmake.cpp
        HMake hmake.cpp
        llvm Projects/LLVM/hmake.cpp
        UE5 Projects/UE5/hmake.cpp
        Boost Projects/Boost/hmake.cpp)

# TODO
# https://rapidjson.org/group___r_a_p_i_d_j_s_o_n___c_o_n_f_i_g.html
# macros to be looked for simd from above and whether that they are defined
