cmake_minimum_required(VERSION 3.21.0)

project(HMake LANGUAGES CXX)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GCC")

    add_compile_options(-fsanitize=thread -std=c++2b)
    add_link_options(-fsanitize=thread -fno-omit-frame-pointer)

    add_compile_options(-std=c++2b)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_compile_options(/fsanitize=address)
    endif ()
    set(CMAKE_CXX_STANDARD 23)
endif ()

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 0)

add_subdirectory(3rdParty/fmt)
add_subdirectory(3rdParty/json)

add_library(hconfigure STATIC
        hconfigure/src/Settings.cpp
        hconfigure/src/Features.cpp
        hconfigure/src/TargetType.cpp
        hconfigure/src/BTarget.cpp
        hconfigure/src/Builder.cpp
        hconfigure/src/BuildSystemFunctions.cpp
        hconfigure/src/Cache.cpp
        hconfigure/src/HashedCommand.cpp
        hconfigure/src/SMFile.cpp
        hconfigure/src/Configuration.cpp
        hconfigure/src/CppSourceTarget.cpp
        hconfigure/src/LinkOrArchiveTarget.cpp
        hconfigure/src/BuildTools.cpp
        hconfigure/src/ToolsCache.cpp
        hconfigure/src/PostBasic.cpp
        hconfigure/src/Utilities.cpp
        hconfigure/src/GetTarget.cpp
        hconfigure/src/ConfigType.cpp
        hconfigure/src/DSC.cpp
        hconfigure/src/ObjectFileProducer.cpp
        hconfigure/src/CSourceTarget.cpp
        hconfigure/src/DLLLoader.cpp
        hconfigure/src/Configure.cpp
        hconfigure/src/CTargetRoundZeroBTarget.cpp
        hconfigure/src/PrebuiltLinkOrArchiveTarget.cpp
        hconfigure/src/PrebuiltBasic.cpp
        hconfigure/src/PlatformSpecific.cpp
        hconfigure/src/Node.cpp)


if (UNIX)
    target_compile_options(hconfigure PRIVATE -fPIC)
    target_compile_options(fmt PRIVATE -fPIC)
endif ()

target_link_libraries(hconfigure PUBLIC fmt::fmt nlohmann_json::nlohmann_json)
target_include_directories(hconfigure
        PUBLIC hconfigure/header
        PUBLIC 3rdParty/rapidjson/include
        PUBLIC 3rdParty/rapidhash
        PUBLIC 3rdParty/parallel-hashmap/parallel_hashmap
)
# Comment out the following to switch from hash to compile-command in the build-log.
target_compile_definitions(hconfigure PUBLIC USE_COMMAND_HASH)

# Comment out the following to switch from nodes.json numbers reflected in build-cache.json to full normalized paths in build-cache.json
target_compile_definitions(hconfigure PUBLIC USE_NODES_CACHE_INDICES_IN_CACHE)

if (UNIX)
    target_compile_options(hconfigure PRIVATE -fvisibility=hidden)
endif ()

add_executable(htools htools/main.cpp)
target_link_libraries(htools PRIVATE hconfigure)

add_executable(hhelper hhelper/src/main.cpp)
target_link_libraries(hhelper PRIVATE hconfigure)
target_compile_definitions(hhelper PRIVATE JSON_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/json/include/")
target_compile_definitions(hhelper PRIVATE RAPIDJSON_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/rapidjson/include/")
target_compile_definitions(hhelper PRIVATE RAPIDHASH_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/rapidhash/")
target_compile_definitions(hhelper PRIVATE PARALLEL_HASHMAP="${CMAKE_SOURCE_DIR}/3rdParty/parallel-hashmap/parallel_hashmap")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER="${CMAKE_SOURCE_DIR}/hconfigure/header/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:hconfigure>")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure>")
target_compile_definitions(hhelper PRIVATE FMT_HEADER="${CMAKE_SOURCE_DIR}/3rdParty/fmt/include/")
target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:fmt::fmt>")
target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_PATH="$<TARGET_FILE:fmt>")

add_executable(hbuild hbuild/src/main.cpp)
target_link_libraries(hbuild PRIVATE hconfigure)

#Following target helps in writing hmake.cpp file with ide setup. Otherwise unused
add_executable(HMakeHelper Examples/Example9/hmake.cpp)
target_link_libraries(HMakeHelper PRIVATE hconfigure)
target_compile_definitions(HMakeHelper PRIVATE EXE)


set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

add_executable(Tests Tests/main.cpp
        Tests/ExamplesTestHelper.cpp
        Tests/Test_Examples.cpp
        Tests/Test_hconfigure.cpp
        Tests/StageTests.cpp
        Tests/Snapshot.cpp)
target_link_libraries(Tests PRIVATE gtest gtest_main)
target_include_directories(Tests PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_compile_definitions(Tests PRIVATE SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}/")
target_link_libraries(Tests PRIVATE hconfigure)

add_dependencies(Tests hhelper)
add_dependencies(Tests hbuild)

add_executable(hwrite hwrite/main.cpp)
target_link_libraries(hwrite PRIVATE hconfigure)

set_property(TARGET HMakeHelper PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")