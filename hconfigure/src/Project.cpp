

#include "Project.hpp"

#include <HConfigureCustomFunctions.hpp>
#include <utility>
Project::Project(std::string projectName, projectVersion version) {
  Project::PROJECT_NAME = std::move(projectName);
  Project::PROJECT_VERSION = version;
}

void to_json(Json &j, const projectVersion &p) {
  j = std::to_string(p.majorVersion) + "." + std::to_string(p.minorVersion) + "." + std::to_string(p.patchVersion);
}

void to_json(Json &j, const CONFIG_TYPE &p) {
  if (p == CONFIG_TYPE::DEBUG) {
    j = "DEBUG";
    j = "RELEASE";
  }
}

void to_json(Json &j, const Project &p) {
  j["PROJECT_NAME"] = Project::PROJECT_NAME;
  j["PROJECT_VERSION"] = Project::PROJECT_VERSION;
  j["SOURCE_DIRECTORY"] = Project::SOURCE_DIRECTORY.path.string();
  j["BUILD_DIRECTORY"] = Project::BUILD_DIRECTORY.path.string();
  j["CONFIGURATION"] = Project::projectConfigurationType;
  j["COMPILER"] = Project::ourCompiler;
  j["LINKER"] = Project::ourLinker;
  std::string compilerFlags = Project::flags[Project::ourCompiler.compilerFamily][Project::projectConfigurationType];
  j["COMPILER_FLAGS"] = compilerFlags;
  std::string linkerFlags = Project::flags[Project::ourLinker.linkerFamily][Project::projectConfigurationType];
  j["LINKER_FLAGS"] = linkerFlags;

  Json exeArray = Json::array();
  for (const auto &e : Project::projectExecutables) {
    Json exeObject;
    exeObject["NAME"] = e.targetName;
    exeObject["LOCATION"] = e.configureDirectory.path.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("EXECUTABLES", j, exeArray);

  Json libArray = Json::array();
  for (const auto &e : Project::projectLibraries) {
    Json exeObject;
    exeObject["NAME"] = e.targetName;
    exeObject["LOCATION"] = e.configureDirectory.path.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("LIBRARIES", j, libArray);
  j["LIBRARY_TYPE"] = Project::libraryType;
}