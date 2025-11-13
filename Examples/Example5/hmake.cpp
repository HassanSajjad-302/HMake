#include "Configure.hpp"

struct CopySharedLib final : public BTarget
{
    const string &copyFrom;
    const string &copyTo;

    CopySharedLib(const string &copyFrom_, const string &copyTo_) : copyFrom(copyFrom_), copyTo(copyTo_)
    {
    }
    void updateBTarget(Builder &, unsigned short, bool &isComplete) override
    {
        if (realBTargets[0].exitStatus == EXIT_SUCCESS && realBTargets[0].updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            std::filesystem::copy(copyFrom, path(copyTo).parent_path(),
                                  std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove(copyFrom);
        }
    }
};

CopySharedLib *p;
void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &catShared = config.getCppSharedDSC("Cat", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("../Example4/Cat/src/Cat.cpp").publicIncludes("../Example4/Cat/header");

    DSC<CppTarget> &animal = config.getCppExeDSC("Animal").privateDeps(
        catShared, PrebuiltDep{.reqRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animal.getSourceTarget().sourceFiles("../Example4/main.cpp");

    p = new CopySharedLib(catShared.getPLOAT().outputFileNode->filePath, animal.getPLOAT().outputFileNode->filePath);
    p->addDepNow<0>(animal.getPLOAT());
    p->addDepNow<0>(catShared.getPLOAT());
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION