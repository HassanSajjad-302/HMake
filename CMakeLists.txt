cmake_minimum_required(VERSION 3.0.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(configure
        configure/src/executable.cpp
        configure/src/file.cpp
        configure/src/directory.cpp
        configure/src/cxx_standard.cpp
        configure/src/cxxStandard.cpp
        configure/src/libraryDependency.cpp
        configure/src/library.cpp
        configure/src/binaryDirExportFile.cpp
        configure/src/binaryDirImportFile.cpp
        configure/src/targetImportedFromFile.cpp
        configure/src/customFunctions.cpp
        configure/src/commanVaribles.cpp
        configure/src/initialize.cpp
        configure/src/configure.cpp
        configure/src/project.cpp
        configure/src/dependencyType.cpp
        )

target_include_directories(configure
        PUBLIC configure/header
        PUBLIC cxxopts/include
        )

add_executable(HMake main.cpp)
add_dependencies(HMake configure)

target_include_directories(HMake PRIVATE cxxopts/include)
set_target_properties(HMake PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY /home/hassan/Projects/HMake/TestingProject)

add_executable(UnBuiltTarget TestingProject/hmake.cpp)
target_include_directories(UnBuiltTarget PRIVATE configure/header)
add_subdirectory(json)
target_link_libraries(HMake PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(configure PRIVATE nlohmann_json::nlohmann_json)
