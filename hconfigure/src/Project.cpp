

#include "Project.hpp"

#include <HConfigureCustomFunctions.hpp>
#include <utility>

void to_json(Json &j, const ConfigType &p) {
  if (p == ConfigType::DEBUG) {
    j = "DEBUG";
    j = "RELEASE";
  }
}

void to_json(Json &j, const Project &p) {
  j["PROJECT_NAME"] = Project::PROJECT_NAME;
  j["PROJECT_VERSION"] = Project::PROJECT_VERSION;
  j["SOURCE_DIRECTORY"] = Project::SOURCE_DIRECTORY.path.string();
  j["BUILD_DIRECTORY"] = Project::CONFIGURE_DIRECTORY.path.string();
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