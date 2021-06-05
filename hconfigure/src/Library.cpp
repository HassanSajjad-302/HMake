
#include "Library.hpp"
#include "Project.hpp"
#include "stack"
#include <HConfigureCustomFunctions.hpp>
Library::Library(std::string targetName_) : libraryType(Project::libraryType), targetName(std::move(targetName_)), configureDirectory(Project::CONFIGURE_DIRECTORY.path),
                                            buildDirectoryPath(Project::CONFIGURE_DIRECTORY.path) {
}

Library::Library(std::string targetName_, fs::path configureDirectoryPathRelativeToProjectBuildPath) : targetName(std::move(targetName_)), libraryType(Project::libraryType) {
  auto targetConfigureDirectoryPath =
      Project::CONFIGURE_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
  fs::create_directory(targetConfigureDirectoryPath);
  configureDirectory = Directory(targetConfigureDirectoryPath);
  buildDirectoryPath = targetConfigureDirectoryPath;
}

//This will imply that directory already exists. While in the above constructor directory will be built while building
//the project.
Library::Library(std::string targetName_, Directory configureDirectory_) : libraryType(Project::libraryType), targetName(std::move(targetName_)), buildDirectoryPath(configureDirectory_.path),
                                                                           configureDirectory(std::move(configureDirectory_)) {
}

void to_json(Json &j, const LibraryType &p) {
  if (p == LibraryType::STATIC) {
    j = "STATIC";
  } else {
    j = "SHARED";
  }
}

//todo: don't give property values if they are not different from project property values.
//todo: improve property values in compile definitions.
//todo: add transitive linker flags
void to_json(Json &j, const Library &library) {
  j["PROJECT_FILE_PATH"] = Project::CONFIGURE_DIRECTORY.path.string() + Project::PROJECT_NAME + ".hmake";
  j["BUILD_DIRECTORY"] = library.buildDirectoryPath.string();
  std::vector<std::string> sourceFilesArray;
  for (const auto &e : library.sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  jsonAssignSpecialist("SOURCE_FILES", j, sourceFilesArray);
  //library dependencies

  std::vector<std::string> includeDirectories;
  for (const auto &e : library.includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path.string());
  }

  std::string compilerFlags;
  for (const auto &e : library.compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }

  std::string linkerFlags;
  for (const auto &e : library.linkerFlagsDependencies) {
    compilerFlags.append(" " + e.linkerFlags + " ");
  }

  Json compileDefinitionsArray;
  for (const auto &e : library.compileDefinitionDependencies) {
    Json compileDefinitionObject;
    compileDefinitionObject["NAME"] = e.compileDefinition.name;
    compileDefinitionObject["VALUE"] = e.compileDefinition.value;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  //This adds first layer of dependencies as is but next layers are added only if they are public.
  std::vector<const Library *> dependencies;
  for (const auto &l : library.libraryDependencies) {
    std::stack<const Library *> st;
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
