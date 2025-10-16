
#ifdef USE_HEADER_UNITS
import "Configuration.hpp";
import "BoostCppTarget.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "DSC.hpp";
import "LOAT.hpp";
#else
#include "Configuration.hpp"
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LOAT.hpp"
#endif

Configuration::Configuration(const string &name_) : BTarget(name_, false, false)
{
}

void Configuration::postConfigurationSpecification() const
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (BoostCppTarget *t : boostCppTargets)
        {
            t->copyConfigCache();
        }
    }
}

void Configuration::initialize()
{
    compilerFeatures.initialize();
    compilerFlags = compilerFeatures.getCompilerFlags();
    linkerFlags = linkerFeatures.getLinkerFlags();
    if (!stdCppTarget)
    {
        stdCppTarget = &getCppStaticDSC("std");
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            if (cache.isCompilerInToolsArray)
            {
                CppSourceTarget *c = stdCppTarget->getSourceTargetPointer();
                // Use getNodeFromNormalizedPath instead
                if constexpr (os == OS::NT)
                {
                    const vector<string> &includes = toolsCache.vsTools[cache.selectedCompilerArrayIndex].includeDirs;
                    Node *zeroInclNode = Node::getNodeFromNonNormalizedPath(includes[0], false);
                    c->actuallyAddInclude(false, zeroInclNode, true, true, true, true);

                    // Only the first include is compiled as header-unit.
                    if (evaluate(TreatModuleAsSource::NO) && evaluate(StdAsHeaderUnit::YES))
                    {
                        c->addHeaderUnitOrFileDirMSVC(zeroInclNode, false, true, true, true, true, true);
                        // c->addHeaderUnitOrFileDirMSVC(zeroInclNode, true, false, true, true, true, true);
                    }
                    else
                    {
                        c->addHeaderUnitOrFileDirMSVC(zeroInclNode, true, false, true, true, true, true);
                    }

                    for (uint32_t i = 1; i < includes.size(); ++i)
                    {
                        Node *inclNode = Node::getNodeFromNonNormalizedPath(includes[i], false);
                        c->actuallyAddInclude(false, inclNode, true, true, true, true);
                        c->addHeaderUnitOrFileDirMSVC(inclNode, true, false, true, true, true, true);
                    }
                }
                else
                {
                    for (const string &str : toolsCache.linuxTools[cache.selectedCompilerArrayIndex].includeDirs)
                    {
                        Node *inclNode = Node::getNodeFromNonNormalizedPath(str, false);
                        c->addHeaderUnitOrFileDir(inclNode, "", true, "", true, true, true, true);
                    }
                }
            }
            if (cache.isLinkerInToolsArray)
            {
                const VSTools &vsTools = toolsCache.vsTools[cache.selectedLinkerArrayIndex];
                for (const string &str : vsTools.libraryDirs)
                {
                    Node *node = Node::getNodeFromNonNormalizedPath(str, false);
                    bool found = false;
                    for (const LibDirNode &libDirNode : stdCppTarget->getLOAT().reqLibraryDirs)
                    {
                        if (libDirNode.node == node)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        stdCppTarget->getLOAT().reqLibraryDirs.emplace_back(node, true);
                    }
                }

                stdCppTarget->getLOAT().useReqLibraryDirs = stdCppTarget->getLOAT().reqLibraryDirs;
            }
        }
    }
}

void Configuration::markArchivePoint()
{
    // TODO
    // This functions marks the archive point i.e. the targets before this function should be archived upon
    // successful build. i.e. some extra info will be saved in build-cache.json file of these targets. The goal is
    // that next time when hbuild is invoked, archived targets source-files won't be checked for existence/rebuilt.
    // Neither the header-files coming from such targets includes will be stored in cache. The use-case is when e.g.
    // a target A dependens on targets B and C, such that these targets source is never meant to be changed e.g. fmt
    // and json source in hmake project.
}

