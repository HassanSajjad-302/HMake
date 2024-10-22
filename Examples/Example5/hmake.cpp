#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catShared = getCppSharedDSC("Cat", true);
    catShared.getSourceTarget().sourceFiles("../Example4/Cat/src/Cat.cpp").publicIncludes("../Example4/Cat/header");

    DSC<CppSourceTarget> &animalShared = getCppExeDSC("Animal").privateLibraries(
        &catShared, PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animalShared.getSourceTarget().sourceFiles("../Example4/main.cpp");

    getRoundZeroUpdateBTarget(
        [&](Builder &, BTarget &bTarget) {
            if (bTarget.realBTargets[0].exitStatus == EXIT_SUCCESS && atomic_ref(bTarget.fileStatus).load())
            {
                const LinkOrArchiveTarget &catSharedLink = catShared.getLinkOrArchiveTarget();
                const LinkOrArchiveTarget &animalSharedLink = animalShared.getLinkOrArchiveTarget();
                copy(catSharedLink.outputFileNode->filePath,
                     path(animalSharedLink.outputFileNode->filePath).parent_path(),
                     std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(catSharedLink.outputFileNode->filePath);

                std::lock_guard lk(printMutex);
                printMessage("libCat.so copied to Animal/ and deleted from Cat/\n");
            }
        },
        animalShared.getLinkOrArchiveTarget(), catShared.getLinkOrArchiveTarget());
}

MAIN_FUNCTION