
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

void SourceNode::initializeSourceJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                      const CppSourceTarget &target)
{
    // Indices::BuildCache::CppBuild::SourceFiles::fullPath
    j.PushBack(node->getValue(), sourceNodeAllocator);

    // Indices::BuildCache::CppBuild::SourceFiles::compileCommandWithTool
    j.PushBack(Node::getType(), sourceNodeAllocator);

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
            const_cast<Node *>(node)->ensureSystemCheckCalled(true);

            objectFileOutputFileNode = Node::getNodeFromNormalizedString(
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

    StaticVector<Node *, 1000> headerFilesUnChecked;

    for (const Value &value : sourceJson[SourceFiles::headerFiles].GetArray())
    {
        bool systemCheckCompleted = false;
        Node *headerNode = Node::tryGetNodeFromValue(systemCheckCompleted, value, true, true);
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
    // has more dir components afterward. They won't be checked.
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

    if (objectFileOutputFileNode->doesNotExist)
    {
        atomic_ref(fileStatus).store(true);
        return;
    }

    if (node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
    {
        atomic_ref(fileStatus).store(true);
        return;
    }

    if (checkHeaderFiles(objectFileOutputFileNode))
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

ValueObjectFileMapping::ValueObjectFileMapping(Value *requireJson_, Node *objectFileOutputFilePath_)
    : requireJson(requireJson_), objectFileOutputFilePath(objectFileOutputFilePath_)
{
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
        namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

        if (sourceJson[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        if (objectFileOutputFileNode->doesNotExist)
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        if (node->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
        {
            atomic_ref(fileStatus).store(true);
            return;
        }

        // While header-units are checked in shouldGenerateSMFileInRoundOne, they are not checked here.
        // Because, if a header-unit of ours is updated, because we are dependent of it, we will be updated as-well.
        if (checkHeaderFiles(objectFileOutputFileNode))
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
            printErrorMessage(
                FORMAT("In target:\n{}\nModule name:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                       target->getTarjanNodeName(), logicalName, node->filePath, val->node->filePath));
        }
    }
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    RealBTarget &realBTarget = realBTargets[round];
    if (round == 2 && type == SM_FILE_TYPE::HEADER_UNIT)
    {
        const_cast<Node *>(node)->ensureSystemCheckCalled(true, true);

        sourceJson.CopyFrom(target->getBuildCache()[Indices::BuildCache::CppBuild::headerUnits][indexInBuildCache],
                            sourceNodeAllocator);

        // This holds the pointer to bmi file instead of object file in-case of a header-unit.
        objectFileOutputFileNode = Node::getNodeFromNormalizedString(
            target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".ifc", true, true);

        if (sourceJson[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
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

            if (sourceJson[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
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
                sourceJson[ModuleFiles::scanningCommandWithTool] = target->compileCommandWithTool.getHash();
                postCompile.parseHeaderDeps(*this);
                (type == SM_FILE_TYPE::HEADER_UNIT ? target->headerUnitScanned : target->moduleFileScanned) = true;
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
            if (isSMRuleFileOutdated)
            {
                StaticVector<string_view, 1000> includeNames;
                saveSMRulesJsonToSourceJson(smrulesFileOutputClang, includeNames);
                initializeHeaderUnits(builder, includeNames);
            }
            else
            {
                initializeNewHeaderUnitsSMRulesNotOutdated(builder);
            }
            if (type != SM_FILE_TYPE::HEADER_UNIT)
            {
                setSMFileType();
                setLogicalNameAndAddToRequirePath();

                for (Value &requireValue :
                     sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::moduleArray].GetArray())
                {
                    namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;
                    if (requireValue[SingleModuleDep::logicalName] == Value(svtogsr(logicalName)))
                    {
                        printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                 target->getTarjanNodeName(), node->filePath));
                    }
                }
            }

            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            target->addDependencyDelayed<0>(*this);
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

                for (const ValueObjectFileMapping &mapping : valueObjectFileMapping)
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

void SMFile::saveSMRulesJsonToSourceJson(const string &smrulesFileOutputClang,
                                         StaticVector<string_view, 1000> &includeNames)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SMRules = ModuleFiles::SmRules;

    // We get half-node since we trust the compiler to have generated if it is returning true
    const Node *smRuleFileNode = Node::getHalfNodeFromNormalizedString(target->buildCacheFilesDirPathNode->filePath +
                                                                       slashc + getOutputFileName() + ".smrules");

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

    Value &prunedRules = sourceJson[ModuleFiles::smRules];
    prunedRules.Clear();

    if (const auto it = rule.FindMember(svtogsr(JConsts::provides)); it == rule.MemberEnd())
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            prunedRules.PushBack(svtogsr(logicalName), sourceNodeAllocator);
        }
        else
        {
            prunedRules.PushBack(Value(kStringType), sourceNodeAllocator);
        }
        prunedRules.PushBack(Value(0), sourceNodeAllocator);
    }
    else
    {
        Value &provideJson = it->value[0];
        Value &logicalNameValue = provideJson.FindMember(Value(svtogsr(JConsts::logicalName)))->value;
        const bool isInterface = provideJson.FindMember(Value(svtogsr(JConsts::isInterface)))->value.GetBool();
        prunedRules.PushBack(logicalNameValue, sourceNodeAllocator);
        prunedRules.PushBack(static_cast<uint64_t>(isInterface), sourceNodeAllocator);
    }

    // Pushing header-unit array and module-array
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
                Value &moduleDevalue = *(prunedRules[SMRules::moduleArray].End() - 1);

                // If source-path does not exist, then it is not a header-unit
                // This source-path will be saved before saving and then will be checked in next invocations in
                // resolveRequirePaths function.
                moduleDevalue.PushBack(Node::getType(), sourceNodeAllocator);
                moduleDevalue.PushBack(logicalName, sourceNodeAllocator);
            }
            else
            {
                includeNames.emplace_back({logicalName.GetString(), logicalName.GetStringLength()});
                prunedRules[SMRules::headerUnitArray].PushBack(Value(kArrayType), sourceNodeAllocator);
                Value &headerUnitDevalue = *(prunedRules[SMRules::headerUnitArray].End() - 1);

                // lower-cased before saving for further use
                string_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                lowerCasePStringOnWindows(const_cast<char *>(str.data()), str.size());
                const Node *halfHeaderUnitNode = Node::getHalfNodeFromNormalizedString(str);

                // fullPath
                headerUnitDevalue.PushBack(halfHeaderUnitNode->getValue(), sourceNodeAllocator);

                const bool angle = requireValue.FindMember(Value(svtogsr(JConsts::lookupMethod)))->value ==
                                   Value(svtogsr(JConsts::includeAngle));
                // angle
                headerUnitDevalue.PushBack(static_cast<uint64_t>(angle), sourceNodeAllocator);

                // These values are initialized later in initializeHeaderUnits.
                // targetIndex
                headerUnitDevalue.PushBack(Value(UINT64_MAX), sourceNodeAllocator);
                // myIndex
                headerUnitDevalue.PushBack(Value(UINT64_MAX), sourceNodeAllocator);
            }
        }
    }
}

void SMFile::initializeModuleJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                  const CppSourceTarget &target)
{
    j.PushBack(node->getValue(), sourceNodeAllocator);
    j.PushBack(Node::getType(), sourceNodeAllocator);
    j.PushBack(Value(kArrayType), sourceNodeAllocator);
    j.PushBack(Value(kArrayType), sourceNodeAllocator);
    j.PushBack(Node::getType(), sourceNodeAllocator);
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
                if (huDirTarget != targetLocal)
                {
                    printErrorMessage(FORMAT("Module Header Unit\n{}\n belongs to two different Targets\n{}\n{}\n",
                                             headerUnitNode->filePath, nodeDir->node->filePath, inclNode.node->filePath,
                                             settings.pcSettings.toolErrorOutput));
                }
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
            for (const InclNode &incl : target->reqIncls)
            {
                if (pathContainsFile(incl.node->filePath, headerUnitNode->filePath))
                {
                    return {nullptr, it->second};
                }
            }
            printErrorMessage("HMake Internal Error");
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
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    for (Value &value : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[value[SingleHeaderUnitDep::targetIndex].GetUint64()];
        SMFile &headerUnit = localTarget->oldHeaderUnits[value[SingleHeaderUnitDep::myIndex].GetUint64()];

        if (!atomic_ref(headerUnit.addedForRoundOne).exchange(true))
        {
            headerUnit.addNewBTargetInFinalBTargetsRound1(builder);
            headerUnit.realBTargets[0].addInTarjanNodeBTarget(0);
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(&headerUnit, value[SingleHeaderUnitDep::angle].GetUint64());
        addDependencyDelayed<0>(headerUnit);
    }
}

void SMFile::initializeHeaderUnits(Builder &builder, const StaticVector<string_view, 1000> &includeNames)
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    for (uint64_t i = 0; i < sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].Size(); ++i)
    {
        Value &requireValue = sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray][i];
        namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

        Node *headerUnitNode = Node::getNodeFromValue(requireValue[SingleHeaderUnitDep::fullPath], true);
        auto [nodeDir, huDirTarget] = findHeaderUnitTarget(headerUnitNode);

        SMFile *headerUnit = nullptr;
        bool doLoad = false;
        bool alreadyAddedInHeaderUnitSet = false;

        if (nodeDir && nodeDir->headerUnitIndex != UINT32_MAX)
        {
            headerUnit = &cppSourceTargets[nodeDir->targetCacheIndex]->oldHeaderUnits[nodeDir->headerUnitIndex];
            alreadyAddedInHeaderUnitSet = true;
        }
        else
        {
            huDirTarget->headerUnitsMutex.lock();
            if (const auto it = huDirTarget->headerUnitsSet.find(headerUnitNode);
                it == huDirTarget->headerUnitsSet.end())
            {
                headerUnit = new SMFile(huDirTarget, headerUnitNode);
                headerUnit->addedForRoundOne = true;
                huDirTarget->headerUnitsSet.emplace(headerUnit).first;

                huDirTarget->headerUnitsMutex.unlock();

                /*if (nodeDir->ignoreHeaderDeps)
                {
                    headerUnit->ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
                }*/

                atomic_ref(headerUnit->indexInBuildCache)
                    .store(huDirTarget->newHeaderUnitsSize.fetch_add(1) + huDirTarget->oldHeaderUnits.size());
                initializeModuleJson(headerUnit->sourceJson, headerUnit->node, headerUnit->sourceNodeAllocator,
                                     *headerUnit->target);

                headerUnit->type = SM_FILE_TYPE::HEADER_UNIT;
                headerUnit->logicalName = string(includeNames[i]);
                headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
            }
            else
            {
                headerUnit = *it;
                huDirTarget->headerUnitsMutex.unlock();
                alreadyAddedInHeaderUnitSet = true;
            }
        }

        if (alreadyAddedInHeaderUnitSet)
        {
            if (headerUnit->isAnOlderHeaderUnit)
            {
                if (!atomic_ref(headerUnit->addedForRoundOne).exchange(true))
                {
                    headerUnit->addNewBTargetInFinalBTargetsRound1(builder);
                    headerUnit->realBTargets[0].addInTarjanNodeBTarget(0);
                }
            }
            // Older header-units indexInBuildCache already set.
            else
            {
                // We don't know whether the other thread has set the headerUnitIndex yet. But the while loop is not run
                // here. So, in-case the other thread has just acquired headerUnitsMutex, it could set the
                // headerUnitIndex so we don't loop much
                doLoad = true;
            }
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(headerUnit, requireValue[SingleHeaderUnitDep::angle].GetUint64());
        addDependencyDelayed<0>(*headerUnit);

        if (doLoad)
        {
            while (atomic_ref(headerUnit->indexInBuildCache).load() == UINT64_MAX)
                ;
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
        // This locks double mutex. Reasoning for performing it in single lock is difficult.
        target->addDependency<1>(*this);
        builder.updateBTargetsSizeGoal += 1;
    }
    builder.cond.notify_one();
}

void SMFile::setSMFileType()
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

    if (sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::isInterface].GetUint64())
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

void SMFile::checkObjectFileOutdatedHeaderUnits()
{
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    if (atomic_ref(isObjectFileOutdatedCallCompleted).load())
    {
        return;
    }

    if (checkHeaderFiles(objectFileOutputFileNode))
    {
        isObjectFileOutdated = true;
        atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
        return;
    }

    for (Value &value : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[value[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            // TODO
            // Maybe we can set isObjectFileSMRuleFileOutdated here as well.
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[value[SingleHeaderUnitDep::myIndex].GetUint64()];

        if (!atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
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
        target->buildCacheFilesDirPathNode->filePath + slashc + getOutputFileName() + ".smrules", true, true);

    if (smRuleFileNode->doesNotExist || node->lastWriteTime > smRuleFileNode->lastWriteTime)

    {
        isSMRuleFileOutdated = true;
        atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
        return;
    }

    // We assume that all header-files systemCheck has already been called and that they exist.
    for (const Value &value : sourceJson[Indices::BuildCache::CppBuild::SourceFiles::headerFiles].GetArray())
    {
        if (const Node *headerNode = Node::getHalfNodeFromValue(value);
            headerNode->lastWriteTime > smRuleFileNode->lastWriteTime)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }
    }

    for (Value &value : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[value[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[value[SingleHeaderUnitDep::myIndex].GetInt()];

        if (!atomic_ref(headerUnit.isSMRuleFileOutdatedCallCompleted).load())
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
    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    if (checkHeaderFiles(objectFileOutputFileNode))
    {
        isObjectFileOutdated = true;
        return;
    }

    for (Value &value : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[value[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            isObjectFileOutdated = true;
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[value[SingleHeaderUnitDep::myIndex].GetUint64()];

        if (!atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
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

    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

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
    for (const Value &value : sourceJson[Indices::BuildCache::CppBuild::SourceFiles::headerFiles].GetArray())
    {
        if (const Node *headerNode = Node::getHalfNodeFromValue(value);
            headerNode->lastWriteTime > smRuleFileNode->lastWriteTime)
        {
            isSMRuleFileOutdated = true;
            return;
        }
    }

    for (Value &value : sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        CppSourceTarget *localTarget = cppSourceTargets[value[SingleHeaderUnitDep::targetIndex].GetUint64()];
        if (!localTarget)
        {
            isSMRuleFileOutdated = true;
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[value[SingleHeaderUnitDep::myIndex].GetInt()];

        if (!atomic_ref(headerUnit.isSMRuleFileOutdatedCallCompleted).load())
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
                    if (smFile->objectFileOutputFileNode->lastWriteTime > objectFileOutputFileNode->lastWriteTime)
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