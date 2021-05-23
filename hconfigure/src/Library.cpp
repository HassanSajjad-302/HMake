
#include "Library.hpp"
#include "Project.hpp"
#include "stack"
Library::Library(std::string targetName_) : libraryType(Project::libraryType), targetName(std::move(targetName_)), configureDirectory(Project::BUILD_DIRECTORY.path),
                                            buildDirectoryPath(Project::BUILD_DIRECTORY.path) {
}

Library::Library(std::string targetName_, fs::path configureDirectoryPathRelativeToProjectBuildPath) : targetName(std::move(targetName_)), libraryType(Project::libraryType) {
  auto targetConfigureDirectoryPath =
      Project::BUILD_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
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

//todo: don't give property values if they are different from project property values.
//todo: don't give property values if some are empty
//todo: improve property values in compile definitions.
void to_json(Json &j, const Library &p) {
  j["PROJECT_FILE_PATH"] = Project::BUILD_DIRECTORY.path.string() + Project::PROJECT_NAME + ".hmake";
  j["NAME"] = p.targetName;
  j["LIBRARY_TYPE"] = p.libraryType;
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
  for (const auto &e : p.compilerFlagsDependencies) {
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
