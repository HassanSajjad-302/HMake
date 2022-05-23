#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variant{};

    /*
    Executable hhelper("hhelper", variant);
    hhelper.sourceFiles.emplace_back("hhelper/src/main.cpp");


    hhelper.compileDefinitionDependencies.push_back(CompileDefinitionDependency{
        .compileDefinition =
            CompileDefinition{.name = "HCONFIGURE_HEADER",
                              .value = Cache::sourceDirectory.directoryPath.string() + "/hconfigure/header/"},
        .dependencyType = DependencyType::PRIVATE});
    hhelper.compileDefinitionDependencies.push_back(CompileDefinitionDependency{
        .compileDefinition =
            CompileDefinition{.name = "JSON_HEADER",
                              .value = Cache::sourceDirectory.directoryPath.string() + "/json/include/"},
        .dependencyType = DependencyType::PRIVATE});
    hhelper.compileDefinitionDependencies.push_back(CompileDefinitionDependency{
        .compileDefinition =
            CompileDefinition{.name = "FMT_HEADER",
                              .value = Cache::sourceDirectory.directoryPath.string() + "/fmt/include/"},
        .dependencyType = DependencyType::PRIVATE});


    Library hconfigure("hconfigure", variant);
    hconfigure.sourceFiles.emplace_back("hhelper/src/main.cpp");


    hconfigure.includeDirectoryDependencies.push_back(IDD{.includeDirectory = Directory(""), .dependencyType =
    DependencyType::PUBLIC});

    hhelper.compileDefinitionDependencies.push_back(CompileDefinitionDependency{
        .compileDefinition =
            CompileDefinition{.name = "HCONFIGURE_STATIC_LIB_DIRECTORY",
                              .value = Cache::sourceDirectory.directoryPath.string() + "/hconfigure/header/"},
        .dependencyType = DependencyType::PRIVATE});


    variant.executables.push_back(app);
    project.projectVariants.push_back(variant);

    project.configure();*/
}
