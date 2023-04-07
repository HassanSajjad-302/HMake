#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catStatic = GetCppStaticDSC("Cat-Static");
    catStatic.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    DSC<CppSourceTarget> &animalStatic = GetCppExeDSC("Animal-Static");
    animalStatic.PRIVATE_LIBRARIES(&catStatic);
    animalStatic.getSourceTarget().SOURCE_FILES("main.cpp");

    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat-Shared");
    catShared.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    DSC<CppSourceTarget> &animalShared = GetCppExeDSC("Animal-Shared");
    animalShared.PRIVATE_LIBRARIES(&catShared);
    animalShared.getSourceTarget().SOURCE_FILES("main.cpp");

    // Have a copyToDependent bool in Cat-Shared and copy the dll based on that bool in LinkOrArchiveTarget.
    using CoppyDLLType = RoundZeroBTarget<void(Builder &, unsigned short)>;
    CoppyDLLType &copyDLL = const_cast<CoppyDLLType &>(targets<CoppyDLLType>.emplace().first.operator*());
    copyDLL.realBTarget.addDependency(*(catShared.linkOrArchiveTarget));
    copyDLL.setUpdateFunctor([&](Builder &builder, unsigned short round) {
        if (!round && copyDLL.realBTarget.exitStatus == EXIT_SUCCESS)
        {
            std::filesystem::copy(catShared.linkOrArchiveTarget->getActualOutputPath(),
                                  path(animalShared.linkOrArchiveTarget->getActualOutputPath()).parent_path(),
                                  std::filesystem::copy_options::overwrite_existing);
        }
    });
    for (const DSC<CppSourceTarget> &cppTargetConst : targets<DSC<CppSourceTarget>>)
    {
        auto &cppTarget = const_cast<DSC<CppSourceTarget> &>(cppTargetConst);

        if (cppTarget.getSourceTarget().EVALUATE(BTFamily::MSVC))
        {
            if (cppTarget.linkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
            {
                cppTarget.getSourceTarget().PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllexport)");
                cppTarget.getSourceTarget().INTERFACE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllimport)");
            }
            else if (cppTarget.linkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_STATIC))
            {
                cppTarget.getSourceTarget().PUBLIC_COMPILE_DEFINITION("CAT_EXPORT", "");
            }
        }
        else
        {
            cppTarget.getSourceTarget().PUBLIC_COMPILE_DEFINITION("CAT_EXPORT", "");
        }
    }
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