
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "CppSourceTarget.hpp";
import "JConsts.hpp";
import "PostBasic.hpp";
import "Settings.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <mutex>;
import <utility>;
#else
#include "SMFile.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "PostBasic.hpp"
#include "Settings.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>
#endif

using std::tie, std::ifstream, std::exception, std::lock_guard;

LibDirNode::LibDirNode(Node *node_, bool isStandard_) : node{node_}, isStandard{isStandard_}
{
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode)
{
    for (LibDirNode &libDirNode_ : libDirNodes)
    {
        if (libDirNode_.node == libDirNode.node)
        {
            return;
        }
    }
    libDirNodes.emplace_back(libDirNode);
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, Node *node_, bool isStandard_)
{
    for (LibDirNode &libDirNode : libDirNodes)
    {
        if (libDirNode.node == node_)
        {
            return;
        }
    }
    libDirNodes.emplace_back(node_, isStandard_);
}

InclNode::InclNode(Node *node_, bool isStandard_, bool ignoreHeaderDeps_)
    : LibDirNode(node_, isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
{
}

bool InclNode::emplaceInList(list<InclNode> &includes, InclNode &libDirNode)
{
    for (InclNode &include : includes)
    {
        if (include.node == libDirNode.node)
        {
            return false;
        }
    }
    includes.emplace_back(libDirNode);
    return true;
}

bool InclNode::emplaceInList(list<InclNode> &includes, Node *node_, bool isStandard_, bool ignoreHeaderDeps_)
{
    for (InclNode &include : includes)
    {
        if (include.node == node_)
        {
            return false;
        }
    }
    includes.emplace_back(node_, isStandard_, ignoreHeaderDeps_);
    return true;
}

bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return std::tie(lhs.node, lhs.isStandard, lhs.ignoreHeaderDeps) <
           std::tie(rhs.node, rhs.isStandard, rhs.ignoreHeaderDeps);
}

bool CompareSourceNode::operator()(const SourceNode &lhs, const SourceNode &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareSourceNode::operator()(Node *lhs, const SourceNode &rhs) const
{
    return lhs < rhs.node;
}

bool CompareSourceNode::operator()(const SourceNode &lhs, Node *rhs) const
{
    return lhs.node < rhs;
}

SourceNode::SourceNode(CppSourceTarget *target_, Node *node_) : target(target_), node{node_}
{
    realBTargets.emplace_back(this, 0);
}

pstring SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".o",
                          pathPrint);
}

pstring SourceNode::getTarjanNodeName() const
{
    return node->filePath;
}

void SourceNode::updateBTarget(Builder &, unsigned short round)
{
    if (!round && fileStatus.load(std::memory_order_acquire) && selectiveBuild)
    {
        RealBTarget &realBTarget = realBTargets[round];
        assignFileStatusToDependents(realBTarget);
        PostCompile postCompile = target->updateSourceNodeBTarget(*this);
        postCompile.parseHeaderDeps(*this, false);
        realBTarget.exitStatus = postCompile.exitStatus;
        // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
        // cached compile-command would be different
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            (*sourceJson)[1].SetString(ptoref(target->compileCommandWithTool));
        }
        lock_guard<mutex> lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
}

void SourceNode::setSourceNodeFileStatus()
{
    objectFileOutputFilePath =
        Node::getNodeFromNormalizedString(target->buildCacheFilesDirPath + node->getFileName() + ".o", true, true);

    if (sourceJson->operator[](1) != PValue(ptoref(target->compileCommandWithTool)))
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    if (objectFileOutputFilePath->doesNotExist)
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    if (node->getLastUpdateTime() > objectFileOutputFilePath->getLastUpdateTime())
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    for (PValue &str : (*sourceJson)[2].GetArray())
    {
        Node *headerNode =
            Node::getNodeFromNormalizedString(pstring_view(str.GetString(), str.GetStringLength()), true, true);
        if (headerNode->doesNotExist)
        {
            fileStatus.store(true, std::memory_order_release);
            return;
        }

        if (headerNode->getLastUpdateTime() > objectFileOutputFilePath->getLastUpdateTime())
        {
            fileStatus.store(true, std::memory_order_release);
            return;
        }
    }
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

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, pstring logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
    realBTargets.emplace_back(this, 1);
}


