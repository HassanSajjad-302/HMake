

#include "BProject.hpp"
void BProject::parseProjectFileJsonAndInitializeBProjectStaticVariables(Json &projectFileJson) {
  BProject::compilerPath = projectFileJson.at("COMPILER").get<Json>().at("PATH").get<std::string>();
  BProject::linkerPath = projectFileJson.at("LINKER").get<Json>().at("PATH").get<std::string>();
  BProject::compilerFlags = projectFileJson.at("COMPILER_FLAGS").get<std::string>();
  BProject::linkerFlags = projectFileJson.at("LINKER_FLAGS").get<std::string>();
  std::string libraryTypeString = projectFileJson.at("LIBRARY_TYPE").get<std::string>();
  LibraryType fileLibraryType;
  if (libraryTypeString == "STATIC") {
    fileLibraryType = LibraryType::STATIC;
  } else if (libraryTypeString == "SHARED") {
    fileLibraryType = LibraryType::SHARED;
  } else {
    throw std::runtime_error(libraryTypeString + " is not in accepted values for LIBRARY_TYPE");
  }
  BProject::libraryType = fileLibraryType;
}
