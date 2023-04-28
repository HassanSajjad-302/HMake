#include "Configure.hpp"

void buildSpecification()
{
    auto makeApps = [](TargetType targetType) {
        string str = targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSCPrebuilt<CPT> &cat =
            GetCPTTargetDSC("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().INTERFACE_INCLUDES("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = GetCppTargetDSC("Dog" + str, true, "DOG_EXPORT", targetType);
        dog.PUBLIC_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog/src/Dog.cpp").PUBLIC_INCLUDES("Dog/header/");

        DSC<CppSourceTarget> &dog2 = GetCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT", targetType);
        dog2.PRIVATE_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog2/src/Dog.cpp").PUBLIC_INCLUDES("Dog2/header/");

        DSC<CppSourceTarget> &app = GetCppExeDSC("App" + str);
        app.PRIVATE_LIBRARIES(&dog).getSourceTarget().SOURCE_FILES("main.cpp");

        DSC<CppSourceTarget> &app2 = GetCppExeDSC("App2" + str);
        app2.PRIVATE_LIBRARIES(&dog2).getSourceTarget().SOURCE_FILES("main2.cpp");
    };

    makeApps(TargetType::LIBRARY_STATIC);
    makeApps(TargetType::LIBRARY_SHARED);
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        exportAllSymbolsAndInitializeGlobals();
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif