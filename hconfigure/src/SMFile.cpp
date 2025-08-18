
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "JConsts.hpp";
import "RunCommand.hpp";
import "Settings.hpp";
import "StaticVector.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <mutex>;
import <utility>;
#else
#include "SMFile.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "RunCommand.hpp"
#include "Settings.hpp"
#include "StaticVector.hpp"
#include "TargetCacheDiskWriteManager.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <mutex>
#include <utility>
#endif

using std::tie, std::ifstream, std::exception, std::lock_guard;

bool CompareSourceNode::operator()(const SourceNode &lhs, const SourceNode &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareSourceNode::operator()(const Node *lhs, const SourceNode &rhs) const
{
    return lhs < rhs.node;
}

bool CompareSourceNode::operator()(const SourceNode &lhs, const Node *rhs) const
{
    return lhs.node < rhs;
}

SourceNode::SourceNode(CppSourceTarget *target_, Node *node_) : target(target_), node{node_}
{
}

SourceNode::SourceNode(CppSourceTarget *target_, const Node *node_, const bool add0, const bool add1, const bool add2)
    : ObjectFile(add0, add1, add2), target(target_), node{node_}
{
}

string SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".o",
                          pathPrint);
}

string SourceNode::getTarjanNodeName() const
{
    return node->filePath;
}

void SourceNode::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round)
    {
        RealBTarget &realBTarget = realBTargets[0];
        if (selectiveBuild)
        {
            const_cast<Node *>(node)->ensureSystemCheckCalled(true);

            objectFileOutputFileNode = Node::getNodeFromNormalizedString(
                target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".o", true, true);

            setSourceNodeFileStatus();
            if (fileStatus)
            {
                assignFileStatusToDependents(0);
                PostCompile postCompile = target->updateSourceNodeBTarget(*this);
                realBTarget.exitStatus = postCompile.exitStatus;
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
                // because cached compile-command would be different
                if (realBTarget.exitStatus == EXIT_SUCCESS)
                {
                    buildCache.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
                    postCompile.parseHeaderDeps(*this);
                }

                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, target, this);
            }
        }
    }
}

bool SourceNode::checkHeaderFiles(const Node *compareNode) const
{
    StaticVector<Node *, 1000> headerFilesUnChecked;

    for (Node *headerNode : buildCache.headerFiles)
    {
        if (headerNode->trySystemCheck(true, true))
        {
            if (headerNode->doesNotExist)
            {
                return true;
            }

            if (headerNode->lastWriteTime > compareNode->lastWriteTime)
            {
                return true;
            }
        }
        else
        {
            headerFilesUnChecked.emplace_back(headerNode);
        }
    }

    if (!headerFilesUnChecked.empty())
    {
        uint64_t uncheckedCountOld = headerFilesUnChecked.size();
        while (true)
        {
            bool zeroLeft = true;
            uint64_t uncheckedCountNew = 0;
            for (uint64_t i = 0; i < uncheckedCountOld; ++i)
            {
                if (Node *headerNode = headerFilesUnChecked[i];
                    headerNode->systemCheckCompleted || atomic_ref(headerNode->systemCheckCompleted).load())
                {
                    if (headerNode->doesNotExist)
                    {
                        return true;
                    }

                    if (headerNode->lastWriteTime > compareNode->lastWriteTime)
                    {
                        return true;
                    }
                }
                else
                {
                    headerFilesUnChecked[uncheckedCountNew] = headerNode;
                    ++uncheckedCountNew;
                    zeroLeft = false;
                }
            }

            if (zeroLeft)
            {
                break;
            }

            uncheckedCountOld = uncheckedCountNew;
        }
    }

    return false;
}

// An invariant is that paths are lexically normalized.
bool pathContainsFile(string_view dir, const string_view file)
{
    string_view withoutFileName(file.data(), file.find_last_of(slashc));

    if (dir.size() > withoutFileName.size())
    {
        return false;
    }

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more dir components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), withoutFileName.begin());
}

