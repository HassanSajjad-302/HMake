#include <iostream>
#include <string>
#include "instructionsFileMain.hpp"
#include "libraryDependency.hpp"

namespace fs = std::filesystem;
int main()
{
    //Example 1
    executable HMake("HMake", file("main.cpp"));
    for (auto x : fs::directory_iterator("src"))
    {
        HMake.sourceFiles.emplace_back(file(x));
    }
    directory headerDirectory("header");
    IDD headerFolderDependency{headerDirectory, dependencyType::PUBLIC};
    HMake.includeDirectoryDependencies.emplace_back(headerFolderDependency);
    HMake.setRunTimeOutputDirectory(directory("/home/hassan/Projects/HMake"));
    //This is the CMakeLists.txt file
    instructionsFileMain mainFile(HMake, projectDetails(CmakeVersion(), "HMake"));
    std::cout<<mainFile.getScript();




    //Exmaple 2

    //When I executed the build from following commented code in the source directory of my other project. It generated
    //the CMakeLists.txt file which quiet matched the functionality of the CMakeLists.txt file of that project.
    //I am here pasting the output of that case:

    /*
     *cmake_minimum_required(VERSION 3.18.0)

project(GetAway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(GetAway-lib  STATIC
GetAway/src/clientBluff.cpp
GetAway/src/resourceStrings.cpp
GetAway/src/clientChat.cpp
GetAway/src/deckSuit.cpp
GetAway/src/serverDikaiti.cpp
GetAway/src/serverChat.cpp
GetAway/src/serverGetAway.cpp
GetAway/src/clientDikaiti.cpp
GetAway/src/sati.cpp
GetAway/src/serverLobby.cpp
GetAway/src/serverListener.cpp
GetAway/src/clientLobbyPF.cpp
GetAway/src/getAwayPData.cpp
GetAway/src/satiAndroid.cpp
GetAway/src/clientGetAwayPF.cpp
GetAway/src/home.cpp
GetAway/src/serverBluff.cpp
GetAway/src/homePF.cpp
GetAway/src/serverLobbyPF.cpp
GetAway/src/clientChatPF.cpp
GetAway/src/replacementStreamBuff.cpp
GetAway/src/clientBluffPF.cpp
GetAway/src/clientLobby.cpp
GetAway/src/serverListenerPF.cpp
GetAway/src/clientGetAway.cpp
GetAway/src/constants.cpp
GetAway/src/androidMain.cpp
GetAway/src/bluffPData.cpp
)

target_include_directories(GetAway-lib
PUBLIC GetAway/header
PUBLIC asio/asio/include
PUBLIC spdlog/include
)

target_compile_definitions(GetAway-lib
PUBLIC ASIO_STANDALONE)

add_executable(GetAway
GetAway/main.cpp
)

target_link_libraries(GetAway
PUBLIC GetAway-lib)
     */


   /* library GetAway_lib("GetAway-lib", libraryType::STATIC);
    for (auto x : fs::directory_iterator("GetAway/src"))
    {
        GetAway_lib.sourceFiles.emplace_back(file(x));
    }
    GetAway_lib.compileDefinitionDependencies.emplace_back(compileDefinitionDependency{"ASIO_STANDALONE",
                                                                                       dependencyType::PUBLIC});
    GetAway_lib.includeDirectoryDependencies.emplace_back(IDD{directory("GetAway/header"), dependencyType::PUBLIC});
    GetAway_lib.includeDirectoryDependencies.emplace_back(IDD{directory("asio/asio/include"), dependencyType::PUBLIC});
    GetAway_lib.includeDirectoryDependencies.emplace_back(IDD{directory("spdlog/include"), dependencyType::PUBLIC});

    executable GetAway("GetAway", file("GetAway/main.cpp"));
    GetAway.libraryDependencies.emplace_back(GetAway_lib);

    //This is CMakeLists.txt file that will be generated.
    instructionsFileMain mainFile(GetAway, projectDetails(CmakeVersion(3,18), "GetAway"));
    std::cout<<mainFile.getScript();*/
}
