
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "JConsts.hpp";
import "RunCommand.hpp";
import "Settings.hpp";
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
#include "TargetCacheDiskWriteManager.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
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

HeaderUnitIndexInfo::HeaderUnitIndexInfo(const uint64_t targetIndex_, const uint64_t myIndex_)
    : targetIndex(targetIndex_), myIndex(myIndex_)
{
}

Value &HeaderUnitIndexInfo::getSingleHeaderUnitDep() const
{
    namespace CppBuild = Indices::BuildCache::CppBuild;
    return buildCache[targetIndex][CppBuild::headerUnits][myIndex];
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

void SourceNode::initializeSourceJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                      const CppSourceTarget &target)
{
    // Indices::BuildCache::CppBuild::SourceFiles::fullPath
    j.PushBack(node->getValue(), sourceNodeAllocator);

    // Indices::BuildCache::CppBuild::SourceFiles::compileCommandWithTool
    j.PushBack(Value(kStringType), sourceNodeAllocator);

    // Indices::BuildCache::CppBuild::SourceFiles::headerFiles
    j.PushBack(Value(kArrayType), sourceNodeAllocator);
}

void SourceNode::updateBTarget(Builder &builder, const unsigned short round)
{
    if (!round)
    {
        RealBTarget &realBTarget = realBTargets[0];
        if (selectiveBuild)
        {
            objectFileOutputFilePath = Node::getNodeFromNormalizedString(
                target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".o", true, true);

            setSourceNodeFileStatus();
            if (atomic_ref(fileStatus).load())
            {
                namespace CppBuild = Indices::BuildCache::CppBuild;

                assignFileStatusToDependents(0);
                PostCompile postCompile = target->updateSourceNodeBTarget(*this);
                realBTarget.exitStatus = postCompile.exitStatus;
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
                // because cached compile-command would be different
                if (realBTarget.exitStatus == EXIT_SUCCESS)
                {
                    sourceJson[CppBuild::SourceFiles::compileCommandWithTool] =
                        target->compileCommandWithTool.getHash();
                    postCompile.parseHeaderDeps(*this);
                }

                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false, std::move(sourceJson),
                                                target->targetCacheIndex, CppBuild::sourceFiles, indexInBuildCache);
            }
        }
    }
}