void SourceNode::setSourceNodeFileStatus()
{
    if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
    {
        fileStatus = true;
        return;
    }

    if (objectFileOutputFileNode->doesNotExist)
    {
        fileStatus = true;
        return;
    }

    if (node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
    {
        fileStatus = true;
        return;
    }

    if (checkHeaderFiles(objectFileOutputFileNode))
    {
        fileStatus = true;
    }
}

void SourceNode::updateBuildCache()
{
    target->cppBuildCache.srcFiles[indexInBuildCache] = std::move(buildCache);
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    // j[JConsts::srcFile] = *(sourceNode.sourceJson);
}

void to_json(Json &j, const SourceNode *sourceNode)
{
    j = *sourceNode;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node < rhs.node;
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

SMFile::SMFile(CppSourceTarget *target_, const Node *node_, string logicalName_)
    : SourceNode(target_, node_, false, false, false), logicalName(std::move(logicalName_)),
      type(SM_FILE_TYPE::HEADER_UNIT)
{
}

void SMFile::checkHeaderFilesIfSMRulesJsonSet()
{
    if (isSMRulesJsonSet)
    {
        // Check all header-files and set fileStatus to true.

        if (compileCommandWithToolCache.hash != target->compileCommandWithTool.getHash())
        {
            fileStatus = true;
            return;
        }

        if (objectFileOutputFileNode->doesNotExist)
        {
            fileStatus = true;
            return;
        }

        if (node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
        {
            fileStatus = true;
            return;
        }

        // While header-units are checked in shouldGenerateSMFileInRoundOne, they are not checked here.
        // Because, if a header-unit of ours is updated, because we are dependent of it, we will be updated as-well.
        if (checkHeaderFiles(objectFileOutputFileNode))
        {
            fileStatus = true;
        }
    }
}

void SMFile::setLogicalNameAndAddToRequirePath()
{
    if (!smRulesCache.exportName.empty())
    {
        logicalName = smRulesCache.exportName;

        using Map = decltype(requirePaths2);

        const RequireNameTargetId req(this->target->id, logicalName);
        if (const SMFile *val = nullptr; requirePaths2.lazy_emplace_l(
                req, [&](const Map::value_type &val_) { val = const_cast<SMFile *>(val_.second); },
                [&](const Map::constructor &constructor) { constructor(req, this); }))
        {
        }
        else
        {
            printErrorMessage(
                FORMAT("In target:\n{}\nModule name:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                       target->getTarjanNodeName(), logicalName, node->filePath, val->node->filePath));
        }
    }
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    RealBTarget &realBTarget = realBTargets[round];
    if (round == 2 && type == SM_FILE_TYPE::HEADER_UNIT)
    {
        const_cast<Node *>(node)->ensureSystemCheckCalled(true, true);

        // This holds the pointer to bmi file instead of an object file in-case of a header-unit.
        objectFileOutputFileNode = Node::getNodeFromNormalizedString(
            target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc", true, true);

        if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
            isSMRuleFileOutdated = true;
            isSMRuleFileOutdatedCallCompleted = true;
            return;
        }

        if (node->doesNotExist || objectFileOutputFileNode->doesNotExist ||
            node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
        }
    }
    else if (round == 1 && realBTarget.exitStatus == EXIT_SUCCESS)
    {
        if (!isAnOlderHeaderUnit)
        {
            // TODO
            // Move this at the end of scope and use getHalfNode if isSMRuleFileOutdated is true. Avoid the filesystem
            // call.
            if (type == SM_FILE_TYPE::HEADER_UNIT)
            {
                objectFileOutputFileNode = Node::getNodeFromNormalizedString(
                    target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc", true, true);
            }
            else
            {
                const_cast<Node *>(node)->ensureSystemCheckCalled(true);
                objectFileOutputFileNode = Node::getNodeFromNormalizedString(
                    target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".m.o", true, true);
            }

            if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
            {
                isObjectFileOutdated = true;
                isSMRuleFileOutdated = true;

                // Following variables are only accessed in parallel for older header-units in is*Outdated functions.
                isObjectFileOutdatedCallCompleted = true;
                isSMRuleFileOutdatedCallCompleted = true;
            }
            else if (node->doesNotExist || objectFileOutputFileNode->doesNotExist ||
                     node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
            {
                isObjectFileOutdated = true;
                atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            }
        }

        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {

            if (!isObjectFileOutdated)
            {
                checkObjectFileOutdatedHeaderUnits();
            }
            if (isObjectFileOutdated)
            {
                fileStatus = true;
                if (!isSMRuleFileOutdated)
                {
                    checkSMRulesFileOutdatedHeaderUnits();
                }
            }
        }
        else
        {
            // Modules checkOutdated functions do not set callCompleted variables since those are never accessed.
            if (!isObjectFileOutdated)
            {
                checkObjectFileOutdatedModules();
            }
            if (isObjectFileOutdated)
            {
                fileStatus = true;
                if (!isSMRuleFileOutdated)
                {
                    checkSMRulesFileOutdatedModules();
                }
            }
        }

        string smrulesFileOutputClang;
        if (isSMRuleFileOutdated)
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            realBTarget.exitStatus = postCompile.exitStatus;
            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                buildCache.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
                postCompile.parseHeaderDeps(*this);
                smrulesFileOutputClang = std::move(postCompile.commandOutput);
            }
            else
            {
                bool breakpoint = true;
                postCompile.executePrintRoutineRoundOne(*this);
            }
        }

        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            includeNames.reserve(4 * 1024);
            includeNames.clear();
            if (isSMRuleFileOutdated)
            {
                saveSMRulesJsonToSMRulesCache(smrulesFileOutputClang);
            }
            if (type != SM_FILE_TYPE::HEADER_UNIT)
            {
                setSMFileType();
                setLogicalNameAndAddToRequirePath();

                for (auto &[_, logicalName2] : smRulesCache.moduleArray)
                {
                    if (logicalName == logicalName2)
                    {
                        printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                 target->getTarjanNodeName(), node->filePath));
                    }
                }
            }

            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            target->addDependencyDelayed<0>(*this);

            if (isSMRuleFileOutdated)
            {
                huDirPlusTargets.clear();
                for (const auto &singleHuDep : smRulesCache.headerUnitArray)
                {
                    huDirPlusTargets.emplace_back(findHeaderUnitTarget(singleHuDep.node));
                }
                builder.executeMutex.lock();
                initializeHeaderUnits(builder);
                if (!target->addedInCopyJson)
                {
                    targetCacheDiskWriteManager.copyJsonBTargets.emplace_back(target);
                    target->addedInCopyJson = true;
                }
            }
            else
            {
                initializeNewHeaderUnitsSMRulesNotOutdated(builder);
            }
            builder.addNewTopBeUpdatedTargets(this);
            isComplete = true;
        }
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        checkHeaderFilesIfSMRulesJsonSet();
        setFileStatusAndPopulateAllDependencies();

        if (!fileStatus)
        {
            if (compileCommandWithToolCache.hash != target->compileCommandWithTool.getHash())
            {
                fileStatus = true;
            }
        }

        if (fileStatus)
        {
            assignFileStatusToDependents(0);
            const PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
                // cached compile-command would be different
                compileCommandWithToolCache.hash = target->compileCommandWithTool.getHash();
            }
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, target, this);
        }
    }
}