void SMFile::decrementTotalSMRuleFileCount(Builder &builder)
{
    // Subtracts 1 atomically and returns the value held previously
    unsigned int count = target->moduleScopeData->totalSMRuleFileCount.fetch_sub(1);
    if (!(count - 1))
    {
        // All header-units are found, so header-units pvalue array size could be reserved

        for (CppSourceTarget *cppTarget : target->moduleScopeData->targets)
        {
            // If a new header-unit was added in this run, sourceJson pointers are adjusted
            if (cppTarget->newHeaderUnitsSize)
            {
                PValue &headerUnitsPValueArray = (*(cppTarget->targetBuildCache))[2];
                headerUnitsPValueArray.Reserve(headerUnitsPValueArray.Size() + cppTarget->newHeaderUnitsSize,
                                               cppTarget->cppAllocator);

                for (const SMFile *headerUnit : cppTarget->headerUnits)
                {
                    if (headerUnit->headerUnitsIndex == UINT64_MAX)
                    {
                        // headerUnit did not exist before in the cache
                        PValue *oldPtr = headerUnit->sourceJson;

                        // old value is moved
                        headerUnitsPValueArray.PushBack(*(headerUnit->sourceJson), cppTarget->cppAllocator);

                        // old pointer cleared
                        delete oldPtr;

                        // Reassigning the old value moved in the array
                        const_cast<SMFile *>(headerUnit)->sourceJson = (headerUnitsPValueArray.End() - 1);
                    }
                    else
                    {
                        // headerUnit existed before in the cache but because array size is increased the sourceJson
                        // pointer is invalidated now. Reassigning the pointer based on previous index.
                        const_cast<SMFile *>(headerUnit)->sourceJson =
                            &(headerUnitsPValueArray[headerUnit->headerUnitsIndex]);
                    }
                }
            }
        }

        // A smrule file was updated. And all module scope smrule files have been checked. Following is done to
        // write the CppSourceTarget .cache files. So, if because of an error during smrule generation of a file,
        // hmake is exiting after round 1, in next invocation, it won't generate the smrule of successfully
        // generated files.
        builder.addCppSourceTargetsInFinalBTargets(target->moduleScopeData->targets);
    }
}

void SMFile::updateBTarget(Builder &builder, unsigned short round)
{
    // Danger Following is executed concurrently
    RealBTarget &realBTarget = realBTargets[round];
    if (round == 1 && realBTarget.exitStatus == EXIT_SUCCESS)
    {
        if (generateSMFileInRoundOne())
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            {
                lock_guard<mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                fflush(stdout);
            }
            postCompile.parseHeaderDeps(*this, true);
            realBTarget.exitStatus = postCompile.exitStatus;
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            (*sourceJson)[1].SetString(ptoref(target->compileCommandWithTool));
            saveRequiresJsonAndInitializeHeaderUnits(builder);
            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");

            target->realBTargets[0].addDependency(*this);
        }
        decrementTotalSMRuleFileCount(builder);
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatusAndPopulateAllDependencies();

        if (!fileStatus.load(std::memory_order_acquire))
        {
            if ((*sourceJson)[4] != PValue(ptoref(target->compileCommandWithTool)))
            {
                fileStatus.store(true, std::memory_order_release);
            }
        }

        if (fileStatus.load(std::memory_order_acquire))
        {
            assignFileStatusToDependents(realBTarget);
            PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            {
                lock_guard<mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                fflush(stdout);
            }

            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
                // cached compile-command would be different
                (*sourceJson)[4].SetString(ptoref(target->compileCommandWithTool));
            }
        }
    }
}

