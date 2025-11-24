
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "ConfigurationAssign.hpp"
#include "CppTarget.hpp"
#include "DSC.hpp"
#include "LOAT.hpp"
#include "ToolsCache.hpp"

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
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // Making sure that Custom Clang fork exists.
        Node::getNodeNonNormalized(compilerFeatures.compiler.bTPath, true);
    }
    compilerFlags = compilerFeatures.getCompilerFlags();
    linkerFlags = linkerFeatures.getLinkerFlags();
    if (!stdCppTarget)
    {
        SystemTarget s1 = systemTarget;
        IgnoreHeaderDeps i1 = ignoreHeaderDeps;
        systemTarget = SystemTarget::YES;
        ignoreHeaderDeps = IgnoreHeaderDeps::YES;

        stdCppTarget = &getCppStaticDSC("std");
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            if (cache.isCompilerInToolsArray)
            {
                CppTarget *c = stdCppTarget->getSourceTargetPointer();
                // Use getNodeFromNormalizedPath instead
                if constexpr (os == OS::NT)
                {
                    const vector<string> &includes = toolsCache.vsTools[cache.selectedCompilerArrayIndex].includeDirs;
                    Node *zeroInclNode = Node::getNodeNonNormalized(includes[0], false);
                    c->actuallyAddInclude(false, zeroInclNode, true, true);

                    // Only the first include is compiled as header-unit.
                    if (evaluate(IsCppMod::YES) && evaluate(StdAsHeaderUnit::YES))
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
                        Node *inclNode = Node::getNodeNonNormalized(includes[i], false);
                        c->actuallyAddInclude(false, inclNode, true, true);
                        c->addHeaderUnitOrFileDirMSVC(inclNode, true, false, true, true, true, true);
                    }

                    if constexpr (os == OS::NT)
                    {
                        if (evaluate(BigHeaderUnit::YES))
                        {
                            c->publicBigHus.emplace_back(nullptr);
                            c->makeHeaderFileAsUnit("windows.h", true, true);
                        }
                    }
                }
                else
                {
                    for (const string &str : toolsCache.linuxTools[cache.selectedCompilerArrayIndex].includeDirs)
                    {
                        Node *inclNode = Node::getNodeNonNormalized(str, false);
                        c->addHeaderUnitOrFileDir(inclNode, "", true, "", true, true);
                    }
                }
            }
            if (cache.isLinkerInToolsArray)
            {
                const VSTools &vsTools = toolsCache.vsTools[cache.selectedLinkerArrayIndex];
                for (const string &str : vsTools.libraryDirs)
                {
                    Node *node = Node::getNodeNonNormalized(str, false);
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
                        stdCppTarget->getLOAT().reqLibraryDirs.emplace_back(node);
                    }
                }

                stdCppTarget->getLOAT().useReqLibraryDirs = stdCppTarget->getLOAT().reqLibraryDirs;
            }
        }

        systemTarget = s1;
        ignoreHeaderDeps = i1;
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

CppTarget &Configuration::getCppObject(const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(name + slashc + name_, this);
    cppTargets.emplace_back(&cppTarget);
    return cppTarget;
}

CppTarget &Configuration::getCppObject(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(myBuildDir, explicitBuild, name + slashc + name_, this);
    cppTargets.emplace_back(&cppTarget);
    return cppTarget;
}

CppTarget &Configuration::getCppObjectAddStdTarget(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(myBuildDir, explicitBuild, name + slashc + name_, this);
    cppTargets.emplace_back(&cppTarget);
    return addStdCppDep(cppTarget);
}

LOAT &Configuration::GetExeLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::GetExeLOAT(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name + slashc + name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOAT(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name + slashc + name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOAT(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name + slashc + name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOAT(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat =
        targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name + slashc + name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getPLOAT(const string &name_, Node *myBuildDir, TargetType linkTargetType_)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, myBuildDir, linkTargetType_);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getStaticPLOAT(const string &name_, Node *myBuildDir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, myBuildDir, TargetType::LIBRARY_STATIC);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getSharedPLOAT(const string &name_, Node *myBuildDir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name + slashc + name_, myBuildDir, TargetType::LIBRARY_SHARED);
    ploats.emplace_back(&loat);
    return loat;
}

CppTarget &Configuration::addStdCppDep(CppTarget &target)
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateDeps(stdCppTarget->getSourceTarget());
    }
    return target;
}

DSC<CppTarget> &Configuration::addStdDSCCppDep(DSC<CppTarget> &target) const
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateDeps(*stdCppTarget);
    }
    return target;
}