string SMFile::getOutputFileName() const
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        // node->getFileName() is not used to prevent error in case header-file with same fileName exists in 2 different
        // include directories. But is being included by different logicalName.
        string str = logicalName;
        for (char &c : str)
        {
            if (c == '/')
            {
                c = '-';
            }
        }
        return str;
    }
    return node->getFileName();
}

void SMFile::saveSMRulesJsonToSMRulesCache(const string &smrulesFileOutputClang)
{
    // We get half-node since we trust the compiler to have generated if it is returning true
    const Node *smRuleFileNode =
        Node::getHalfNode(target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".smrules");

    Document d;
    // The assumption is that clang only outputs scanning data during scanning on output while MSVC outputs nothing.
    if (smrulesFileOutputClang.empty())
    {
        smRuleFileBuffer = readValueFromFile(smRuleFileNode->filePath, d);
    }
    else
    {
        // ParseInsitu with .data() can be used.
        d.Parse(smrulesFileOutputClang.c_str());
    }

    Value &rule = d.FindMember(svtogsr(JConsts::rules))->value[0];
    smRulesCache = BuildCache::Cpp::ModuleFile::SmRules{};

    if (const auto it = rule.FindMember(svtogsr(JConsts::provides)); it == rule.MemberEnd())
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smRulesCache.exportName = logicalName;
        }
        smRulesCache.isInterface = false;
    }
    else
    {
        Value &provideJson = it->value[0];
        const Value &logicalNameValue = provideJson.FindMember(Value(svtogsr(JConsts::logicalName)))->value;
        const bool isInterface = provideJson.FindMember(Value(svtogsr(JConsts::isInterface)))->value.GetBool();
        smRulesCache.exportName = vtosv(logicalNameValue);
        smRulesCache.isInterface = isInterface;
    }

    // Pushing header-unit array and module-array

    for (auto it = rule.FindMember(svtogsr(JConsts::requires_)); it != rule.MemberEnd(); ++it)
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smRulesCache.headerUnitArray.reserve(it->value.GetArray().Size());
        }
        for (Value &requireValue : it->value.GetArray())
        {
            Value &logicalNameSMRules = requireValue.FindMember(Value(svtogsr(JConsts::logicalName)))->value;

            if (auto sourcePathIt = requireValue.FindMember(Value(svtogsr(JConsts::sourcePath)));
                sourcePathIt == requireValue.MemberEnd())
            {
                // If source-path does not exist, then it is not a header-unit
                // This source-path will be saved before saving and then will be checked in next invocations in
                // resolveRequirePaths function.

                auto &[fullPath, logicalName] = smRulesCache.moduleArray.emplace_back();
                logicalName = vtosv(logicalNameSMRules);
            }
            else
            {
                includeNames.emplace_back(logicalNameSMRules.GetString(), logicalNameSMRules.GetStringLength());

                BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu =
                    smRulesCache.headerUnitArray.emplace_back();

                // lower-cased before saving for further use
                string_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                lowerCasePStringOnWindows(const_cast<char *>(str.data()), str.size());
                hu.node = Node::getHalfNode(str);

                hu.angle = requireValue.FindMember(Value(svtogsr(JConsts::lookupMethod)))->value ==
                           Value(svtogsr(JConsts::includeAngle));
            }
        }
    }
}

InclNodePointerTargetMap SMFile::findHeaderUnitTarget(Node *headerUnitNode) const
{
    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const HeaderUnitNode *nodeDir = nullptr;

    // Iterating over all header-unit-dirs of the target to find out which header-unit dir this header-unit
    // comes from and which target that header-unit-dir belongs to if any
    for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
    {
        if (pathContainsFile(inclNode.node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget && huDirTarget != targetLocal)
            {
                printErrorMessage(FORMAT("Module Header Unit\n{}\n belongs to two different Targets\n{}\n{}\n",
                                         headerUnitNode->filePath, nodeDir->node->filePath, inclNode.node->filePath,
                                         settings.pcSettings.toolErrorOutput));
            }
            huDirTarget = targetLocal;
            nodeDir = &inclNode;
        }
    }

    if (huDirTarget)
    {
        return {nodeDir, huDirTarget};
    }

    if (const auto it = target->configuration->moduleFilesToTarget.find(headerUnitNode);
        it != target->configuration->moduleFilesToTarget.end())
    {
        // The mapped target must be the same as the SMFile target from which this header-unit is discovered or one
        // of its reqDeps
        if (it->second == target || target->reqDeps.find(it->second) != target->reqDeps.end())
        {
            return {nullptr, it->second};
            for (const InclNode &incl : target->reqIncls)
            {
                if (pathContainsFile(incl.node->filePath, headerUnitNode->filePath))
                {
                    return {nullptr, it->second};
                }
            }
            // printErrorMessage("HMake Internal Error");
        }
    }

    string errorMessage = FORMAT("Could not find the target for Header Unit\n{}\ndiscovered in file\n{}\nin "
                                 "Target\n{}.\nSearched for header-unit target in the following reqHuDirs.\n",
                                 headerUnitNode->filePath, node->filePath, target->name);
    for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
    {
        errorMessage += FORMAT("HuDirTarget {} inclNode {}\n", targetLocal->name, inclNode.node->filePath);
    }

    printErrorMessage(errorMessage);
}

