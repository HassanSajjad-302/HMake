#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &app = GetCppExeDSC("app");
    app.getSourceTarget().SOURCE_FILES("main.cpp");
}

extern "C" EXPORT int func(BSMode bsMode_)
{
    std::function f([&]() {
        exportAllSymbolsAndInitializeGlobals();
        initializeCache(bsMode_);
        buildSpecification();
    });
    return executeInTryCatchAndSetErrorMessagePtr(std::move(f));
}

#ifdef EXE
int main(int argc, char **argv)
{
    func(getBuildSystemModeFromArguments(argc, argv));
    configureOrBuild();
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    std::function f([&]() { func(bsMode_); });
    int errorCode = executeInTryCatchAndSetErrorMessagePtr(std::move(f));
    if (errorCode != EXIT_SUCCESS)
    {
        return errorCode;
    }
    std::function j([&]() { configureOrBuild(); });
    return executeInTryCatchAndSetErrorMessagePtr(std::move(j));
}
#endif