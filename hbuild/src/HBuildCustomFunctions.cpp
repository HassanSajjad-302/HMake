#include "HBuildCustomFunctions.hpp"
#include "BBuildTarget.hpp"
#include "BProject.hpp"
#include "fstream"

void build::parseProjectFileAdStartBuildingTarget(const fs::path &targetFilePath) {
  Json json;
  std::ifstream(targetFilePath) >> json;

  BProject::projectFilePath = json.at("PROJECT_FILE_PATH").get<std::string>();
  Json projectFileJson;
  std::ifstream(BProject::projectFilePath) >> projectFileJson;
  BProject::parseProjectFileJsonAndInitializeBProjectStaticVariables(projectFileJson);

  BBuildTarget buildTarget(targetFilePath, json);
  buildTarget.build();
}

void build::doJsonSpecialParseAndConvertStringArrayToPathArray(const std::string &jstr, Json &j, std::vector<fs::path> &container) {
  if (!j.contains(jstr)) {
    return;
  } else {
    if (j.at(jstr).size() == 1) {
      std::string str = j.at(jstr).get<std::string>();
      container.emplace_back(fs::path(str));
      return;
    }
    std::vector<std::string> tmpContainer;
    j.at(jstr).get_to(tmpContainer);
    for (auto &i : tmpContainer) {
      container.emplace_back(fs::path(i));
    }
  }
}