void SMFile::initializeNewHeaderUnitsSMRulesNotOutdated(Builder &builder)
{
    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(&headerUnit, hu.angle);
        addDependencyDelayed<0>(headerUnit);
    }

    builder.executeMutex.lock();
    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.addedForRoundOne)
        {
            headerUnit.addedForRoundOne = true;
            builder.updateBTargets.emplace(&headerUnit);
            builder.updateBTargetsSizeGoal += 1;
        }
    }
}

void SMFile::initializeHeaderUnits(Builder &builder)
{
    for (uint32_t i = 0; i < smRulesCache.headerUnitArray.size(); ++i)
    {
        auto &singleHuDep = smRulesCache.headerUnitArray[i];
        auto [nodeDir, huDirTarget] = huDirPlusTargets[i];

        SMFile *headerUnit = nullptr;

        if (const auto it = huDirTarget->headerUnitsSet.find(singleHuDep.node); it == huDirTarget->headerUnitsSet.end())
        {
            headerUnit = new SMFile(huDirTarget, singleHuDep.node);
            // not needed for new header-units since the doubt is only about older header-units that whether they have
            // been added or not.
            headerUnit->addedForRoundOne = true;
            huDirTarget->headerUnitsSet.emplace(headerUnit);

            /*if (nodeDir->ignoreHeaderDeps)
            {
                headerUnit->ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
            }*/

            headerUnit->indexInBuildCache = huDirTarget->newHeaderUnitsSize + huDirTarget->oldHeaderUnits.size();
            ++huDirTarget->newHeaderUnitsSize;

            headerUnit->type = SM_FILE_TYPE::HEADER_UNIT;
            headerUnit->logicalName = string(includeNames[i]);
            headerUnit->buildCache.node = const_cast<Node *>(headerUnit->node);
            headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
        }
        else
        {
            headerUnit = *it;

            if (headerUnit->isAnOlderHeaderUnit && !headerUnit->addedForRoundOne)
            {
                headerUnit->addedForRoundOne = true;
                headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
            }
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(headerUnit, singleHuDep.angle);
        addDependencyDelayed<0>(*headerUnit);

        singleHuDep.targetIndex = huDirTarget->cacheIndex;
        singleHuDep.myIndex = headerUnit->indexInBuildCache;
    }
}

void SMFile::addNewBTargetInFinalBTargetsRound1(Builder &builder)
{
    builder.updateBTargets.emplace(this);
    if (!target->addedInCopyJson)
    {
        targetCacheDiskWriteManager.copyJsonBTargets.emplace_back(target);
        target->addedInCopyJson = true;
    }
    builder.updateBTargetsSizeGoal += 1;
}

void SMFile::setSMFileType()
{
    if (smRulesCache.isInterface)
    {
        if (!smRulesCache.exportName.empty())
        {
            type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        }
    }
    else
    {
        if (!smRulesCache.exportName.empty())
        {
            type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
        }
        else
        {
            type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
        }
    }
}

void SMFile::checkObjectFileOutdatedHeaderUnits()
{
    if (isObjectFileOutdatedCallCompleted || atomic_ref(isObjectFileOutdatedCallCompleted).load())
    {
        return;
    }

    if (checkHeaderFiles(objectFileOutputFileNode))
    {
        isObjectFileOutdated = true;
        atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
        return;
    }

    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];

        if (!localTarget)
        {
            // TODO
            // Maybe we can set isObjectFileSMRuleFileOutdated here as well.
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isObjectFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
        {
            headerUnit.checkObjectFileOutdatedHeaderUnits();
        }

        if (headerUnit.isObjectFileOutdated)
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }
    }
    atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
}

void SMFile::checkSMRulesFileOutdatedHeaderUnits()
{
    // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
    // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
    // .m.o file.

    if (isSMRuleFileOutdatedCallCompleted || atomic_ref(isSMRuleFileOutdatedCallCompleted).load())
    {
        return;
    }

    if (isSMRulesJsonSet)
    {
        isSMRuleFileOutdated = false;
        atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
        return;
    }

    const Node *smRuleFileNode = Node::getNodeFromNonNormalizedPath(
        target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".smrules", true, true);

    if (smRuleFileNode->doesNotExist || node->lastWriteTime > smRuleFileNode->lastWriteTime)

    {
        isSMRuleFileOutdated = true;
        atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
        return;
    }

    // We assume that all header-files systemCheck has already been called and that they exist.
    for (const Node *headerNode : buildCache.headerFiles)
    {
        if (headerNode->lastWriteTime > smRuleFileNode->lastWriteTime)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }
    }

    for (BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        if (!localTarget)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isSMRuleFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isSMRuleFileOutdatedCallCompleted).load())
        {
            headerUnit.checkSMRulesFileOutdatedHeaderUnits();
        }

        if (headerUnit.isSMRuleFileOutdated)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }
    }
    atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
}

