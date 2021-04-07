cmake_minimum_required(VERSION 3.0.0)

project(HMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(HMake
        main.cpp
        src/executable.cpp
        src/projectdetails.cpp
        src/file.cpp
        src/sourcefiles.cpp
        src/directory.cpp
        src/instructionsFileMain.cpp
        src/cxx_standard.cpp
        src/cxxStandard.cpp
        src/libraryDependency.cpp
        src/library.cpp
        )

target_include_directories(HMake
        PUBLIC header
        )

set_target_properties(HMake PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY /home/hassan/Projects/HMake)