void SMFile::saveRequiresJsonAndInitializeHeaderUnits(Builder &builder)
{
    if (readJsonFromSMRulesFile)
    {
        pstring smFilePath = target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".smrules";
        PDocument d;
        smRuleFileBuffer = readPValueFromFile(smFilePath, d);

        PValue &rule = d.FindMember(ptoref(JConsts::rules))->value[0];

        PValue &prunedRules = (*sourceJson)[3];
        prunedRules.Clear();

        if (auto it = rule.FindMember(ptoref(JConsts::provides)); it == rule.MemberEnd())
        {
            prunedRules.PushBack(PValue(kStringType), sourceNodeAllocator);
        }
        else
        {
            PValue &logicalNamePValue = it->value[0].FindMember(PValue(ptoref(JConsts::logicalName)))->value;
            prunedRules.PushBack(logicalNamePValue, sourceNodeAllocator);
        }

        prunedRules.PushBack(PValue(kArrayType), sourceNodeAllocator);

        for (auto it = rule.FindMember(ptoref(JConsts::requires_)); it != rule.MemberEnd(); ++it)
        {
            for (PValue &requirePValue : it->value.GetArray())
            {
                prunedRules[1].PushBack(PValue(kArrayType), sourceNodeAllocator);
                PValue &prunedRequirePValue = *(prunedRules[1].End() - 1);

                prunedRequirePValue.PushBack(requirePValue.FindMember(PValue(ptoref(JConsts::logicalName)))->value,
                                             sourceNodeAllocator);

                auto sourcePathIt = requirePValue.FindMember(PValue(ptoref(JConsts::sourcePath)));

                if (sourcePathIt == requirePValue.MemberEnd())
                {
                    // If source-path does not exist, then it is header-unit
                    prunedRequirePValue.PushBack(PValue(kStringType), sourceNodeAllocator);
                    prunedRequirePValue.PushBack(PValue(false), sourceNodeAllocator);
                }
                else
                {
                    // lower-cased before saving for further use
                    pstring_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                    for (const char &c : str)
                    {
                        const_cast<char &>(c) = tolower(c);
                    }

                    // First push source-path, then whether it is header-unit, then lookup-method == include-angle
                    prunedRequirePValue.PushBack(sourcePathIt->value, sourceNodeAllocator);

                    prunedRequirePValue.PushBack(
                        requirePValue.FindMember(PValue(ptoref(JConsts::lookupMethod)))->value ==
                            PValue(ptoref(JConsts::includeAngle)),
                        sourceNodeAllocator);
                }
            }
        }
    }

    if ((*sourceJson)[3][0].GetStringLength())
    {
        logicalName = pstring((*sourceJson)[3][0].GetString(), (*sourceJson)[3][0].GetStringLength());
        target->moduleScopeData->requirePathsMutex.lock();
        auto [pos, ok] = target->moduleScopeData->requirePaths.try_emplace(logicalName, this);
        target->moduleScopeData->requirePathsMutex.unlock();

        if (!ok)
        {
            const auto &[key, val] = *pos;
            printErrorMessageColor(
                fmt::format(
                    "In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                    target->moduleScope->targetSubDir, logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            decrementTotalSMRuleFileCount(builder);
            throw std::exception();
        }
    }
    iterateRequiresJsonToInitializeNewHeaderUnits(builder);
}

// An invariant is that paths are lexically normalized.
bool pathContainsFile(pstring_view dir, pstring_view file)
{
    pstring_view withoutFileName(file.data(), file.find_last_of(slashc));

    if (dir.size() > withoutFileName.size())
    {
        return false;
    }

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more directory components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), withoutFileName.begin());
}