void SMFile::checkObjectFileOutdatedModules()
{
    if (checkHeaderFiles(objectFileOutputFileNode))
    {
        isObjectFileOutdated = true;
        return;
    }

    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        if (!localTarget)
        {
            isObjectFileOutdated = true;
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isObjectFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
        {
            headerUnit.checkObjectFileOutdatedHeaderUnits();
        }

        if (headerUnit.isObjectFileOutdated)
        {
            isObjectFileOutdated = true;
            return;
        }
    }
}

void SMFile::checkSMRulesFileOutdatedModules()
{
    // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
    // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
    // .m.o file.

    if (isSMRulesJsonSet)
    {
        isSMRuleFileOutdated = false;
        return;
    }

    const Node *smRuleFileNode = Node::getNodeFromNonNormalizedPath(
        target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".smrules", true, true);

    if (smRuleFileNode->doesNotExist || node->lastWriteTime > smRuleFileNode->lastWriteTime)

    {
        isSMRuleFileOutdated = true;
        return;
    }

    // We assume that all header-files systemCheck has already been called and that they exist.
    for (const Node *headerNode : buildCache.headerFiles)
    {
        if (headerNode->lastWriteTime > smRuleFileNode->lastWriteTime)
        {
            isSMRuleFileOutdated = true;
            return;
        }
    }

    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        if (!localTarget)
        {
            isSMRuleFileOutdated = true;
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isSMRuleFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isSMRuleFileOutdatedCallCompleted).load())
        {
            headerUnit.checkSMRulesFileOutdatedHeaderUnits();
        }

        if (headerUnit.isSMRuleFileOutdated)
        {
            isSMRuleFileOutdated = true;
            return;
        }
    }
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".m.o",
                          pathPrint);
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::updateBuildCache()
{
    BuildCache::Cpp::ModuleFile *modFile;
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        modFile = &target->cppBuildCache.headerUnits[indexInBuildCache];
    }
    else
    {
        modFile = &target->cppBuildCache.modFiles[indexInBuildCache];
    }
    auto &[srcFile, smRules, compileCommandWithTool] = *modFile;
    if (Builder::round == 1)
    {
        // moduleArray is not yet moved as it is used in resolveRequiredPaths and is later updated in
        // SMFile::updateBTarget round 0.
        srcFile = std::move(buildCache);
        smRules.headerUnitArray = std::move(smRulesCache.headerUnitArray);
        smRules.exportName = smRulesCache.exportName;
        smRules.isInterface = smRulesCache.isInterface;
    }
    else
    {
        smRules.moduleArray = std::move(smRulesCache.moduleArray);
        compileCommandWithTool.hash = compileCommandWithToolCache.hash;
    }
}

// TODO
// Propose the idea of big smfile. This combined with markArchivePoint could result in much increased in performance.
void SMFile::setFileStatusAndPopulateAllDependencies()
{
    flat_hash_set<SMFile *> uniqueElements;
    for (auto &[dependency, ignore] : realBTargets[0].dependencies)
    {
        if (dependency->getBTargetType() == BTargetType::SMFILE)
        {
            if (auto *smFile = static_cast<SMFile *>(dependency))
            {
                if (uniqueElements.emplace(smFile).second)
                {
                    for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
                    {
                        uniqueElements.emplace(smFileDep);
                    }
                }
            }
        }
    }

    allSMFileDependenciesRoundZero.reserve(uniqueElements.size());
    for (SMFile *smFile : uniqueElements)
    {
        allSMFileDependenciesRoundZero.emplace_back(smFile);
    }

    if (!fileStatus)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            if (smFile->objectFileOutputFileNode->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
            {
                fileStatus = true;
                break;
            }
        }
    }

    if (fileStatus)
    {
        for (SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            for (auto &h : smFile->headerUnitsConsumptionData)
            {
                headerUnitsConsumptionData.emplace(h);
            }
        }
    }
}

