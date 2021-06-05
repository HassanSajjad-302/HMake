#include "Executable.hpp"
#include "HConfigureCustomFunctions.hpp"
#include "Project.hpp"
#include <stack>

Executable::Executable(std::string targetName_, File file) : targetName(std::move(targetName_)), configureDirectory(Project::CONFIGURE_DIRECTORY.path),
                                                             buildDirectoryPath(Project::CONFIGURE_DIRECTORY.path) {
  sourceFiles.emplace_back(std::move(file));
}

Executable::Executable(std::string targetName_, File file, fs::path configureDirectoryPathRelativeToProjectBuildPath) : targetName(std::move(targetName_)) {
  auto targetConfigureDirectoryPath =
      Project::CONFIGURE_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
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

void to_json(Json &j, const Executable &executable) {
  j["PROJECT_FILE_PATH"] = Project::CONFIGURE_DIRECTORY.path.string() + Project::PROJECT_NAME + ".hmake";
  j["BUILD_DIRECTORY"] = executable.buildDirectoryPath.string();
  std::vector<std::string> sourceFilesArray;
  for (const auto &e : executable.sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  jsonAssignSpecialist("SOURCE_FILES", j, sourceFilesArray);
  //library dependencies

  std::vector<std::string> includeDirectories;
  for (const auto &e : executable.includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path.string());
  }

  std::string compilerFlags;
  for (const auto &e : executable.compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }

  std::string linkerFlags;
  for (const auto &e : executable.linkerFlagsDependencies) {
    compilerFlags.append(" " + e.linkerFlags + " ");
  }

  Json compileDefinitionsArray;
  for (const auto &e : executable.compileDefinitionDependencies) {
    Json compileDefinitionObject;
    compileDefinitionObject["NAME"] = e.compileDefinition.name;
    compileDefinitionObject["VALUE"] = e.compileDefinition.value;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  //This adds first layer of dependencies as is but next layers are added only if they are public.
  std::vector<const Library *> dependencies;
  for (const auto &l : executable.libraryDependencies) {
    std::stack<const Library *> st;
    st.push(&(l.library));
    while (!st.empty()) {
      auto obj = st.top();
      st.pop();
      dependencies.push_back(obj);
      for (const auto &i : obj->libraryDependencies) {
        if (i.libraryDependencyType == DependencyType::PUBLIC) {
          st.push(&(i.library));
        }
      }
    }
  }

  std::vector<std::string> libraryDependencies;
  for (auto i : dependencies) {
    for (const auto &e : i->includeDirectoryDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        std::string str = e.includeDirectory.path.string();
        includeDirectories.push_back(str);
      }
    }

    for (const auto &e : i->compilerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.compilerFlags + " ");
      }
    }

    for (const auto &e : i->linkerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.linkerFlags + " ");
      }
    }

    for (const auto &e : i->compileDefinitionDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        Json compileDefinitionObject;
        compileDefinitionObject["NAME"] = e.compileDefinition.name;
        compileDefinitionObject["VALUE"] = e.compileDefinition.value;
        compileDefinitionsArray.push_back(compileDefinitionObject);
      }
    }
    libraryDependencies.emplace_back(i->configureDirectory.path.string() + i->targetName + "." + [=]() {
      if (i->libraryType == LibraryType::STATIC) {
        return "static";
      } else {
        return "shared";
      }
    }() + "." + "hmake");
  }

  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", j, libraryDependencies);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", j, includeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", j, compilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", j, linkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", j, compileDefinitionsArray);
}
