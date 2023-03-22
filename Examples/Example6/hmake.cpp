#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);

    auto makeAnimal = [](TargetType targetType) {
        string str = targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";
        DSCPrebuilt<CPT> &cat = GetCPTDSC("Cat" + str, "../Example2/Build/Cat" + str + "/", targetType);
        cat.getSourceTarget()
            .INTERFACE_INCLUDES("../Example2/Cat/header")
            .INTERFACE_COMPILE_DEFINITION("CAT_EXPORT",
                                          targetType == TargetType::LIBRARY_STATIC ? "" : "__declspec(dllimport)");

        DSC<CppSourceTarget> &animal = GetCppExeDSC("Animal" + str);
        animal.PRIVATE_LIBRARIES(&cat);
        animal.getSourceTarget().SOURCE_FILES("main.cpp");
    };

    makeAnimal(TargetType::LIBRARY_STATIC);
    makeAnimal(TargetType::LIBRARY_SHARED);

    configureOrBuild();
}