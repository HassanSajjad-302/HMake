#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catShared = getCppSharedDSC("Cat", true);
    catShared.getSourceTarget().sourceFiles("../Example4/Cat/src/Cat.cpp").publicIncludes("../Example4/Cat/header");

    DSC<CppSourceTarget> &animalShared = getCppExeDSC("Animal").privateLibraries(
        &catShared, PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animalShared.getSourceTarget().sourceFiles("../Example4/main.cpp");

    getRoundZeroUpdateBTarget(
        [&](Builder &builder, BTarget &bTarget) {
            if (bTarget.realBTargets[0].exitStatus == EXIT_SUCCESS &&
                bTarget.fileStatus.load(std::memory_order_acquire))
            {
                std::filesystem::copy(catShared.getLinkOrArchiveTarget().getActualOutputPath(),
                                      path(animalShared.getLinkOrArchiveTarget().getActualOutputPath()).parent_path(),
                                      std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(catShared.getLinkOrArchiveTarget().getActualOutputPath());
                std::lock_guard<std::mutex> lk(printMutex);
                printMessage("libCat.so copied to Animal/ and deleted from Cat/\n");
            }
        },
        animalShared.getLinkOrArchiveTarget(), catShared.getLinkOrArchiveTarget());
}

MAIN_FUNCTION