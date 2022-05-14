cmake_minimum_required(VERSION 3.18.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(hhelper hhelper/src/main.cpp)
target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER="${CMAKE_SOURCE_DIR}/hconfigure/header/")
target_compile_definitions(hhelper PRIVATE JSON_HEADER="${CMAKE_SOURCE_DIR}/json/include/")
target_compile_definitions(hhelper PRIVATE FMT_HEADER="${CMAKE_SOURCE_DIR}/fmt/include/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_DIRECTORY="${CMAKE_BINARY_DIR}/")


add_library(hconfigure STATIC
        hconfigure/src/Configure.cpp
        )
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure>")

target_include_directories(hconfigure
        PUBLIC hconfigure/header
        PUBLIC cxxopts/include
        PUBLIC fmt/include/
        )
target_link_libraries(hhelper PRIVATE hconfigure)

add_executable(hbuild
        hbuild/src/BBuild.cpp
        hbuild/src/main.cpp)
target_include_directories(hbuild PRIVATE hbuild/header hconfigure/header)

add_executable(UnBuiltTarget Examples/Example4/hmake.cpp)
target_include_directories(UnBuiltTarget PRIVATE hconfigure/header)
add_subdirectory(json)
target_link_libraries(hconfigure PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(hbuild PRIVATE nlohmann_json::nlohmann_json hconfigure)
target_link_libraries(hhelper PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(UnBuiltTarget PRIVATE nlohmann_json::nlohmann_json hconfigure)
add_dependencies(UnBuiltTarget hconfigure)


set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

add_executable(Test_Correctness Tests/main.cpp Tests/TestHelper.cpp)
target_include_directories(Test_Correctness PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_link_libraries(Test_Correctness PRIVATE gtest gtest_main)
target_compile_definitions(Test_Correctness PRIVATE TESTDIRECTORY="${CMAKE_SOURCE_DIR}/Tests")

add_dependencies(Test_Correctness hhelper)
add_dependencies(Test_Correctness hconfigure)
add_dependencies(Test_Correctness hbuild)

add_subdirectory(fmt)

target_link_libraries(Test_Correctness PRIVATE fmt::fmt)
target_link_libraries(hbuild PRIVATE fmt::fmt)
target_link_libraries(hhelper PRIVATE fmt::fmt)
set(REPROC_DEVELOP OFF)
set(REPROC++ ON)

# Just For Quick Testing Purposes.
add_executable(Testing Testing/main.cpp)
target_link_libraries(Testing PRIVATE fmt::fmt)

