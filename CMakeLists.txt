cmake_minimum_required(VERSION 3.0.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hhelper hhelper/src/main.cpp)
target_include_directories(hhelper PRIVATE hhelper/src)
target_compile_definitions(hhelper PRIVATE HCONFIGURE_HEADER="${CMAKE_SOURCE_DIR}/hconfigure/header/")
target_compile_definitions(hhelper PRIVATE JSON_HEADER="${CMAKE_SOURCE_DIR}/json/include/")
target_compile_definitions(hhelper PRIVATE HCONFIGURE_STATIC_LIB_PATH="${CMAKE_BINARY_DIR}/")


add_library(hconfigure
        hconfigure/src/Configure.cpp
        )

target_include_directories(hconfigure
        PUBLIC hconfigure/header
        PUBLIC cxxopts/include
        )

add_executable(hbuild
        hbuild/src/BBuild.cpp
        hbuild/src/main.cpp)
target_include_directories(hbuild PUBLIC hbuild/header)
target_include_directories(hbuild PRIVATE hconfigure/header)

add_executable(UnBuiltTarget Examples/Example8/hmake.cpp)
target_include_directories(UnBuiltTarget PRIVATE hconfigure/header)
add_subdirectory(json)
target_link_libraries(hconfigure PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(hbuild PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(hhelper PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(UnBuiltTarget PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(UnBuiltTarget PRIVATE hconfigure)
add_dependencies(UnBuiltTarget hconfigure)