cmake_minimum_required(VERSION 3.21.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(hhelper hhelper/src/main.cpp)
target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER="${CMAKE_SOURCE_DIR}/hconfigure/header/")
target_compile_definitions(hhelper PRIVATE JSON_HEADER="${CMAKE_SOURCE_DIR}/json/include/")
target_compile_definitions(hhelper PRIVATE FMT_HEADER="${CMAKE_SOURCE_DIR}/fmt/include/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:hconfigure>")
target_compile_definitions(hhelper PRIVATE FMT_STATIC_LIB_DIRECTORY="$<TARGET_FILE_DIR:fmt>")


add_library(hconfigure STATIC
        hconfigure/src/Configure.cpp
        )
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_PATH="$<TARGET_FILE:hconfigure>")

target_include_directories(hconfigure
        PUBLIC hconfigure/header
        PUBLIC cxxopts/include
        PUBLIC fmt/include
        PUBLIC json/include
        )
target_link_libraries(hhelper PRIVATE hconfigure)

add_executable(hbuild
        hbuild/src/BBuild.cpp
        hbuild/src/main.cpp)
target_include_directories(hbuild PRIVATE hbuild/header)

#Following target helps in writing hmake.cpp file with ide setup. Otherwise unused
add_executable(HMakeCppHelper Projects/Boost/hmake.cpp)
target_include_directories(HMakeCppHelper PRIVATE hconfigure/header)
add_subdirectory(json)
target_link_libraries(hbuild PRIVATE hconfigure)
target_link_libraries(HMakeCppHelper PRIVATE hconfigure)


set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

add_executable(Tests Tests/main.cpp Tests/ExamplesTestHelper.cpp Tests/Test_Examples.cpp Tests/Test_hconfigure.cpp)
target_include_directories(Tests PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_link_libraries(Tests PRIVATE gtest gtest_main nlohmann_json::nlohmann_json)
target_compile_definitions(Tests PRIVATE SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}/")

add_dependencies(Tests hhelper)
add_dependencies(Tests hbuild)

add_subdirectory(fmt)

target_link_libraries(Tests PRIVATE fmt::fmt)
target_link_libraries(hbuild PRIVATE fmt::fmt)
target_link_libraries(hhelper PRIVATE fmt::fmt)
target_link_libraries(HMakeCppHelper PRIVATE fmt::fmt)
