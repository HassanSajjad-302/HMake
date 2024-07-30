
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

LibDirNode::LibDirNode(Node *node_, const bool isStandard_) : node{node_}, isStandard{isStandard_}
{
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode)
{
    for (const LibDirNode &libDirNode_ : libDirNodes)
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
    for (const LibDirNode &libDirNode : libDirNodes)
    {
        if (libDirNode.node == node_)
        {
            return;
        }
    }
    libDirNodes.emplace_back(node_, isStandard_);
}

InclNode::InclNode(Node *node_, const bool isStandard_, const bool ignoreHeaderDeps_)
    : LibDirNode(node_, isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
{
}

bool InclNode::emplaceInList(list<InclNode> &includes, InclNode &libDirNode)
{
    for (const InclNode &include : includes)
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
    for (const InclNode &include : includes)
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

pstring SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".o",
                          pathPrint);
}

pstring SourceNode::getTarjanNodeName() const
{
    return node->filePath;
}

void SourceNode::updateBTarget(Builder &, const unsigned short round)
{
    if (!round && fileStatus.load() && selectiveBuild)
    {
        RealBTarget &realBTarget = realBTargets[0];
        assignFileStatusToDependents(realBTarget);
        PostCompile postCompile = target->updateSourceNodeBTarget(*this);
        postCompile.parseHeaderDeps(*this, false);
        realBTarget.exitStatus = postCompile.exitStatus;
        // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
        // cached compile-command would be different
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            (*sourceJson)[Indices::TargetBuildCache::SourceFiles::compileCommandWithTool] =
                target->compileCommandWithTool.getHash();
        }
        lock_guard lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
}

void SourceNode::setSourceNodeFileStatus()
{
    using SourceFiles = Indices::TargetBuildCache::SourceFiles;

    objectFileOutputFilePath =
        Node::getNodeFromNormalizedString(target->buildCacheFilesDirPath + node->getFileName() + ".o", true, true);

    if ((*sourceJson)[SourceFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
    {
        fileStatus.store(true);
        return;
    }

    if (objectFileOutputFilePath->doesNotExist)
    {
        fileStatus.store(true);
        return;
    }

    if (node->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
    {
        fileStatus.store(true);
        return;
    }

    for (PValue &str : (*sourceJson)[SourceFiles::headerFiles].GetArray())
    {
        const Node *headerNode =
            Node::getNodeFromNormalizedString(pstring_view(str.GetString(), str.GetStringLength()), true, true);
        if (headerNode->doesNotExist)
        {
            fileStatus.store(true);
            return;
        }

        if (headerNode->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
        {
            fileStatus.store(true);
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

HeaderUnitConsumer::HeaderUnitConsumer(const bool angle_, pstring logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

PValueObjectFileMapping::PValueObjectFileMapping(PValue *requireJson_, Node *objectFileOutputFilePath_)
    : requireJson(requireJson_), objectFileOutputFilePath(objectFileOutputFilePath_)
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round)
{

    using ModuleFiles = Indices::TargetBuildCache::ModuleFiles;

    // Danger Following is executed concurrently
    RealBTarget &realBTarget = realBTargets[round];
    if (round == 1 && realBTarget.exitStatus == EXIT_SUCCESS)
    {
        pstring smrulesFileOutputClang;
        if (shouldGenerateSMFileInRoundOne())
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            postCompile.parseHeaderDeps(*this, true);
            {
                lock_guard lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                fflush(stdout);
            }
            realBTarget.exitStatus = postCompile.exitStatus;
            smrulesFileOutputClang = std::move(postCompile.commandSuccessOutput);
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            (*sourceJson)[Indices::TargetBuildCache::ModuleFiles::scanningCommandWithTool] =
                target->compileCommandWithTool.getHash();
            saveSMRulesJsonToSourceJson(smrulesFileOutputClang);
            iterateRequiresJsonToInitializeNewHeaderUnits(builder);
            assert(type != SM_FILE_TYPE::NOT_assignED && "Type Not Assigned");

            target->realBTargets[0].addDependency(*this);
        }
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatusAndPopulateAllDependencies();

        if (!fileStatus.load())
        {
            if ((*sourceJson)[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
            {
                fileStatus.store(true);
            }
        }

        if (fileStatus.load())
        {
            assignFileStatusToDependents(realBTarget);
            const PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            {
                lock_guard lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                fflush(stdout);
            }

            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
                // cached compile-command would be different
                (*sourceJson)[ModuleFiles::compileCommandWithTool] = target->compileCommandWithTool.getHash();

                for (const PValueObjectFileMapping &mapping : pValueObjectFileMapping)
                {
                    using SingleModuleDep = Indices::TargetBuildCache::ModuleFiles::SmRules::SingleModuleDep;
                    (*mapping.requireJson)[SingleModuleDep::fullPath] =
                        PValue(mapping.objectFileOutputFilePath->filePath.c_str(),
                               mapping.objectFileOutputFilePath->filePath.size());
                }
            }
        }
    }
}

void SMFile::saveSMRulesJsonToSourceJson(const pstring &smrulesFileOutputClang)
{

    using ModuleFile = Indices::TargetBuildCache::ModuleFiles;
    using SMRules = ModuleFile::SmRules;

    if (readJsonFromSMRulesFile)
    {
        const pstring smFilePath =
            target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".smrules";
        PDocument d;
        // The assumptions is that clang only outputs scanning data during scanning on output while MSVC outputs
        // nothing.
        if (smrulesFileOutputClang.empty())
        {
            smRuleFileBuffer = readPValueFromFile(smFilePath, d);
        }
        else
        {
            // ParseInsitu with .data() can be used.
            d.Parse(smrulesFileOutputClang.c_str());
        }

        PValue &rule = d.FindMember(ptoref(JConsts::rules))->value[0];

        PValue &prunedRules = (*sourceJson)[ModuleFile::smRules];
        prunedRules.Clear();

        if (const auto it = rule.FindMember(ptoref(JConsts::provides)); it == rule.MemberEnd())
        {
            prunedRules.PushBack(PValue(kStringType), sourceNodeAllocator);
            prunedRules.PushBack(PValue(false), sourceNodeAllocator);
        }
        else
        {
            PValue &provideJson = it->value[0];
            PValue &logicalNamePValue = provideJson.FindMember(PValue(ptoref(JConsts::logicalName)))->value;
            PValue &isInterfacePValue = provideJson.FindMember(PValue(ptoref(JConsts::isInterface)))->value;
            prunedRules.PushBack(logicalNamePValue, sourceNodeAllocator);
            prunedRules.PushBack(isInterfacePValue, sourceNodeAllocator);
        }

        prunedRules.PushBack(PValue(kArrayType), sourceNodeAllocator);

        for (auto it = rule.FindMember(ptoref(JConsts::requires_)); it != rule.MemberEnd(); ++it)
        {
            for (PValue &requirePValue : it->value.GetArray())
            {
                prunedRules[SMRules::requireArray].PushBack(PValue(kArrayType), sourceNodeAllocator);
                PValue &prunedRequirePValue = *(prunedRules[SMRules::requireArray].End() - 1);

                prunedRequirePValue.PushBack(requirePValue.FindMember(PValue(ptoref(JConsts::logicalName)))->value,
                                             sourceNodeAllocator);

                auto sourcePathIt = requirePValue.FindMember(PValue(ptoref(JConsts::sourcePath)));

                if (sourcePathIt == requirePValue.MemberEnd())
                {
                    // If source-path does not exist, then it is not a header-unit
                    // This source-path will be saved before saving and then will be checked in next invocations in
                    // resolveRequirePaths function.
                    prunedRequirePValue.PushBack(PValue(false), sourceNodeAllocator);
                    prunedRequirePValue.PushBack(PValue(kStringType), sourceNodeAllocator);
                    prunedRequirePValue.PushBack(PValue(false), sourceNodeAllocator);
                }
                else
                {
                    prunedRequirePValue.PushBack(PValue(true), sourceNodeAllocator);
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

    if ((*sourceJson)[ModuleFile::smRules][SMRules::exportName].GetStringLength())
    {
        logicalName = pstring((*sourceJson)[ModuleFile::smRules][SMRules::exportName].GetString(),
                              (*sourceJson)[ModuleFile::smRules][SMRules::exportName].GetStringLength());
        target->requirePathsMutex.lock();
        auto [pos, ok] = target->requirePaths.try_emplace(logicalName, this);
        target->requirePathsMutex.unlock();

        if (!ok)
        {
            const auto &[key, val] = *pos;
            printErrorMessageColor(
                fmt::format("In target:\n{}\nModule name:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                            target->getTarjanNodeName(), logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }
    }
}

// An invariant is that paths are lexically normalized.
bool pathContainsFile(pstring_view dir, const pstring_view file)
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
    using SingleModuleDep = Indices::TargetBuildCache::ModuleFiles::SmRules::SingleModuleDep;

    if (requirePValue[SingleModuleDep::logicalName] == PValue(ptoref(logicalName)))
    {
        printErrorMessageColor(fmt::format("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                           target->getTarjanNodeName(), node->filePath),
                               settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }

    Node *headerUnitNode =
        Node::getNodeFromNormalizedString(pstring_view(requirePValue[SingleModuleDep::fullPath].GetString(),
                                                       requirePValue[SingleModuleDep::fullPath].GetStringLength()),
                                          true);

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const InclNode *nodeDir = nullptr;

    // Iterating over all header-unit-directories of the target to find out which header-unit directory this header-unit
    // comes from and which target that header-unit-directory belongs to if any
    for (const auto &[inclNode, targetLocal] : target->requirementHuDirs)
    {
        if (pathContainsFile(inclNode.node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget && huDirTarget != targetLocal)
            {
                printErrorMessageColor(fmt::format("Module Header Unit\n{}\n belongs to two different Target Header "
                                                   "Unit Includes\n{}\n{}\n",
                                                   headerUnitNode->filePath, nodeDir->node->filePath,
                                                   inclNode.node->filePath, settings.pcSettings.toolErrorOutput),
                                       settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }
            huDirTarget = targetLocal;
            nodeDir = &inclNode;
        }
    }

    if (!huDirTarget)
    {
        printErrorMessageColor(
            fmt::format("Could not find the target for Header Unit\n{}\ndiscovered in file\n{}\nin Target\n{}\n",
                        headerUnitNode->filePath, node->filePath, target->getTargetPointer()),
            settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }

    set<SMFile, CompareSourceNode>::const_iterator headerUnitIt;

    huDirTarget->headerUnitsMutex.lock();
    if (const auto it = huDirTarget->headerUnits.find(headerUnitNode); it == huDirTarget->headerUnits.end())
    {
        headerUnitIt = huDirTarget->headerUnits.emplace(huDirTarget, headerUnitNode).first;
        huDirTarget->headerUnitsMutex.unlock();

        auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

        if (nodeDir->ignoreHeaderDeps)
        {
            headerUnit.ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
        }

        headerUnit.headerUnitsIndex =
            pvalueIndexInSubArray((*huDirTarget->targetBuildCache)[Indices::TargetBuildCache::headerUnits],
                                  PValue(ptoref(headerUnit.node->filePath)));

        if (headerUnit.headerUnitsIndex != UINT64_MAX)
        {
            headerUnit.sourceJson =
                &(*huDirTarget->targetBuildCache)[Indices::TargetBuildCache::headerUnits][headerUnit.headerUnitsIndex];
        }
        else
        {
            headerUnit.sourceJson = new PValue(kArrayType);
            ++huDirTarget->newHeaderUnitsSize;

            headerUnit.sourceJson->PushBack(ptoref(headerUnit.node->filePath), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kStringType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kArrayType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kArrayType), headerUnit.sourceNodeAllocator);
            headerUnit.sourceJson->PushBack(PValue(kStringType), headerUnit.sourceNodeAllocator);
        }

        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;

        headerUnit.addNewBTargetInFinalBTargets(builder);
    }
    else
    {
        huDirTarget->headerUnitsMutex.unlock();
        headerUnitIt = it;
    }

    auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

    // Should be true if JConsts::lookupMethod == "include-angle";
    headerUnitsConsumptionMethods[&headerUnit].emplace(requirePValue[SingleModuleDep::boolean].GetBool(),
                                                       requirePValue[SingleModuleDep::logicalName].GetString());
    realBTargets[0].addDependency(headerUnit);
}

void SMFile::addNewBTargetInFinalBTargets(Builder &builder)
{
    {
        std::lock_guard lk(builder.executeMutex);
        builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, this);
        target->adjustHeaderUnitsBTarget.realBTargets[1].addDependency(*this);
        builder.updateBTargetsSizeGoal += 1;
    }
    builder.cond.notify_all();
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder)
{
    using ModuleFiles = Indices::TargetBuildCache::ModuleFiles;

    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        for (PValue &requirePValue : (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::requireArray].GetArray())
        {
            initializeNewHeaderUnit(requirePValue, builder);
        }
    }
    else
    {
        for (PValue &requirePValue : (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::requireArray].GetArray())
        {
            using SingleModuleDep = Indices::TargetBuildCache::ModuleFiles::SmRules::SingleModuleDep;
            if (requirePValue[SingleModuleDep::logicalName] == PValue(ptoref(logicalName)))
            {
                printErrorMessageColor(fmt::format("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                   target->getTarjanNodeName(), node->filePath),
                                       settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }

            // It is header-unit if the rule had source-path key
            if (requirePValue[SingleModuleDep::isHeaderUnit].GetBool())
            {
                initializeNewHeaderUnit(requirePValue, builder);
            }
        }

        if ((*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::isInterface].GetBool())
        {
            if ((*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::exportName].GetStringLength())
            {
                type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
            }
        }
        else
        {
            if ((*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::exportName].GetStringLength())
            {
                type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
            }
            else
            {
                type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
            }
        }
    }
}

bool SMFile::shouldGenerateSMFileInRoundOne()
{

    using ModuleFiles = Indices::TargetBuildCache::ModuleFiles;

    objectFileOutputFilePath =
        Node::getNodeFromNormalizedString(target->buildCacheFilesDirPath + node->getFileName() + ".m.o", true, true);

    if ((*sourceJson)[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
    {
        readJsonFromSMRulesFile = true;
        // This is the only access in round 1. Maybe change to relaxed
        fileStatus.store(true);
        return true;
    }

    bool needsUpdate = false;

    if (objectFileOutputFilePath->doesNotExist)
    {
        needsUpdate = true;
    }
    else
    {
        if (node->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
        {
            needsUpdate = true;
        }
        else
        {
            for (const PValue &value : (*sourceJson)[ModuleFiles::headerFiles].GetArray())
            {
                const Node *headerNode = Node::getNodeFromNormalizedString(
                    pstring_view(value.GetString(), value.GetStringLength()), true, true);
                if (headerNode->doesNotExist)
                {
                    needsUpdate = true;
                    break;
                }

                if (headerNode->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
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
        fileStatus.store(true);

        readJsonFromSMRulesFile = true;

        // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
        // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
        // .m.o file.

        if (const Node *smRuleFileNode = Node::getNodeFromNonNormalizedPath(
                target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".smrules", true, true);
            smRuleFileNode->doesNotExist)
        {
            return true;
        }
        else
        {
            if (node->lastWriteTime > smRuleFileNode->lastWriteTime)
            {
                return true;
            }

            for (const PValue &value : (*sourceJson)[ModuleFiles::headerFiles].GetArray())
            {
                const Node *headerNode = Node::getNodeFromNormalizedString(
                    pstring_view(value.GetString(), value.GetStringLength()), true, true);
                if (headerNode->doesNotExist)
                {
                    return true;
                }

                if (headerNode->lastWriteTime > smRuleFileNode->lastWriteTime)
                {
                    return true;
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

            if (!fileStatus.load())
            {
                if (smFile->objectFileOutputFilePath->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
                {
                    fileStatus.store(true);
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
    pstring str;
    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED",
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }
        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
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
        }

        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str += "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " ";
        }

        str += "/Fo" + addQuotes(outputFilesWithoutExtension + ".m.o ");
    }
    else if (target->evaluate(BTFamily::GCC))
    {
        // Flags are for clang. I don't know GCC flags at the moment but clang is masqueraded as gcc in HMake so
        // modifying this temporarily on Linux.

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED",
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
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
            str += "-fmodule-output=" + addQuotes(outputFilesWithoutExtension + ".ifc") + " ";
        }

        str += "-o " + addQuotes(outputFilesWithoutExtension + ".m.o ");
    }

    return str;
}

pstring SMFile::getFlagPrint(const pstring &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    const bool infra = ccpSettings.infrastructureFlags;

    pstring str;

    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED",
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }

        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            str = infra ? "/interface " : "";
        }
        else if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            str = infra ? "/exportHeader " : "";
        }
        else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            str = (infra ? "/internalPartition " : "") + str;
        }

        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str += infra ? "/ifcOutput" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(outputFilesWithoutExtension + ".ifc", ccpSettings.ifcOutputFile) + " ";
            }
        }

        str += infra ? "/Fo" : "";
        if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
        {
            str += getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile) + " ";
        }
    }
    else if (target->evaluate(BTFamily::GCC))
    {

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED",
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
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

        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
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
        }
    }

    return str;
}

pstring SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    const pstring ifcFilePath =
        addQuotes(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc");

    string str;

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
        throw std::exception();
    }
    if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT ||
        type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
    {
        str = "/reference " + logicalName + "=" + ifcFilePath + " ";
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:";
            str += angleStr + " ";
            str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
        }
    }
    else
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
        throw std::exception();
    }
    return str;
}

pstring SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    const pstring ifcFilePath = target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const pstring &logicalName_) {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName_
                   : logicalName_ + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };

    pstring str;
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        throw std::exception();
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
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
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
        }
    }

    return str;
}

pstring SMFile::getModuleCompileCommandPrintLastHalf() const
{
    const CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
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