string SMFile::getFlag() const
{
    string str;
    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            str = "/interface ";
            str += "/ifcOutput" +
                   addQuotes(target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc") +
                   " ";
        }
        else if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            str = "/exportHeader ";
            str += "/ifcOutput" + addQuotes(objectFileOutputFileNode->filePath) + " ";
        }
        else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            str = "/internalPartition ";
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += "/Fo" + addQuotes(objectFileOutputFileNode->filePath);
        }
    }
    else if (target->evaluate(BTFamily::GCC))
    {
        // Flags are for clang. I don't know GCC flags at the moment but clang is masqueraded as gcc in HMake so
        // modifying this temporarily on Linux.

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        /*        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
                {
                    str = "/interface ";
                }
                else if (type == SM_FILE_TYPE::HEADER_UNIT)
                {
                    str = "/exportHeader ";
                }
                else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    str = "/internalPartition ";
                }*/

        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str += "-fmodule-output=" +
                   addQuotes(target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc") +
                   " ";
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += "-o " + addQuotes(objectFileOutputFileNode->filePath);
        }
    }

    return str;
}

string SMFile::getFlagPrint() const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    const bool infra = ccpSettings.infrastructureFlags;

    string str;

    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }

        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            str = infra ? "/interface " : "";
            str += infra ? "/ifcOutput" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str +=
                    getReducedPath(target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc",
                                   ccpSettings.ifcOutputFile) +
                    " ";
            }
        }
        else if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            str = infra ? "/exportHeader " : "";
            str += infra ? "/ifcOutput" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(objectFileOutputFileNode->filePath, ccpSettings.ifcOutputFile) + " ";
            }
        }
        else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            str = (infra ? "/internalPartition " : "") + str;
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += infra ? "/Fo" : "";
            if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(objectFileOutputFileNode->filePath, ccpSettings.objectFile) + " ";
            }
        }
    }
    else if (target->evaluate(BTFamily::GCC))
    {

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        /*        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
                {
                    str = (infra ? "/interface " : "");
                }
                else if (type == SM_FILE_TYPE::HEADER_UNIT)
                {
                    str = (infra ? "/exportHeader " : "");
                }
                else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    str = (infra ? "/internalPartition " : "") + str;
                }*/

        /*if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str += infra ? "-fmodule-output=" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(outputFilesWithoutExtension + ".ifc", ccpSettings.ifcOutputFile) + " ";
            }
        }

        str += infra ? "-o" : "";
        if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
        {
            str += getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile) + " ";
        }*/
    }

    return str;
}

string SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{

    string str;

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    }
    if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT ||
        type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
    {
        const string ifcFilePath =
            addQuotes(target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc");
        str = "/reference " + logicalName + "=" + ifcFilePath + " ";
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");

        const string angleStr = dependentSMFile.headerUnitsConsumptionData.find(this)->second ? "angle" : "quote";
        str += "/headerUnit:";
        str += angleStr + " ";
        str += logicalName + "=" + addQuotes(objectFileOutputFileNode->filePath) + " ";
    }
    else
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    }
    return str;
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    const string ifcFilePath = target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const string &logicalName_) {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName_
                   : logicalName_ + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };

    string str;
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
    }
    if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT ||
        type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
    {
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
            if (ccpSettings.infrastructureFlags)
            {
                str += "/reference ";
            }
            str += getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
            if (ccpSettings.infrastructureFlags)
            {
                const string angleStr =
                    dependentSMFile.headerUnitsConsumptionData.find(this)->second ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }

    return str;
}

string SMFile::getModuleCompileCommandPrintLastHalf() const
{
    const CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            moduleCompileCommandPrintLastHalf += smFile->getRequireFlagPrint(*this);
        }
    }

    moduleCompileCommandPrintLastHalf +=
        ccpSettings.infrastructureFlags
            ? target->getInfrastructureFlags(target->configuration->compilerFeatures.compiler, false)
            : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf += getFlagPrint();
    return moduleCompileCommandPrintLastHalf;
}