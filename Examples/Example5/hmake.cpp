#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat", true);
    catShared.getSourceTarget().SOURCE_FILES("../Example4/Cat/src/Cat.cpp").PUBLIC_INCLUDES("../Example4/Cat/header");

    DSC<CppSourceTarget> &animalShared = GetCppExeDSC("Animal").PRIVATE_LIBRARIES(
        &catShared, PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animalShared.getSourceTarget().SOURCE_FILES("../Example4/main.cpp");

    GetRoundZeroUpdateBTarget(
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