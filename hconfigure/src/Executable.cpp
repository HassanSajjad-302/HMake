#include "Executable.hpp"
#include "Project.hpp"
#include <stack>

Executable::Executable(std::string targetName_, File file) : targetName(std::move(targetName_)), configureDirectory(Project::BUILD_DIRECTORY.path),
                                                             buildDirectoryPath(Project::BUILD_DIRECTORY.path) {
  sourceFiles.emplace_back(std::move(file));
}

Executable::Executable(std::string targetName_, File file, fs::path configureDirectoryPathRelativeToProjectBuildPath) : targetName(std::move(targetName_)) {
  auto targetConfigureDirectoryPath =
      Project::BUILD_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
  fs::create_directory(targetConfigureDirectoryPath);
  configureDirectory = Directory(targetConfigureDirectoryPath);
  buildDirectoryPath = targetConfigureDirectoryPath;
  sourceFiles.emplace_back(std::move(file));
}

//This will imply that directory already exists. While in the above constructor directory will be built while building
//the project.
Executable::Executable(std::string targetName_, File file, Directory configureDirectory_) : targetName(std::move(targetName_)), buildDirectoryPath(configureDirectory_.path),
                                                                                            configureDirectory(std::move(configureDirectory_)) {
  sourceFiles.emplace_back(std::move(file));
}

void to_json(Json &j, const Executable &p) {
  j["PROJECT_FILE_PATH"] = Project::BUILD_DIRECTORY.path.string() + Project::PROJECT_NAME + ".hmake";
  j["NAME"] = p.targetName;
  j["BUILD_DIRECTORY"] = p.buildDirectoryPath.string();
  Json sourceFilesArray;
  for (const auto &e : p.sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  j["SOURCE_FILES"] = sourceFilesArray;
  //library dependencies

  Json includeDirectories;
  for (const auto &e : p.includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path.string());
  }

  std::string compilerFlags;
  for (const auto &e : p.compilerFlagDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }

  Json compileDefinitionsArray;
  for (const auto &e : p.compileDefinitionDependencies) {
    Json compileDefinitionObject;
    compileDefinitionObject["NAME"] = e.compileDefinition;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  //This adds first layer of dependencies as is but next layers are added only if they are public.
  std::vector<Library *> dependencies;
  for (auto l : p.libraryDependencies) {
    std::stack<Library *> st;
    st.push(&(l.library));
    while (!st.empty()) {
      auto obj = st.top();
      dependencies.push_back(obj);
      for (auto i : obj->libraryDependencies) {
        if (i.libraryDependencyType == DependencyType::PUBLIC) {
          dependencies.push_back(&(i.library));
        }
      }
      st.pop();
    }
  }

  for (auto i : dependencies) {
    for (const auto &E : i->includeDirectoryDependencies) {
      includeDirectories.push_back(E.includeDirectory.path.string());
    }

    for (const auto &e : i->compilerFlagsDependencies) {
      compilerFlags.append(" " + e.compilerFlags + " ");
    }

    for (const auto &E : i->compileDefinitionDependencies) {
      Json compileDefinitionObject;
      compileDefinitionObject["NAME"] = E.compileDefinition;
      compileDefinitionsArray.push_back(compileDefinitionObject);
    }
  }

  j["INCLUDE_DIRECTORIES"] = includeDirectories;
  if (compilerFlags.empty()) {
    j["COMPILER_TRANSITIVE_FLAGS"] = Json();
  } else {
    j["COMPILER_TRANSITIVE_FLAGS"] = compilerFlags;
  }
  j["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
}
