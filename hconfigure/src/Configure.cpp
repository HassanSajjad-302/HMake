
#include "Configure.hpp"
#include "fstream"

void configure(const Library &library) {
  Json json;
  json = library;
  fs::path p = library.configureDirectory.path;
  std::string fileName;
  if (library.libraryType == LibraryType::STATIC) {
    fileName = library.targetName + ".static.hmake";
  } else {
    fileName = library.targetName + ".shared.hmake";
  }
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &l : library.libraryDependencies) {
    configure(l.library);
  }
}

void configure(const Executable &executable) {
  Json json;
  json = executable;
  fs::path p = executable.configureDirectory.path;
  std::string fileName = executable.targetName + ".executable.hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (const auto &l : executable.libraryDependencies) {
    configure(l.library);
  }
}

void configure() {
  Json json = Project();
  fs::path p = Project::CONFIGURE_DIRECTORY.path;
  std::string fileName = Project::PROJECT_NAME + ".hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &t : Project::projectExecutables) {
    configure(t);
  }
  for (auto &t : Project::projectLibraries) {
    configure(t);
  }
}

void configure(const Package &package) {
  //Check that no two JSON'S of packageVariants of package are same
  std::set<nlohmann::json> checkSet;
  for (auto &i : package.packageVariants) {
    //This associatedJson is ordered_json, thus two different json's equality test will be false if the order is different even if they have same elements.
    //Thus we first convert it into json normal which is unordered one.
    nlohmann::json j = i.associtedJson;
    if (auto [pos, ok] = checkSet.emplace(j); !ok) {
      throw std::logic_error("No two json's of packagevariants can be same. Different order with same values does not make two Json's different");
    }
  }

  int count = 1;
  Json packageFileJson;
  for (auto &i : package.packageVariants) {
    packageFileJson.emplace_back(i.associtedJson);

    fs::path variantDirectoryPath = package.packageConfigureDirectory.path / std::to_string(count);
  }

  fs::path file = package.packageConfigureDirectory.path / "package.hmake";
  std::ofstream(file) << packageFileJson;
}
