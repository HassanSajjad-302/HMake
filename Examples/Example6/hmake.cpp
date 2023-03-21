#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);

    DSC<CppSourceTarget> &animal = GetCppExeDSC("animal");
    animal.getSourceTarget().SOURCE_FILES("main.cpp");

    // PLibrary means prebuilt library. if you want to use a prebuilt library, you can use it this way.
    // Note that the library had to specify the includeDirectoryDependency explicitly. An easy was to use a packaged
    // library. Generating a hmake packaged library is shown in next example.

    PrebuiltLinkOrArchiveTarget prebuiltLinkOrArchiveTarget("Cat-Static", "../Example2/Build/Cat-Static/",
                                                            TargetType::LIBRARY_STATIC);
    CPT catCppPrebuilt;
    catCppPrebuilt.usageRequirementIncludes.emplace(Node::getNodeFromString("../Example2/Cat/header", false));
    catCppPrebuilt.usageRequirementCompileDefinitions.emplace(Define("CAT_EXPORT"));
    animal.linkOrArchiveTarget->PRIVATE_DEPS(&prebuiltLinkOrArchiveTarget);
    animal.objectFileProducer->PRIVATE_DEPS(&catCppPrebuilt);

    configureOrBuild();
}