bool SourceNode::checkHeaderFiles(const Node *compareNode) const
{
    namespace SourceFiles = Indices::BuildCache::CppBuild::SourceFiles;
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SMRules = ModuleFiles::SmRules;

    vector<Node *> headerFilesUnChecked;

    headerFilesUnChecked.reserve(sourceJson[SourceFiles::headerFiles].GetArray().Size());

    for (const Value &pValue : sourceJson[SourceFiles::headerFiles].GetArray())
    {
        bool systemCheckCompleted = false;
        Node *headerNode = Node::tryGetNodeFromValue(systemCheckCompleted, pValue, true, true);
        if (systemCheckCompleted)
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
                if (Node *headerNode = headerFilesUnChecked[i]; atomic_ref(headerNode->systemCheckCompleted).load())
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
    // has more directory components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), withoutFileName.begin());
}

void SourceNode::setSourceNodeFileStatus()
{
    namespace SourceFiles = Indices::BuildCache::CppBuild::SourceFiles;

    if (sourceJson[SourceFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
    {
        atomic_ref(fileStatus).store(true);
        return;
    }

    if (objectFileOutputFilePath->doesNotExist)
    {
        atomic_ref(fileStatus).store(true);
        return;
    }

    if (node->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
    {
        atomic_ref(fileStatus).store(true);
        return;
    }

    if (checkHeaderFiles(objectFileOutputFilePath))
    {
        atomic_ref(fileStatus).store(true);
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

HeaderUnitConsumer::HeaderUnitConsumer(const bool angle_, string logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

ValueObjectFileMapping::ValueObjectFileMapping(Value *requireJson_, Node *objectFileOutputFilePath_)
    : requireJson(requireJson_), objectFileOutputFilePath(objectFileOutputFilePath_)
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_, bool olderHeaderUnit)
    : SourceNode(target_, node_, false, false, false), type(type_)
{
}

void SMFile::setSMRulesJson(string_view smRulesJson)
{
    smRuleFileBuffer = std::make_unique<vector<char>>(smRulesJson.size() + 1);
    (*smRuleFileBuffer)[smRulesJson.size()] = '\0';

    for (uint64_t i = 0; i < smRulesJson.size(); ++i)
    {
        (*smRuleFileBuffer)[i] = smRulesJson[i];
    }

    sourceJson[Indices::BuildCache::CppBuild::ModuleFiles::smRules] =
        Document(kArrayType).ParseInsitu(&(*smRuleFileBuffer)[0]).Move();

    isSMRulesJsonSet = true;
}

void SMFile::checkHeaderFilesIfSMRulesJsonSet()
{
    if (isSMRulesJsonSet)
    {
        // Check all header-files and set fileStatus to true.
        namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

        if (sourceJson[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        if (objectFileOutputFilePath->doesNotExist)
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        if (node->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        // While header-units are checked in shouldGenerateSMFileInRoundOne, they are not checked here.
        // Because, if a header-unit of ours is updated, because we are dependent of it, we will be updated as-well.
        if (checkHeaderFiles(objectFileOutputFilePath))
        {
            atomic_ref(fileStatus).store(true);
        }
    }
}

void SMFile::setLogicalNameAndAddToRequirePath()
{
    namespace ModuleFile = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SMRules = ModuleFile::SmRules;

    if (sourceJson[ModuleFile::smRules][SMRules::exportName].GetStringLength())
    {
        logicalName = string(sourceJson[ModuleFile::smRules][SMRules::exportName].GetString(),
                             sourceJson[ModuleFile::smRules][SMRules::exportName].GetStringLength());

        using Map = decltype(requirePaths2);

        const RequireNameTargetId req(this->target->id, logicalName);
        if (const SMFile *val = nullptr; requirePaths2.lazy_emplace_l(
                req, [&](const Map::value_type &val_) { val = const_cast<SMFile *>(val_.second); },
                [&](const Map::constructor &constructor) { constructor(req, this); }))
        {
        }
        else
        {
            printErrorMessageColor(
                FORMAT("In target:\n{}\nModule name:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                       target->getTarjanNodeName(), logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }
    }
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    // Danger Following is executed concurrently
    RealBTarget &realBTarget = realBTargets[round];
    if (round == 2 && type == SM_FILE_TYPE::HEADER_UNIT)
    {
        const_cast<Node *>(node)->ensureSystemCheckCalled(true, true);

        sourceJson.CopyFrom(target->getBuildCache()[Indices::BuildCache::CppBuild::headerUnits][indexInBuildCache],
                            sourceNodeAllocator);

        objectFileOutputFilePath = Node::getNodeFromNormalizedString(
            target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".m.o", true, true);

        if (sourceJson[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
            isSMRuleFileOutdated = true;
            isSMRuleFileOutdatedCallCompleted = true;
            return;
        }

        if (node->doesNotExist || objectFileOutputFilePath->doesNotExist ||
            node->lastWriteTime > objectFileOutputFilePath->lastWriteTime || checkHeaderFiles(objectFileOutputFilePath))
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
        }
    }
    else if (round == 1 && realBTarget.exitStatus == EXIT_SUCCESS)
    {
        if (!foundFromCache)
        {
            objectFileOutputFilePath = Node::getNodeFromNormalizedString(
                target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".m.o", true, true);

            if (sourceJson[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
            {
                isObjectFileOutdated = true;
                atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
                isSMRuleFileOutdated = true;
                atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            }

            // TODO
            // Change this to "else if" and move it into a function.
            // doesNotExist should be treated a bit differently
            if (node->doesNotExist || objectFileOutputFilePath->doesNotExist ||
                node->lastWriteTime > objectFileOutputFilePath->lastWriteTime ||
                checkHeaderFiles(objectFileOutputFilePath))
            {
                isObjectFileOutdated = true;
                atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            }
        }

        string smrulesFileOutputClang;
        isObjectFileOutdatedComparedToSourceFileAndItsDeps();
        if (isObjectFileOutdated)
        {
            fileStatus = true;
            isSMRulesFileOutdatedComparedToSourceFileAndItsDeps();
        }

        if (isSMRuleFileOutdated)
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                sourceJson[ModuleFiles::scanningCommandWithTool] = target->compileCommandWithTool.getHash();
                postCompile.parseHeaderDeps(*this);
            }
            else
            {
                // TODO
                // print the error output.
            }
            realBTarget.exitStatus = postCompile.exitStatus;
            smrulesFileOutputClang = std::move(postCompile.commandOutput);
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            if (isObjectFileOutdated)
            {
                saveSMRulesJsonToSourceJson(smrulesFileOutputClang);
            }
            setLogicalNameAndAddToRequirePath();

            for (Value &requireValue : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::moduleArray].GetArray())
            {
                namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;
                if (requireValue[SingleModuleDep::logicalName] == Value(svtogsr(logicalName)))
                {
                    printErrorMessageColor(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                  target->getTarjanNodeName(), node->filePath),
                                           settings.pcSettings.toolErrorOutput);
                    throw std::exception();
                }
            }

            initializeHeaderUnits(builder);
            if (type != SM_FILE_TYPE::HEADER_UNIT)
            {
                setSMFileType(builder);
                if (isSMRuleFileOutdated)
                {
                    target->moduleFileScanned = true;
                }
            }

            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            target->addDependency<0>(*this);
        }
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        checkHeaderFilesIfSMRulesJsonSet();
        setFileStatusAndPopulateAllDependencies();

        if (!atomic_ref(fileStatus).load())
        {
            if (sourceJson[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
            {
                atomic_ref(fileStatus).store(true);
            }
        }

        if (atomic_ref(fileStatus).load())
        {
            assignFileStatusToDependents(0);
            const PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
                // cached compile-command would be different
                sourceJson[ModuleFiles::compileCommandWithTool] = target->compileCommandWithTool.getHash();

                for (const ValueObjectFileMapping &mapping : pValueObjectFileMapping)
                {
                    namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;
                    (*mapping.requireJson)[SingleModuleDep::fullPath] = mapping.objectFileOutputFilePath->getValue();
                }
            }

            if (type == SM_FILE_TYPE::HEADER_UNIT)
            {
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false, std::move(sourceJson),
                                                target->targetCacheIndex, Indices::BuildCache::CppBuild::headerUnits,
                                                indexInBuildCache);
            }
            else
            {
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false, std::move(sourceJson),
                                                target->targetCacheIndex, Indices::BuildCache::CppBuild::moduleFiles,
                                                indexInBuildCache);
            }
        }
    }
}

void SMFile::saveSMRulesJsonToSourceJson(const string &smrulesFileOutputClang)
{
    namespace ModuleFile = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SMRules = ModuleFile::SmRules;

    // We get half-node since we trust the compiler to have generated if it is returning true
    const Node *smRuleFileNode = Node::getHalfNodeFromNormalizedString(target->buildCacheFilesDirPathNode->filePath +
                                                                       slashc + node->getFileName() + ".smrules");

    Document d;
    // The assumptions is that clang only outputs scanning data during scanning on output while MSVC outputs
    // nothing.
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

    Value &prunedRules = sourceJson[ModuleFile::smRules];
    prunedRules.Clear();

    if (const auto it = rule.FindMember(svtogsr(JConsts::provides)); it == rule.MemberEnd())
    {
        prunedRules.PushBack(Value(kStringType), sourceNodeAllocator);
        prunedRules.PushBack(Value(false), sourceNodeAllocator);
    }
    else
    {
        Value &provideJson = it->value[0];
        Value &logicalNameValue = provideJson.FindMember(Value(svtogsr(JConsts::logicalName)))->value;
        Value &isInterfaceValue = provideJson.FindMember(Value(svtogsr(JConsts::isInterface)))->value;
        prunedRules.PushBack(logicalNameValue, sourceNodeAllocator);
        prunedRules.PushBack(isInterfaceValue, sourceNodeAllocator);
    }

    prunedRules.PushBack(Value(kArrayType), sourceNodeAllocator);
    prunedRules.PushBack(Value(kArrayType), sourceNodeAllocator);

    for (auto it = rule.FindMember(svtogsr(JConsts::requires_)); it != rule.MemberEnd(); ++it)
    {
        for (Value &requireValue : it->value.GetArray())
        {
            Value &logicalName = requireValue.FindMember(Value(svtogsr(JConsts::logicalName)))->value;

            if (auto sourcePathIt = requireValue.FindMember(Value(svtogsr(JConsts::sourcePath)));
                sourcePathIt == requireValue.MemberEnd())
            {
                prunedRules[SMRules::moduleArray].PushBack(Value(kArrayType), sourceNodeAllocator);
                Value &moduleDepValue = *(prunedRules[SMRules::moduleArray].End() - 1);

                // If source-path does not exist, then it is not a header-unit
                // This source-path will be saved before saving and then will be checked in next invocations in
                // resolveRequirePaths function.
                moduleDepValue.PushBack(Node::getType(), sourceNodeAllocator);
                moduleDepValue.PushBack(logicalName, sourceNodeAllocator);
            }
            else
            {
                prunedRules[SMRules::headerUnitArray].PushBack(Value(kArrayType), sourceNodeAllocator);
                Value &headerUnitDepValue = *(prunedRules[SMRules::headerUnitArray].End() - 1);

                // lower-cased before saving for further use
                string_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                lowerCasePStringOnWindows(const_cast<char *>(str.data()), str.size());
                const Node *halfHeaderUnitNode = Node::getHalfNodeFromNormalizedString(str);

                // fullPath
                headerUnitDepValue.PushBack(halfHeaderUnitNode->getValue(), sourceNodeAllocator);
                // logicalName
                headerUnitDepValue.PushBack(logicalName, sourceNodeAllocator);
                // angle
                headerUnitDepValue.PushBack(requireValue.FindMember(Value(svtogsr(JConsts::lookupMethod)))->value ==
                                                Value(svtogsr(JConsts::includeAngle)),
                                            sourceNodeAllocator);

                // These values are initialized later in initializeHeaderUnits.
                // targetIndex
                headerUnitDepValue.PushBack(Value(UINT64_MAX), sourceNodeAllocator);
                // myIndex
                headerUnitDepValue.PushBack(Value(UINT64_MAX), sourceNodeAllocator);
            }
        }
    }
}

void SMFile::initializeModuleJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                  const CppSourceTarget &target)
{
    j.PushBack(node->getValue(), sourceNodeAllocator);
    j.PushBack(Value(kStringType), sourceNodeAllocator);
    j.PushBack(Value(kArrayType), sourceNodeAllocator);
    j.PushBack(Value(kArrayType), sourceNodeAllocator);
    j.PushBack(Value(kStringType), sourceNodeAllocator);
}

InclNodePointerTargetMap SMFile::findHeaderUnitTarget(Node *headerUnitNode) const
{

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const InclNode *nodeDir = nullptr;

    // Iterating over all header-unit-directories of the target to find out which header-unit directory this header-unit
    // comes from and which target that header-unit-directory belongs to if any
    for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
    {
        if (pathContainsFile(inclNode.node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget && huDirTarget != targetLocal)
            {
                printErrorMessageColor(FORMAT("Module Header Unit\n{}\n belongs to two different Target Header "
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
        if (const auto it = target->configuration->moduleFilesToTarget.find(headerUnitNode);
            it != target->configuration->moduleFilesToTarget.end())
        {
            huDirTarget = it->second.target;

            // The mapped target must be the same as the smfile target from which this header-unit is discovered or one
            // of its requirementDeps

            bool isHuDirTargetRelated = false;

            if (huDirTarget == target)
            {
                isHuDirTargetRelated = true;
            }

            if (!isHuDirTargetRelated)
            {
                if (const auto it2 = target->requirementDeps.find(it->second.target);
                    it2 != target->requirementDeps.end())
                {
                    isHuDirTargetRelated = true;
                }
            }

            if (isHuDirTargetRelated)
            {
                bool found = false;

                for (InclNode &incl : target->reqIncls)
                {
                    if (incl.node == it->second.incl)
                    {
                        found = true;
                        nodeDir = &incl;
                        break;
                    }
                }

                if (found)
                {
                    return {nodeDir, huDirTarget};
                }
            }
        }
        printErrorMessageColor(FORMAT("Could not find the target for Header Unit\n{}\ndiscovered in file\n{}\nin "
                                      "Target\n{}.\nSearched for header-unit target in the following reqHuDirs.\n",
                                      headerUnitNode->filePath, node->filePath, target->name),
                               settings.pcSettings.toolErrorOutput);
        for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
        {
            printErrorMessage(FORMAT("HuDirTarget {} inclNode {}\n", targetLocal->name, inclNode.node->filePath));
        }

        throw std::exception();
    }

    return {nodeDir, huDirTarget};
}

void SMFile::initializeHeaderUnits(Builder &builder)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    for (Value &requireValue : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {

        namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

        if (requireValue[SingleHeaderUnitDep::logicalName] == Value(svtogsr(logicalName)))
        {
            printErrorMessageColor(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                          target->getTarjanNodeName(), node->filePath),
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }

        Node *headerUnitNode = Node::getNodeFromValue(requireValue[SingleHeaderUnitDep::fullPath], true);
        auto [nodeDir, huDirTarget] = findHeaderUnitTarget(headerUnitNode);

        huDirTarget->headerUnitsMutex.lock();
        SMFile *headerUnit = nullptr;
        bool doLoad = false;
        if (const auto it = huDirTarget->headerUnitsSet.find(headerUnitNode); it == huDirTarget->headerUnitsSet.end())
        {
            headerUnit = new SMFile(huDirTarget, headerUnitNode);
            huDirTarget->headerUnitsSet.emplace(headerUnit).first;

            huDirTarget->headerUnitsMutex.unlock();

            if (nodeDir->ignoreHeaderDeps)
            {
                headerUnit->ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
            }

            atomic_ref(headerUnit->indexInBuildCache)
                .store(huDirTarget->newHeaderUnitsSize.fetch_add(1) + huDirTarget->oldHeaderUnits.size());
            initializeModuleJson(headerUnit->sourceJson, headerUnit->node, headerUnit->sourceNodeAllocator,
                                 *headerUnit->target);

            headerUnit->type = SM_FILE_TYPE::HEADER_UNIT;
            headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
        }
        else
        {
            headerUnit = *it;
            huDirTarget->headerUnitsMutex.unlock();
            // We don't know whether the other thread has set the headerUnitIndex yet. But the while loop is not run
            // here. So, in-case the other thread has just acquired headerUnitsMutex, it could set the headerUnitIndex
            // so we don't loop much
            doLoad = true;

            if (headerUnit->foundFromCache && !atomic_ref(headerUnit->addedForRoundOne).exchange(true))
            {
                headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
                headerUnit->realBTargets[0].addInTarjanNodeBTarget(0);
            }
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(
            headerUnit, HeaderUnitConsumer{requireValue[SingleHeaderUnitDep::angle].GetBool(),
                                           requireValue[SingleHeaderUnitDep::logicalName].GetString()});
        addDependency<0>(*headerUnit);

        if (doLoad)
        {
            while (atomic_ref(headerUnit->indexInBuildCache).load() == UINT64_MAX)
                ;
        }
        else
        {
            // We ourselves initialized and set headerUnitIndex
        }

        requireValue[SingleHeaderUnitDep::targetIndex] = huDirTarget->targetCacheIndex;
        requireValue[SingleHeaderUnitDep::myIndex] = headerUnit->indexInBuildCache;
    }
}

void SMFile::addNewBTargetInFinalBTargetsRound1(Builder &builder)
{
    {
        std::lock_guard lk(builder.executeMutex);
        builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, this);
        target->addDependency<1>(*this);
        builder.updateBTargetsSizeGoal += 1;
    }
    builder.cond.notify_one();
}

void SMFile::setSMFileType(Builder &builder)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    if (sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::isInterface].GetBool())
    {
        if (sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::exportName].GetStringLength())
        {
            type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        }
    }
    else
    {
        if (sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::exportName].GetStringLength())
        {
            type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
        }
        else
        {
            type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
        }
    }
}

void SMFile::isObjectFileOutdatedComparedToSourceFileAndItsDeps()
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    if (atomic_ref(isObjectFileOutdatedCallCompleted).load())
    {
        return;
    }

    if (checkHeaderFiles(objectFileOutputFilePath))
    {
        isObjectFileOutdated = true;
        atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
        return;
    }

    for (Value &pValue : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {

        CppSourceTarget *localTarget = cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[pValue[SingleHeaderUnitDep::myIndex].GetUint64()];

        if (!atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
        {
            headerUnit.isObjectFileOutdatedComparedToSourceFileAndItsDeps();
        }

        if (headerUnit.isObjectFileOutdated)
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }
    }
}

void SMFile::isSMRulesFileOutdatedComparedToSourceFileAndItsDeps()
{
    // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
    // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
    // .m.o file.

    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    if (atomic_ref(isSMRuleFileOutdatedCallCompleted).load())
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
        target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".smrules", true, true);

    if (smRuleFileNode->doesNotExist || node->lastWriteTime > smRuleFileNode->lastWriteTime ||
        checkHeaderFiles(smRuleFileNode))
    {
        isSMRuleFileOutdated = true;
        atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
        return;
    }

    for (Value &pValue : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[pValue[SingleHeaderUnitDep::myIndex].GetInt()];

        if (!atomic_ref(headerUnit.isSMRuleFileOutdatedCallCompleted).load())
        {
            headerUnit.isSMRulesFileOutdatedComparedToSourceFileAndItsDeps();
        }

        if (headerUnit.isSMRuleFileOutdated)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
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

// TODO
// Propose the idea of big smfile. This combined with markArchivePoint could result in much increased in performance.
void SMFile::setFileStatusAndPopulateAllDependencies()
{
    auto emplaceInAll = [&](SMFile *smFile) -> bool {
        if (const auto &[pos, Ok] = allSMFileDependenciesRoundZero.emplace(smFile); Ok)
        {
            for (auto &h : smFile->headerUnitsConsumptionData)
            {
                headerUnitsConsumptionData.emplace(h);
            }
            return true;
        }
        return false;
    };

    if (!atomic_ref(fileStatus).load())
    {
        for (auto &[dependency, ignore] : realBTargets[0].dependencies)
        {
            if (dependency->getBTargetType() == BTargetType::SMFILE)
            {
                if (auto *smFile = static_cast<SMFile *>(dependency); emplaceInAll(smFile))
                {
                    for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
                    {
                        emplaceInAll(smFileDep);
                    }
                    if (smFile->objectFileOutputFilePath->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
                    {
                        atomic_ref(fileStatus).store(true);
                    }
                }
            }
        }
    }
    else
    {
        for (auto &[dependency, ignore] : realBTargets[0].dependencies)
        {
            if (dependency->getBTargetType() == BTargetType::SMFILE)
            {
                if (auto *smFile = static_cast<SMFile *>(dependency); emplaceInAll(smFile))
                {
                    for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
                    {
                        emplaceInAll(smFileDep);
                    }
                }
            }
        }
    }
}

string SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    string str;
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

string SMFile::getFlagPrint(const string &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    const bool infra = ccpSettings.infrastructureFlags;

    string str;

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

string SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    const string ifcFilePath =
        addQuotes(target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".ifc");

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
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");

        const HeaderUnitConsumer &headerUnitConsumer = dependentSMFile.headerUnitsConsumptionData.find(this)->second;
        string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
        str += "/headerUnit:";
        str += angleStr + " ";
        str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
    }
    else
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
        throw std::exception();
    }
    return str;
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    const string ifcFilePath = target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName() + ".ifc";
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
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
            const HeaderUnitConsumer &headerUnitConsumer =
                dependentSMFile.headerUnitsConsumptionData.find(this)->second;

            if (ccpSettings.infrastructureFlags)
            {
                string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
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
    moduleCompileCommandPrintLastHalf +=
        getFlagPrint(target->buildCacheFilesDirPathNode->filePath + slashc + node->getFileName());
    return moduleCompileCommandPrintLastHalf;
}