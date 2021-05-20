

#include "project.hpp"

#include <utility>
project::project(std::string projectName, projectVersion version) {
    project::PROJECT_NAME = std::move(projectName);
    project::PROJECT_VERSION = version;
}

void to_json(json &j, const projectVersion &p) {
    j = std::to_string(p.majorVersion) + "." + std::to_string(p.minorVersion) + "." + std::to_string(p.patchVersion);
}

void to_json(json &j, const configurationType &p) {
    if(p == configurationType::DEBUG){
        j = "DEBUG";
        j = "RELEASE";
    }
}

void to_json(json& j, const project& p) {
    j["PROJECT_NAME"] = project::PROJECT_NAME;
    j["PROJECT_VERSION"] = project::PROJECT_VERSION;
    j["CXX_COMPILER"] = project::CXX_COMPILER;
    j["SOURCE_DIRECTORY"] = project::SOURCE_DIRECTORY.path.string();
    j["BUILD_DIRECTORY"] = project::BUILD_DIRECTORY.path.string();
    json exeArray = json::array();
    for(auto e: p.projectExecutables){
        json exeObject;
        exeObject["NAME"] = e.targetName;
        exeObject["LOCATION"] = e.configureDirectory.path.string();
        exeArray.push_back(exeObject);
    }
    j["EXECUTABLES"] = exeArray;
    j["CONFIGURATION"] = project::projectConfigurationType;
    j["FLAGS"]["CXX_COMPILER"]["DEBUG"] = "-g";
    j["FLAGS"]["CXX_COMPILER"]["RELEASE"] = "-g";
    j["FLAGS"]["CXX_COMPILER"]["RELWITHDEBUGINFO"] = "-g";
    j["FLAGS"]["CXX_COMPILER"]["MINSIZEREL"] = "-g";
    j["FLAGS"]["LINKER"]["EXECUTABLE"]["DEBUG"] = "-g";
    j["FLAGS"]["LINKER"]["EXECUTABLE"]["RELEASE"] = "-g";
    j["FLAGS"]["LINKER"]["EXECUTABLE"]["RELWITHDEBUGINFO"] = "-g";
    j["FLAGS"]["LINKER"]["EXECUTABLE"]["MINSIZERELEASE"] = "-g";
    j["FLAGS"]["LINKER"]["SHARED"]["DEBUG"] = "-g";
    j["FLAGS"]["LINKER"]["SHARED"]["RELEASE"] = "-g";
    j["FLAGS"]["LINKER"]["SHARED"]["RELWITHDEBUGINFO"] = "-g";
    j["FLAGS"]["LINKER"]["SHARED"]["MINSIZERELEASE"] = "-g";
    j["FLAGS"]["LINKER"]["STATIC"]["DEBUG"] = "-g";
    j["FLAGS"]["LINKER"]["STATIC"]["RELEASE"] = "-g";
    j["FLAGS"]["LINKER"]["STATIC"]["RELWITHDEBUGINFO"] = "-g";
    j["FLAGS"]["LINKER"]["STATIC"]["MINSIZERELEASE"] = "-g";
}