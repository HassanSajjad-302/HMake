cmake_minimum_required(VERSION 3.0.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hconfigure
        hconfigure/src/executable.cpp
        hconfigure/src/file.cpp
        hconfigure/src/directory.cpp
        hconfigure/src/cxx_standard.cpp
        hconfigure/src/cxxStandard.cpp
        hconfigure/src/libraryDependency.cpp
        hconfigure/src/library.cpp
        hconfigure/src/binaryDirExportFile.cpp
        hconfigure/src/binaryDirImportFile.cpp
        hconfigure/src/targetImportedFromFile.cpp
        hconfigure/src/customFunctions.cpp
        hconfigure/src/commanVaribles.cpp
        hconfigure/src/initialize.cpp
        hconfigure/src/configure.cpp
        hconfigure/src/project.cpp
        hconfigure/src/dependencyType.cpp
        hconfigure/src/flags.cpp
        hconfigure/src/family.cpp
        )

target_include_directories(hconfigure
        PUBLIC hconfigure/header
        PUBLIC cxxopts/include
        )

add_library(hbuild
        hbuild/src/build.cpp)
target_include_directories(hbuild PUBLIC hbuild/header)

add_executable(HMake main.cpp)
target_link_libraries(HMake PRIVATE hconfigure)
target_link_libraries(HMake PRIVATE hbuild)
target_include_directories(HMake PRIVATE cxxopts/include)

add_executable(UnBuiltTarget TestingProject/hmake.cpp)
target_include_directories(UnBuiltTarget PRIVATE hconfigure/header)
add_subdirectory(json)
target_link_libraries(HMake PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(hconfigure PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(hbuild PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(UnBuiltTarget PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(UnBuiltTarget PRIVATE hconfigure)
add_dependencies(UnBuiltTarget hconfigure)