CppSourceTarget &Configuration::getCppObject(const string &name_)
{
    CppSourceTarget &cppSourceTarget = targets<CppSourceTarget>.emplace_back(name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObject(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                             const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectAddStdTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                         const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return addStdCppDep(cppSourceTarget);
}

LOAT &Configuration::GetExeLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::GetExeLOAT(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name + slashc + name_,
                                            TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOAT(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name + slashc + name_,
                                            TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOAT(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name + slashc + name_,
                                            TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getPLOAT(const string &name_, const string &dir, TargetType linkTargetType_)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, dir, linkTargetType_);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getStaticPLOAT(const string &name_, const string &dir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, dir, TargetType::LIBRARY_STATIC);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getSharedPLOAT(const string &name_, const string &dir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, dir, TargetType::LIBRARY_SHARED);
    ploats.emplace_back(&loat);
    return loat;
}

CppSourceTarget &Configuration::addStdCppDep(CppSourceTarget &target)
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateDeps(&stdCppTarget->getSourceTarget());
    }
    return target;
}

DSC<CppSourceTarget> &Configuration::addStdDSCCppDep(DSC<CppSourceTarget> &target) const
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateDeps(*stdCppTarget);
    }
    return target;
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const string &name_, bool defines, string define)
{
    DSC<CppSourceTarget> &dscCppTarget =
        targets<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, bool defines, string define)
{
    DSC<CppSourceTarget> &dscCppTarget = targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp),
                                                                      &GetExeLOAT(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                  const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLOAT(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC(name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC(name_, defines, std::move(define)));
    }
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            getCppStaticDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            getCppSharedDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticLOAT(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLOAT(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedLOAT(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLOAT(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const string &name_, const string &dir, const bool defines,
                                                       string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_P(name_, dir, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_P(name_, dir, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const string &name_, const string &prebuiltName,
                                                       const string &dir, bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            targets<DSC<CppSourceTarget>>.emplace_back(cppSourceTarget, &getStaticPLOAT(prebuiltName, dir), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            targets<DSC<CppSourceTarget>>.emplace_back(cppSourceTarget, &getSharedPLOAT(prebuiltName, dir), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_P(const string &name_, const string &dir, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticPLOAT(name_, dir), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_P(const string &name_, const string &dir, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedPLOAT(name_, dir), defines, std::move(define)));
}

CppSourceTarget &Configuration::getCppObjectNoName(const string &name_)
{
    CppSourceTarget &cppSourceTarget = targets<CppSourceTarget>.emplace_back(name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                   const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoNameAddStdTarget(bool explicitBuild,
                                                               const string &buildCacheFilesDirPath_,
                                                               const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return addStdCppDep(cppSourceTarget);
}

LOAT &Configuration::GetExeLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::GetExeLOATNoName(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOATNoName(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOATNoName(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getPLOATNoName(const string &name_, const string &dir, TargetType linkTargetType_)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, dir, linkTargetType_);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getStaticPLOATNoName(const string &name_, const string &dir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, dir, TargetType::LIBRARY_STATIC);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getSharedPLOATNoName(const string &name_, const string &dir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, dir, TargetType::LIBRARY_SHARED);
    ploats.emplace_back(&loat);
    return loat;
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const string &name_, bool defines, string define)
{
    DSC<CppSourceTarget> &dscCppTarget = targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           bool defines, string define)
{
    DSC<CppSourceTarget> &dscCppTarget = targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), nullptr, defines,
        std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &GetExeLOATNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                        const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLOATNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSCNoName(name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSCNoName(name_, defines, std::move(define)));
    }
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            getCppStaticDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            getCppSharedDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticLOATNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLOATNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedLOATNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLOATNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, const string &dir, const bool defines,
                                                             string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_PNoName(name_, dir, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_PNoName(name_, dir, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, const string &prebuiltName,
                                                             const string &dir, bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &getCppObjectNoName(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPLOATNoName(prebuiltName, dir), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPLOATNoName(prebuiltName, dir), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_PNoName(const string &name_, const string &dir, const bool defines,
                                                             string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticPLOATNoName(name_, dir), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_PNoName(const string &name_, const string &dir, const bool defines,
                                                             string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedPLOATNoName(name_, dir), defines, std::move(define)));
}

BoostCppTarget &Configuration::getBoostCppTarget(const string &name, bool headerOnly, bool hasBigHeader,
                                                 bool createTestsTarget, bool createExamplesTarget)
{
    return *boostCppTargets.emplace_back(&targets<BoostCppTarget>.emplace_back(
        name, this, headerOnly, hasBigHeader, createTestsTarget, createExamplesTarget));
}

bool operator<(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.name < rhs.name;
}

Configuration &getConfiguration(const string &name)
{
    return targets<Configuration>.emplace_back(name);
}