DSC<CppTarget> &Configuration::getCppObjectDSC(const string &name_, bool defines, string define)
{
    DSC<CppTarget> &dscCppTarget =
        targets<DSC<CppTarget>>.emplace_back(&getCppObject(name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppTarget> &Configuration::getCppObjectDSC(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                               bool defines, string define)
{
    DSC<CppTarget> &dscCppTarget = targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(explicitBuild, myBuildDir, name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppTarget> &Configuration::getCppExeDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(&getCppObject(name_ + dashCpp), &GetExeLOAT(name_),
                                                                defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppExeDSC(bool explicitBuild, Node *myBuildDir, const string &name_,
                                            const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(explicitBuild, myBuildDir, name_ + dashCpp), &GetExeLOAT(explicitBuild, myBuildDir, name_),
        defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppTargetDSC(const string &name_, const bool defines, string define)
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

DSC<CppTarget> &Configuration::getCppTargetDSC(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                               const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC(explicitBuild, myBuildDir, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC(explicitBuild, myBuildDir, name_, defines, std::move(define)));
    }
}

DSC<CppTarget> &Configuration::getCppStaticDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(&getCppObject(name_ + dashCpp), &getStaticLOAT(name_),
                                                                defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppStaticDSC(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                               const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(explicitBuild, myBuildDir, name_ + dashCpp), &getStaticLOAT(explicitBuild, myBuildDir, name_),
        defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(&getCppObject(name_ + dashCpp), &getSharedLOAT(name_),
                                                                defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSC(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                               const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(explicitBuild, myBuildDir, name_ + dashCpp), &getSharedLOAT(explicitBuild, myBuildDir, name_),
        defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppTargetDSC_P(const string &name_, Node *myBuildDir, const bool defines,
                                                 string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_P(name_, myBuildDir, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_P(name_, myBuildDir, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppTarget> &Configuration::getCppTargetDSC_P(const string &name_, const string &prebuiltName, Node *myBuildDir,
                                                 bool defines, string define)
{
    CppTarget *cppTarget = &getCppObject(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            targets<DSC<CppTarget>>.emplace_back(cppTarget, &getStaticPLOAT(prebuiltName, myBuildDir), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            targets<DSC<CppTarget>>.emplace_back(cppTarget, &getSharedPLOAT(prebuiltName, myBuildDir), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppTarget> &Configuration::getCppStaticDSC_P(const string &name_, Node *myBuildDir, const bool defines,
                                                 string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticPLOAT(name_, myBuildDir), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSC_P(const string &name_, Node *myBuildDir, const bool defines,
                                                 string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedPLOAT(name_, myBuildDir), defines, std::move(define)));
}

CppTarget &Configuration::getCppObjectNoName(const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(name_, this);
    cppTargets.emplace_back(&cppTarget);
    return cppTarget;
}

CppTarget &Configuration::getCppObjectNoName(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(myBuildDir, explicitBuild, name_, this);
    cppTargets.emplace_back(&cppTarget);
    return cppTarget;
}

CppTarget &Configuration::getCppObjectNoNameAddStdTarget(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    CppTarget &cppTarget = targets<CppTarget>.emplace_back(myBuildDir, explicitBuild, name_, this);
    cppTargets.emplace_back(&cppTarget);
    return addStdCppDep(cppTarget);
}

LOAT &Configuration::GetExeLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::GetExeLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name_, TargetType::EXECUTABLE);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getStaticLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name_, TargetType::LIBRARY_STATIC);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOATNoName(const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

LOAT &Configuration::getSharedLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_)
{
    LOAT &loat = targets<LOAT>.emplace_back(*this, myBuildDir, explicitBuild, name_, TargetType::LIBRARY_SHARED);
    loats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getPLOATNoName(const string &name_, Node *myBuildDir, TargetType linkTargetType_)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, myBuildDir, linkTargetType_);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getStaticPLOATNoName(const string &name_, Node *myBuildDir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, myBuildDir, TargetType::LIBRARY_STATIC);
    ploats.emplace_back(&loat);
    return loat;
}

PLOAT &Configuration::getSharedPLOATNoName(const string &name_, Node *myBuildDir)
{
    PLOAT &loat = targets<PLOAT>.emplace_back(*this, name_, myBuildDir, TargetType::LIBRARY_SHARED);
    ploats.emplace_back(&loat);
    return loat;
}

DSC<CppTarget> &Configuration::getCppObjectDSCNoName(const string &name_, bool defines, string define)
{
    DSC<CppTarget> &dscCppTarget =
        targets<DSC<CppTarget>>.emplace_back(&getCppObjectNoName(name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppTarget> &Configuration::getCppObjectDSCNoName(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                                     bool defines, string define)
{
    DSC<CppTarget> &dscCppTarget = targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, myBuildDir, name_ + dashCpp), nullptr, defines, std::move(define));
    return addStdDSCCppDep(dscCppTarget);
}

DSC<CppTarget> &Configuration::getCppExeDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(&getCppObjectNoName(name_ + dashCpp),
                                                                &GetExeLOATNoName(name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppExeDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_,
                                                  const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, myBuildDir, name_ + dashCpp),
        &GetExeLOATNoName(explicitBuild, myBuildDir, name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppTargetDSCNoName(const string &name_, const bool defines, string define)
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

DSC<CppTarget> &Configuration::getCppTargetDSCNoName(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                                     const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSCNoName(explicitBuild, myBuildDir, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSCNoName(explicitBuild, myBuildDir, name_, defines, std::move(define)));
    }
}

DSC<CppTarget> &Configuration::getCppStaticDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticLOATNoName(name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppStaticDSCNoName(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                                     const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, myBuildDir, name_ + dashCpp),
        &getStaticLOATNoName(explicitBuild, myBuildDir, name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedLOATNoName(name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSCNoName(const bool explicitBuild, Node *myBuildDir, const string &name_,
                                                     const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, myBuildDir, name_ + dashCpp),
        &getSharedLOATNoName(explicitBuild, myBuildDir, name_), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, Node *myBuildDir, const bool defines,
                                                       string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_PNoName(name_, myBuildDir, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_PNoName(name_, myBuildDir, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, const string &prebuiltName,
                                                       Node *myBuildDir, bool defines, string define)
{
    CppTarget *cppTarget = &getCppObjectNoName(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            targets<DSC<CppTarget>>.emplace_back(cppTarget, &getStaticPLOATNoName(prebuiltName, myBuildDir), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            targets<DSC<CppTarget>>.emplace_back(cppTarget, &getSharedPLOATNoName(prebuiltName, myBuildDir), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
}

DSC<CppTarget> &Configuration::getCppStaticDSC_PNoName(const string &name_, Node *myBuildDir, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticPLOATNoName(name_, myBuildDir), defines, std::move(define)));
}

DSC<CppTarget> &Configuration::getCppSharedDSC_PNoName(const string &name_, Node *myBuildDir, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedPLOATNoName(name_, myBuildDir), defines, std::move(define)));
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
