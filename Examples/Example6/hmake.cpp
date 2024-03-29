#include "Configure.hpp"

void buildSpecification()
{
    auto makeApps = [](TargetType targetType) {
        string str = targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSC<CppSourceTarget> &cat =
            GetCppTargetDSC_P("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().INTERFACE_INCLUDES("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = GetCppTargetDSC("Dog" + str, targetType, true, "DOG_EXPORT");
        dog.PUBLIC_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog/src/Dog.cpp").PUBLIC_INCLUDES("Dog/header");

        DSC<CppSourceTarget> &dog2 = GetCppTargetDSC("Dog2" + str, targetType, true, "DOG2_EXPORT");
        dog2.PRIVATE_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog2/src/Dog.cpp").PUBLIC_INCLUDES("Dog2/header");

        DSC<CppSourceTarget> &app = GetCppExeDSC("App" + str);
        app.getLinkOrArchiveTarget().setOutputName("app");
        app.PRIVATE_LIBRARIES(&dog).getSourceTarget().SOURCE_FILES("main.cpp");

        DSC<CppSourceTarget> &app2 = GetCppExeDSC("App2" + str);
        app2.getLinkOrArchiveTarget().setOutputName("app");
        app2.PRIVATE_LIBRARIES(&dog2).getSourceTarget().SOURCE_FILES("main2.cpp");
    };

    makeApps(TargetType::LIBRARY_STATIC);
    makeApps(TargetType::LIBRARY_SHARED);
}

MAIN_FUNCTION