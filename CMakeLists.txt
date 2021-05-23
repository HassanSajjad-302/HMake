cmake_minimum_required(VERSION 3.0.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hconfigure
        hconfigure/src/Executable.cpp
        hconfigure/src/File.cpp
        hconfigure/src/Directory.cpp
        hconfigure/src/CxxStandard.cpp
        hconfigure/src/LibraryDependency.cpp
        hconfigure/src/Library.cpp
        hconfigure/src/BinaryDirExportFile.cpp
        hconfigure/src/BinaryDirImportFile.cpp
        hconfigure/src/TargetImportedFromFile.cpp
        hconfigure/src/CustomFunctions.cpp
        hconfigure/src/CommanVariables.cpp
        hconfigure/src/Initialize.cpp
        hconfigure/src/Configure.cpp
        hconfigure/src/Project.cpp
        hconfigure/src/DependencyType.cpp
        hconfigure/src/Flags.cpp
        hconfigure/src/Family.cpp
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