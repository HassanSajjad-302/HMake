cmake_minimum_required(VERSION 3.28.0)

project(HMake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(HMAKE_USE_THREAD_SANITIZER OFF BOOL "Whether to use thread-sanitizer on Linux or not.")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")

    if ("${HMAKE_USE_THREAD_SANITIZER}" MATCHES ON)
        add_link_options(-fsanitize=address -fno-omit-frame-pointer)
        add_compile_options(-fsanitize=address)
    endif ()

    add_compile_options(-fno-exceptions -fno-rtti)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_compile_options(/fsanitize=address)
    endif ()

    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_definitions(-D_HAS_EXCEPTIONS=0) # for STL

endif ()

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 0)

add_subdirectory(3rdParty/fmt)
add_subdirectory(3rdParty/json)

set(HCONFIGURE_SOURCES
        hconfigure/src/Settings.cpp
        hconfigure/src/Features.cpp
        hconfigure/src/BTarget.cpp
        hconfigure/src/Builder.cpp
        hconfigure/src/BuildSystemFunctions.cpp
        hconfigure/src/Cache.cpp
        hconfigure/src/HashedCommand.cpp
        hconfigure/src/SMFile.cpp
        hconfigure/src/Configuration.cpp
        hconfigure/src/CppSourceTarget.cpp
        hconfigure/src/LOAT.cpp
        hconfigure/src/BuildTools.cpp
        hconfigure/src/ToolsCache.cpp
        hconfigure/src/RunCommand.cpp
        hconfigure/src/Utilities.cpp
        hconfigure/src/ObjectFileProducer.cpp
        hconfigure/src/Configure.cpp
        hconfigure/src/PLOAT.cpp
        hconfigure/src/PlatformSpecific.cpp
        hconfigure/src/Node.cpp
        hconfigure/src/ObjectFile.cpp
        hconfigure/src/SpecialNodes.cpp
        3rdParty/lz4/lib/lz4.c
        hconfigure/src/ConfigHelpers.cpp
        hconfigure/src/HashValues.cpp
        hconfigure/src/TargetCache.cpp
        hconfigure/src/TargetCacheDiskWriteManager.cpp
        hconfigure/src/BoostCppTarget.cpp)

set(INCLUDE_BUILD ON)
set(INCLUDE_CONFIGURE ON)
set(HMAKE_USE_SMALL_FILE ON BOOL "use full file or small file(compressed, indices, hashed) for storing cache.")
set(HelperSource Projects/Boost/hmake.cpp)

if (${INCLUDE_CONFIGURE})

    add_library(hconfigure-c STATIC ${HCONFIGURE_SOURCES})
    target_compile_features(hconfigure-c PUBLIC cxx_std_23)

    if (UNIX)
        target_compile_options(hconfigure-c PRIVATE -fPIC)
    endif ()

    target_link_libraries(hconfigure-c PUBLIC fmt::fmt nlohmann_json::nlohmann_json)
    target_include_directories(hconfigure-c
            PUBLIC hconfigure/header
            PUBLIC 3rdParty
            PUBLIC 3rdParty/rapidjson/include
            PUBLIC 3rdParty/parallel-hashmap/parallel_hashmap
            PUBLIC 3rdParty/lz4/lib
    )

    add_executable(htools htools/main.cpp)
    target_link_libraries(htools PRIVATE hconfigure-c)

    if ("${HMAKE_USE_SMALL_FILE}" MATCHES ON)
        message("HMAKE_USE_SMALL_FILE set")
        # Comment out the following to switch from hash to compile-command in the build-log.
        target_compile_definitions(hconfigure-c PUBLIC USE_COMMAND_HASH)

        # Comment out the following to switch from nodes.json numbers reflected in build-cache.json to full normalized paths in build-cache.json
        target_compile_definitions(hconfigure-c PUBLIC USE_NODES_CACHE_INDICES_IN_CACHE)

        # Comment out the following to switch from json files stored as compressed vs stored as-it-is
        target_compile_definitions(hconfigure-c PUBLIC USE_JSON_FILE_COMPRESSION)

    endif ()

    if (UNIX)
        target_compile_options(hconfigure-c PRIVATE -fvisibility=hidden)
    endif ()

    #Following target helps in writing hmake.cpp file with ide setup. Otherwise unused

    add_executable(ConfigureHelper ${HelperSource})
    target_link_libraries(ConfigureHelper PRIVATE hconfigure-c)

endif ()

if (${INCLUDE_BUILD})

    add_library(hconfigure-b STATIC ${HCONFIGURE_SOURCES})
    target_compile_features(hconfigure-b PUBLIC cxx_std_23)


    target_compile_definitions(hconfigure-b PUBLIC BUILD_MODE)

    if (UNIX)
        target_compile_options(hconfigure-b PRIVATE -fPIC)
    endif ()

    target_link_libraries(hconfigure-b PUBLIC fmt::fmt nlohmann_json::nlohmann_json)
    target_include_directories(hconfigure-b
            PUBLIC hconfigure/header
            PUBLIC 3rdParty
            PUBLIC 3rdParty/rapidjson/include
            PUBLIC 3rdParty/parallel-hashmap/parallel_hashmap
            PUBLIC 3rdParty/lz4/lib
    )

    if ("${HMAKE_USE_SMALL_FILE}" MATCHES ON)
        message("HMAKE_USE_SMALL_FILE set")
        target_compile_definitions(hconfigure-b PUBLIC USE_COMMAND_HASH)
        target_compile_definitions(hconfigure-b PUBLIC USE_NODES_CACHE_INDICES_IN_CACHE)
        target_compile_definitions(hconfigure-b PUBLIC USE_JSON_FILE_COMPRESSION)

    endif ()

    if (UNIX)
        target_compile_options(hconfigure-b PRIVATE -fvisibility=hidden)
    endif ()

    add_executable(BuildHelper ${HelperSource})
    target_link_libraries(BuildHelper PRIVATE hconfigure-b)