void SMFile::initializeNewHeaderUnit(const PValue &requirePValue, Builder &builder)
{
    ModuleScopeData *moduleScopeData = target->moduleScopeData;
    if (requirePValue[0] == PValue(ptoref(logicalName)))
    {
        printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                           target->moduleScope->targetSubDir, node->filePath),
                               settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    Node *headerUnitNode = Node::getNodeFromNormalizedString(
        pstring_view(requirePValue[1].GetString(), requirePValue[1].GetStringLength()), true);

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const InclNode *nodeDir = nullptr;

    // Iterating over all header-unit-directories of the module-scope to find out which header-unit
    // directory this header-unit comes from and which target that header-unit-directory belongs to
    // if any
    for (const auto &[inclNode, targetLocal] : moduleScopeData->huDirTarget)
    {
        const InclNode *dirNode = inclNode;
        if (pathContainsFile(dirNode->node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget && huDirTarget != targetLocal)
            {
                printErrorMessageColor(fmt::format("Module Header Unit\n{}\n belongs to two different Target Header "
                                                   "Unit Includes\n{}\n{}\nof Module "
                                                   "Scope\n{}\n",
                                                   headerUnitNode->filePath, nodeDir->node->filePath,
                                                   dirNode->node->filePath, target->moduleScope->getTargetPointer()),
                                       settings.pcSettings.toolErrorOutput);
                decrementTotalSMRuleFileCount(builder);
                throw std::exception();
            }
            else
            {
                huDirTarget = targetLocal;
                nodeDir = inclNode;
            }
        }
    }

    if (!huDirTarget)
    {
        printErrorMessageColor(
            fmt::format(
                "Module Header Unit\n{}\n does not belongs to any Target Header Unit Includes of Module Scope\n{}\n",
                headerUnitNode->filePath, target->moduleScope->getTargetPointer()),
            settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    set<SMFile, CompareSourceNode>::const_iterator headerUnitIt;

    target->moduleScopeData->headerUnitsMutex.lock();
    if (auto it = moduleScopeData->headerUnits.find(headerUnitNode); it == moduleScopeData->headerUnits.end())
    {
        headerUnitIt = moduleScopeData->headerUnits.emplace(huDirTarget, headerUnitNode).first;
        target->moduleScopeData->headerUnitsMutex.unlock();
        {
            lock_guard<mutex> lk{huDirTarget->headerUnitsMutex};
            huDirTarget->headerUnits.emplace(&(*headerUnitIt));
        }

        auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

        if (nodeDir->ignoreHeaderDeps)
        {
            headerUnit.ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
        }

        headerUnit.headerUnitsIndex =
            pvalueIndexInSubArray((*(huDirTarget->targetBuildCache))[2], PValue(ptoref(headerUnit.node->filePath)));

        if (headerUnit.headerUnitsIndex != UINT64_MAX)
        {
            headerUnit.sourceJson = &((*(huDirTarget->targetBuildCache))[2][headerUnit.headerUnitsIndex]);
        }
        else
        {
            headerUnit.sourceJson = new PValue(kArrayType);
            ++(huDirTarget->newHeaderUnitsSize);

            headerUnit.sourceJson->PushBack(ptoref(headerUnit.node->filePath), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kStringType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kArrayType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kArrayType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kStringType), headerUnit.sourceNodeAllocator);
        }

        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        {
            ++target->moduleScopeData->totalSMRuleFileCount;
        }

        builder.addNewBTargetInFinalBTargets(&headerUnit);
    }
    else
    {
        target->moduleScopeData->headerUnitsMutex.unlock();
        headerUnitIt = it;
    }

    auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

    // Should be true if JConsts::lookupMetho == "include-angle";
    headerUnitsConsumptionMethods[&headerUnit].emplace(requirePValue[2].GetBool(), requirePValue[0].GetString());
    realBTargets[0].addDependency(headerUnit);
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder)
{
    auto checkForColon = [](const PValue &value) {
        pstring_view str(value.GetString(), value.GetStringLength());
        return str.contains(':');
    };

    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        // Json &rules = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
        for (PValue &requirePValue : (*sourceJson)[3][1].GetArray())
        {
            initializeNewHeaderUnit(requirePValue, builder);
        }
    }
    else
    {
        // If following remains false then source is GlobalModuleFragment.
        bool hasLogicalNameRequireDependency = false;
        // If following is true then smFile is PartitionImplementation.
        bool hasPartitionExportDependency = false;

        for (PValue &requirePValue : (*sourceJson)[3][1].GetArray())
        {
            if (requirePValue[0] == PValue(ptoref(logicalName)))
            {
                printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                                   target->moduleScope->targetSubDir, node->filePath),
                                       settings.pcSettings.toolErrorOutput);
                decrementTotalSMRuleFileCount(builder);
                throw std::exception();
            }

            // It is header-unit if the rule had source-path key
            if (requirePValue[1].GetStringLength())
            {
                initializeNewHeaderUnit(requirePValue, builder);
            }
            else
            {
                hasLogicalNameRequireDependency = true;
                if (checkForColon(requirePValue[0]))
                {
                    hasPartitionExportDependency = true;
                }
            }
        }

        // Value of provides rule key logical-name
        if ((*sourceJson)[3][0].GetStringLength())
        {
            type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        }
        else
        {
            if (hasLogicalNameRequireDependency)
            {
                type = hasPartitionExportDependency ? SM_FILE_TYPE::PARTITION_IMPLEMENTATION
                                                    : SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
            }
            else
            {
                type = SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT;
            }
        }
    }
}

