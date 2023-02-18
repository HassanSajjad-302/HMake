cmake_minimum_required(VERSION 3.21.0)

project(HMake LANGUAGES CXX)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_compile_options(-fsanitize=thread -std=c++2b)
  add_link_options(-fsanitize=thread -fno-omit-frame-pointer)
else ()
  message(${CMAKE_CXX_COMPILER_ID})
  set(CMAKE_CXX_STANDARD 23)
endif ()

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 0)

add_subdirectory(fmt)
add_subdirectory(json)

add_library(hconfigure STATIC
        hconfigure/src/Settings.cpp
        hconfigure/src/Features.cpp
        hconfigure/src/TargetType.cpp
        hconfigure/src/BasicTargets.cpp
        hconfigure/src/Builder.cpp
        hconfigure/src/BuildSystemFunctions.cpp
        hconfigure/src/Cache.cpp
        hconfigure/src/SMFile.cpp
        hconfigure/src/Configuration.cpp
        hconfigure/src/CppSourceTarget.cpp
        hconfigure/src/LinkOrArchiveTarget.cpp
        hconfigure/src/BuildTools.cpp
        hconfigure/src/ToolsCache.cpp
        hconfigure/src/PostBasic.cpp
        hconfigure/src/Utilities.cpp
        hconfigure/src/GetTarget.cpp
        hconfigure/src/ConfigType.cpp)

target_link_libraries(hconfigure PUBLIC fmt::fmt nlohmann_json::nlohmann_json)
target_include_directories(hconfigure
                PUBLIC hconfigure/header
        )

add_executable(htools htools/main.cpp)
target_link_libraries(htools PRIVATE hconfigure)

add_executable(hhelper hhelper/src/main.cpp)
target_link_libraries(hhelper PRIVATE hconfigure)
target_compile_definitions(hhelper PRIVATE JSON_HEADER= "${CMAKE_SOURCE_DIR}/json/include/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER= "${CMAKE_SOURCE_DIR}/hconfigure/header/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_DIRECTORY= "$<TARGET_FILE_DIR:hconfigure>")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_PATH= "$<TARGET_FILE:hconfigure>")
target_compile_definitions(hhelper PRIVATE FMT_HEADER= "${CMAKE_SOURCE_DIR}/fmt/include/")
target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_DIRECTORY= "$<TARGET_FILE_DIR:fmt::fmt>")
target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_PATH= "$<TARGET_FILE:fmt>")

add_executable(hbuild
        hbuild/src/main.cpp)
target_link_libraries(hbuild PRIVATE hconfigure)

#Following target helps in writing hmake.cpp file with ide setup. Otherwise unused
add_executable(HMakeHelper Examples/Example11/hmake.cpp)
target_link_libraries(HMakeHelper PRIVATE hconfigure)


set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

add_executable(Tests Tests/main.cpp Tests/ExamplesTestHelper.cpp Tests/Test_Examples.cpp Tests/Test_hconfigure.cpp)
target_link_libraries(Tests PRIVATE gtest gtest_main nlohmann_json::nlohmann_json fmt::fmt)
target_include_directories(Tests PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_compile_definitions(Tests PRIVATE SOURCE_DIRECTORY= "${CMAKE_SOURCE_DIR}/")

add_dependencies(Tests hhelper)
add_dependencies(Tests hbuild)