endif ()


# TODO
# Following is not needed on Linux, I guess.
if (UNIX)
    target_compile_options(fmt PRIVATE -fPIC)
endif ()

if ((${INCLUDE_CONFIGURE} MATCHES ON) AND (${INCLUDE_BUILD} MATCHES ON))


    add_executable(hhelper hhelper/src/main.cpp)
    target_link_libraries(hhelper PRIVATE hconfigure-c)

    target_compile_definitions(hhelper PRIVATE THIRD_PARTY_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/")
    target_compile_definitions(hhelper PRIVATE JSON_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/json/include/")
    target_compile_definitions(hhelper PRIVATE RAPIDJSON_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/rapidjson/include/")
    target_compile_definitions(hhelper PRIVATE PARALLEL_HASHMAP="${CMAKE_SOURCE_DIR}/3rdParty/parallel-hashmap/parallel_hashmap")
    target_compile_definitions(hhelper PRIVATE LZ4_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/lz4/lib")
    target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER="${CMAKE_SOURCE_DIR}/hconfigure/header/")
    target_compile_definitions(hhelper PRIVATE HCONFIGURE_C_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:hconfigure-c>")
    target_compile_definitions(hhelper PRIVATE HCONFIGURE_C_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure-c>")
    target_compile_definitions(hhelper PRIVATE HCONFIGURE_B_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure-b>")
    target_compile_definitions(hhelper PRIVATE HCONFIGURE_B_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:hconfigure-b>")
    target_compile_definitions(hhelper PRIVATE FMT_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/fmt/include/")
    target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:fmt::fmt>")
    target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_PATH="$<TARGET_FILE:fmt>")

    if ("${HMAKE_USE_THREAD_SANITIZER}" MATCHES ON)
        target_compile_definitions(hhelper PRIVATE USE_THREAD_SANITIZER)
    endif ()

    add_executable(hbuild hbuild/src/main.cpp)
    target_link_libraries(hbuild PRIVATE hconfigure-c)


    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(googletest)

    add_executable(Tests Tests/main.cpp
            Tests/ExamplesTestHelper.cpp
            Tests/Test_Examples.cpp
            Tests/Test_hconfigure.cpp
            Tests/StageTests.cpp
            Tests/Snapshot.cpp
            Tests/IgnoreHeaderDeps.cpp)
    target_link_libraries(Tests PRIVATE gtest gtest_main)
    target_include_directories(Tests PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
    target_compile_definitions(Tests PRIVATE SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}/")
    target_link_libraries(Tests PRIVATE hconfigure-b)
    target_link_libraries(Tests PRIVATE hconfigure-c)

    add_dependencies(Tests hhelper)
    add_dependencies(Tests hbuild)

    add_executable(hwrite hwrite/main.cpp)
    target_link_libraries(hwrite PRIVATE hconfigure-c)

endif ()

add_executable(copy copy.cpp)
target_link_libraries(copy PRIVATE hconfigure-c)

add_executable(benchmark Examples/benchmark/benchmark.cpp)


function(add_dual_executables)
    math(EXPR count "${ARGC} / 2")
    foreach (i RANGE 0 ${count})
        math(EXPR idx_name "${i} * 2")
        math(EXPR idx_src "${idx_name} + 1")

        if (idx_src LESS ARGC)
            list(GET ARGV ${idx_name} base_name)
            list(GET ARGV ${idx_src} src_file)

            add_executable("${base_name}Config" "${src_file}")
            target_link_libraries("${base_name}Config" PRIVATE hconfigure-c)

            add_executable("${base_name}Build" "${src_file}")
            target_link_libraries("${base_name}Build" PRIVATE hconfigure-b)
        endif ()
    endforeach ()
endfunction()

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
        Example-A1 Examples/Example-A1/hmake.cpp
        Example-A2 Examples/Example-A2/hmake.cpp
        Example-A3 Examples/Example-A3/hmake.cpp
        Example-A4 Examples/Example-A4/hmake.cpp
        Example-A5 Examples/Example-A5/hmake.cpp
        Example-A6 Examples/Example-A6/hmake.cpp
        Test1 Tests/Stage/Test1/hmake.cpp
        Test2 Tests/Stage/Test2/hmake.cpp
        Test3 Tests/Stage/Test3/hmake.cpp
        Boost Projects/Boost/hmake.cpp)

# TODO
# https://rapidjson.org/group___r_a_p_i_d_j_s_o_n___c_o_n_f_i_g.html
# macros to be looked for simd from above and whether that they are defined