bool SMFile::generateSMFileInRoundOne()
{

    objectFileOutputFilePath =
        Node::getNodeFromNormalizedString(target->buildCacheFilesDirPath + node->getFileName() + ".m.o", true, true);

    if ((*sourceJson)[1] != PValue(ptoref(target->compileCommandWithTool)))
    {
        readJsonFromSMRulesFile = true;
        // This is the only access in round 1. Maybe change to relaxed
        fileStatus.store(true, std::memory_order_release);
        return true;
    }

    bool needsUpdate = false;

    if (objectFileOutputFilePath->doesNotExist)
    {
        needsUpdate = true;
    }
    else
    {
        if (node->getLastUpdateTime() > objectFileOutputFilePath->getLastUpdateTime())
        {
            needsUpdate = true;
        }
        else
        {
            for (const PValue &value : (*sourceJson)[2].GetArray())
            {
                Node *headerNode = Node::getNodeFromNormalizedString(
                    pstring_view(value.GetString(), value.GetStringLength()), true, true);
                if (headerNode->doesNotExist)
                {
                    needsUpdate = true;
                    break;
                }

                if (headerNode->getLastUpdateTime() > objectFileOutputFilePath->getLastUpdateTime())
                {
                    needsUpdate = true;
                    break;
                }
            }
        }
    }

    if (needsUpdate)
    {
        // This is the only access in round 1. Maybe change to relaxed
        fileStatus.store(true, std::memory_order_release);

        readJsonFromSMRulesFile = true;

        // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
        // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
        // .m.o file.

        Node *smRuleFileNode = Node::getNodeFromNonNormalizedPath(
            target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + (".smrules"), true, true);

        if (smRuleFileNode->doesNotExist)
        {
            return true;
        }
        else
        {
            if (node->getLastUpdateTime() > smRuleFileNode->getLastUpdateTime())
            {
                return true;
            }
            else
            {
                for (const PValue &value : (*sourceJson)[2].GetArray())
                {
                    Node *headerNode = Node::getNodeFromNormalizedString(
                        pstring_view(value.GetString(), value.GetStringLength()), true, true);
                    if (headerNode->doesNotExist)
                    {
                        return true;
                    }

                    if (headerNode->getLastUpdateTime() > smRuleFileNode->getLastUpdateTime())
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

pstring SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".m.o",
                          pathPrint);
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::setFileStatusAndPopulateAllDependencies()
{
    auto emplaceInAll = [&](SMFile *smFile) {
        if (const auto &[pos, Ok] = allSMFileDependenciesRoundZero.emplace(smFile); Ok)
        {
            for (auto &[headerUnitSMFile, headerUnitConsumerSet] : smFile->headerUnitsConsumptionMethods)
            {
                for (const HeaderUnitConsumer &headerUnitConsumer : headerUnitConsumerSet)
                {
                    headerUnitsConsumptionMethods.emplace(headerUnitSMFile, set<HeaderUnitConsumer>{})
                        .first->second.emplace(headerUnitConsumer);
                }
            }

            if (!fileStatus.load(std::memory_order_acquire))
            {
                if (smFile->objectFileOutputFilePath->getLastUpdateTime() >
                    objectFileOutputFilePath->getLastUpdateTime())
                {
                    fileStatus.store(true, std::memory_order_release);
                    return;
                }
            }
        }
    };

    for (auto &[dependency, ignore] : realBTargets[0].dependencies)
    {
        if (dependency->getBTargetType() == BTargetType::SMFILE)
        {
            auto *smFile = static_cast<SMFile *>(dependency);
            emplaceInAll(smFile);
            for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
            {
                emplaceInAll(smFileDep);
            }
        }
    }
}

pstring SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    pstring str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
                  addQuotes(outputFilesWithoutExtension + ".m.o");
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED", settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/interface " + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        pstring angleStr = angle ? "angle " : "quote ";
        return "/exportHeader " + str;
    }
    else
    {
        str = "/Fo" + addQuotes(outputFilesWithoutExtension + ".m.o");
        if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            return "/internalPartition " + str;
        }
        else
        {
            return str;
        }
    }
}

pstring SMFile::getFlagPrint(const pstring &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    bool infra = ccpSettings.infrastructureFlags;

    pstring str = infra ? "/ifcOutput" : "";
    if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
    {
        str += getReducedPath(outputFilesWithoutExtension + ".ifc", ccpSettings.ifcOutputFile) + " ";
    }
    str += infra ? "/Fo" : "";
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        str += getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile);
    }

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED", settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return (infra ? "/interface " : "") + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        return (infra ? "/exportHeader " : "") + str;
    }
    else
    {
        str = infra ? "/Fo" : "" + getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile);
        if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            return (infra ? "/internalPartition " : "") + str;
        }
        else
        {
            return str;
        }
    }
}

pstring SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    pstring ifcFilePath =
        addQuotes(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc");

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/reference " + logicalName + "=" + ifcFilePath;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        pstring str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:";
            str += angleStr + " ";
            str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
        }
        return str;
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

pstring SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    pstring ifcFilePath = target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const pstring &logicalName_) {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName_
                   : logicalName_ + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            return "";
        }
        else
        {
            pstring str;
            if (ccpSettings.infrastructureFlags)
            {
                str += "/reference ";
            }
            return str + getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            return "";
        }
        else
        {
            pstring str;
            for (const HeaderUnitConsumer &headerUnitConsumer :
                 dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
            {
                if (ccpSettings.infrastructureFlags)
                {
                    pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                    str += "/headerUnit:" + angleStr + " ";
                }
                str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
            }
            return str;
        }
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

pstring SMFile::getModuleCompileCommandPrintLastHalf()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    pstring moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            moduleCompileCommandPrintLastHalf += smFile->getRequireFlagPrint(*this);
        }
    }

    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? target->getInfrastructureFlags(false) : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf +=
        getFlagPrint(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)());
    return moduleCompileCommandPrintLastHalf;
}