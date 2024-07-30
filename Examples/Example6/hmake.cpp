#include "Configure.hpp"

void buildSpecification()
{
    auto makeApps = [](TargetType targetType) {
        string str = targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSC<CppSourceTarget> &cat =
            getCppTargetDSC_P("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().interfaceIncludes("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = getCppTargetDSC("Dog" + str, targetType, true, "DOG_EXPORT");
        dog.publicLibraries(&cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppSourceTarget> &dog2 = getCppTargetDSC("Dog2" + str, targetType, true, "DOG2_EXPORT");
        dog2.privateLibraries(&cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

        DSC<CppSourceTarget> &app = getCppExeDSC("App" + str);
        app.getLinkOrArchiveTarget().setOutputName("app");
        app.privateLibraries(&dog).getSourceTarget().sourceFiles("main.cpp");

        DSC<CppSourceTarget> &app2 = getCppExeDSC("App2" + str);
        app2.getLinkOrArchiveTarget().setOutputName("app");
        app2.privateLibraries(&dog2).getSourceTarget().sourceFiles("main2.cpp");
    };

    makeApps(TargetType::LIBRARY_STATIC);
    makeApps(TargetType::LIBRARY_SHARED);
}

MAIN_FUNCTION