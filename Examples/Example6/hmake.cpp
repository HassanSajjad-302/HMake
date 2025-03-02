#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    auto makeApps = [&]() {
        const string str = config.targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSC<CppSourceTarget> &cat =
            config.getCppTargetDSC_P("Cat" + str, "../Example4/Build/Release/Cat" + str + "/", true, "CAT_EXPORT");
        cat.getSourceTarget().interfaceIncludes("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = config.getCppTargetDSC("Dog" + str, true, "DOG_EXPORT");
        dog.publicLibraries(&cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppSourceTarget> &dog2 = config.getCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT");
        dog2.privateLibraries(&cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

        DSC<CppSourceTarget> &app = config.getCppExeDSC("App" + str);
        app.getLinkOrArchiveTarget().setOutputName("app");
        app.privateLibraries(&dog).getSourceTarget().sourceFiles("main.cpp");

        DSC<CppSourceTarget> &app2 = config.getCppExeDSC("App2" + str);
        app2.getLinkOrArchiveTarget().setOutputName("app");
        app2.privateLibraries(&dog2).getSourceTarget().sourceFiles("main2.cpp");
    };

    config.targetType = TargetType::LIBRARY_STATIC;
    makeApps();
    config.targetType = TargetType::LIBRARY_SHARED;
    makeApps();
}

void buildSpecification()
{
    getConfiguration();
    callConfigurationSpecification();
}

MAIN_FUNCTION