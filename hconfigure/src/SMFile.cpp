
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Configuration.hpp";
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
#include "Configuration.hpp"
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

HeaderUnitIndexInfo::HeaderUnitIndexInfo(const uint64_t targetIndex_, const uint64_t myIndex_)
    : targetIndex(targetIndex_), myIndex(myIndex_)
{
}

PValue &HeaderUnitIndexInfo::getSingleHeaderUnitDep() const
{
    namespace CppTarget = Indices::CppTarget;
    return tempCache[targetIndex][CppTarget::buildCache][CppTarget::BuildCache::headerUnits][myIndex];
}

SourceNode::SourceNode(CppSourceTarget *target_, Node *node_) : target(target_), node{node_}
{
}

SourceNode::SourceNode(CppSourceTarget *target_, const Node *node_, const bool add0, const bool add1, const bool add2)
    : ObjectFile(add0, add1, add2), target(target_), node{node_}
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

void SourceNode::initializeSourceJson()
{
    // Indices::CppTarget::BuildCache::SourceFiles::fullPath
    sourceJson->PushBack(node->getPValue(), sourceNodeAllocator);

    // Indices::CppTarget::BuildCache::SourceFiles::compileCommandWithTool
    sourceJson->PushBack(PValue(kStringType), sourceNodeAllocator);

    // Indices::CppTarget::BuildCache::SourceFiles::headerFiles
    sourceJson->PushBack(PValue(kArrayType), sourceNodeAllocator);

    if (target->configuration && target->configuration->evaluate(GenerateModuleData::YES))
    {
        // Indices::CppTarget::BuildCache::SourceFiles::moduleData
        sourceJson->PushBack(PValue(kArrayType), sourceNodeAllocator);

        PValue &moduleData = (*sourceJson)[Indices::CppTarget::BuildCache::SourceFiles::moduleData];

        // Indices::CppTarget::BuildCache::ModuleFiles::ModuleData::exportName
        moduleData.PushBack(kStringType, sourceNodeAllocator);

        // Indices::CppTarget::BuildCache::ModuleFiles::ModuleData::isInterface
        moduleData.PushBack(PValue(false), sourceNodeAllocator);

        // Indices::CppTarget::BuildCache::ModuleFiles::ModuleData::moduleArray
        moduleData.PushBack(kArrayType, sourceNodeAllocator);

        // Indices::CppTarget::BuildCache::ModuleFiles::ModuleData::headerUnitArray
        moduleData.PushBack(kArrayType, sourceNodeAllocator);
    }
}

void SourceNode::updateBTarget(Builder &builder, const unsigned short round)
{
    if (!round)
    {
        RealBTarget &realBTarget = realBTargets[0];
        if (selectiveBuild || (target->configuration && target->configuration->evaluate(GenerateModuleData::YES)))
        {
            objectFileOutputFilePath = Node::getNodeFromNormalizedString(
                target->buildCacheFilesDirPath + node->getFileName() + ".o", true, true);

            setSourceNodeFileStatus();
            if (atomic_ref(fileStatus).load())
            {
                assignFileStatusToDependents(realBTarget);
                PostCompile postCompile = target->updateSourceNodeBTarget(*this);
                realBTarget.exitStatus = postCompile.exitStatus;
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
                // because cached compile-command would be different
                if (realBTarget.exitStatus == EXIT_SUCCESS)
                {
                    (*sourceJson)[Indices::CppTarget::BuildCache::SourceFiles::compileCommandWithTool] =
                        target->compileCommandWithTool.getHash();
                    if (target->configuration && target->configuration->evaluate(GenerateModuleData::YES))
                    {
                        postCompile.parseHeaderDeps(*this, false, true);
                    }
                    else
                    {
                        postCompile.parseHeaderDeps(*this, false, false);
                    }
                }
                lock_guard lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                fflush(stdout);
            }
        }

        if (target->configuration && target->configuration->evaluate(GenerateModuleData::YES) &&
            realBTarget.exitStatus == EXIT_SUCCESS)
        {
            if (!(*sourceJson)[Indices::CppTarget::BuildCache::SourceFiles::headerFiles].Empty())
            {
                populateModuleData(builder);
            }
        }
    }

    /*// All module data is populated. Here, we can move from
    for (auto &[depTarget, type] : realBTarget.dependencies)
    {
        if (depTarget->getBTargetType() == BTargetType::SMFILE)
        {
            if (const SMFile *smFile = static_cast<SMFile *>(depTarget);
                smFile->type == SM_FILE_TYPE::HEADER_UNIT_DISABLED)
            {
                namespace SourceFiles = Indices::CppTarget::BuildCache::SourceFiles;

                PValue &requireArray = (*sourceJson)[SourceFiles::moduleData][SourceFiles::ModuleData::requireArray];

                for (const PValue &singleModuleDep :
                     (*smFile->sourceJson)[SourceFiles::moduleData][SourceFiles::ModuleData::requireArray].GetArray())
                {
                    PValue v(singleModuleDep, sourceNodeAllocator);
                    requireArray.PushBack(v, sourceNodeAllocator);
                }
            }
        }
    }*/
}

static bool fileInIncludes(vector<pstring_view> &files, pstring_view file, bool &angle)
{
    constexpr uint64_t s = pstring("#include ").size();
}

void SourceNode::populateModuleData(Builder &builder)
{
    const vector<pstring> source = split(fileToPString(node->filePath), "\n");
    vector<pstring_view> includes;
    for (const pstring &str : source)
    {
        if (str.starts_with("#include "))
        {
            bool includeAlreadyInSource = false;
            for (const pstring_view str2 : includes)
            {
                if (str == str2)
                {
                    includeAlreadyInSource = true;
                    break;
                }
            }

            if (includeAlreadyInSource)
            {
                continue;
            }

            constexpr uint64_t s = pstring("#include ").size();

            bool quote = false;
            if (str[s] == '\"')
            {
                quote = true;
            }
            const pstring_view fileName(str.c_str() + s + 1, str.size() - (s + 2));
            for (PValue &pValue : (*sourceJson)[Indices::CppTarget::BuildCache::SourceFiles::headerFiles].GetArray())
            {
                Node *headerUnitNode = Node::getNotSystemCheckCalledNodeFromPValue(pValue);
                if (compareStringsFromEnd(fileName, getLastNameAfterSlashView(headerUnitNode->filePath)))
                {
                    // Do things with this pvalue. As this pvalue
                    auto [nodeDir, huDirTarget] = findHeaderUnitTarget(headerUnitNode);

                    huDirTarget->headerUnitsMutex.lock();
                    if (const auto it = huDirTarget->headerUnits.find(headerUnitNode);
                        it == huDirTarget->headerUnits.end())
                    {
                        SMFile *headerUnit = new SMFile(huDirTarget, headerUnitNode);
                        huDirTarget->headerUnits.emplace(headerUnit).first;
                        huDirTarget->headerUnitsMutex.unlock();

                        if (nodeDir->ignoreHeaderDeps)
                        {
                            headerUnit->ignoreHeaderDeps = SMFile::ignoreHeaderDepsForIgnoreHeaderUnits;
                        }

                        headerUnit->headerUnitsIndex = pvalueIndexInSubArray(
                            (*huDirTarget->targetBuildCache)[Indices::CppTarget::BuildCache::headerUnits],
                            headerUnit->node->getPValue());

                        if (headerUnit->headerUnitsIndex != UINT64_MAX)
                        {
                            headerUnit->sourceJson =
                                &(*huDirTarget->targetBuildCache)[Indices::CppTarget::BuildCache::headerUnits]
                                                                 [headerUnit->headerUnitsIndex];
                        }
                        else
                        {
                            // Indices::HeaderUnitDisabled
                            ++huDirTarget->newHeaderUnitsSize;
                            headerUnit->sourceJson = new PValue(kArrayType);
                            headerUnit->initializeSourceJson();
                        }

                        headerUnit->type = SM_FILE_TYPE::HEADER_UNIT_DISABLED;

                        headerUnit->addNewBTargetInFinalBTargets(builder);
                    }
                    else
                    {
                        huDirTarget->headerUnitsMutex.unlock();
                    }
                }
                namespace SourceFiles = Indices::CppTarget::BuildCache::SourceFiles;
                PValue &headerUnitArray =
                    (*sourceJson)[SourceFiles::moduleData][SourceFiles::ModuleData::headerUnitArray];

                const pstring logicalName(str.c_str() + s, str.size() - s);

                // SingleModuleDep::fullPath
                headerUnitArray.PushBack(headerUnitNode->getPValue(), sourceNodeAllocator);

                // SingleModuleDep::logicalName
                headerUnitArray.PushBack(
                    PValue(kStringType).SetString(logicalName.c_str(), logicalName.size(), sourceNodeAllocator),
                    sourceNodeAllocator);

                // SingleModuleDep::angle
                headerUnitArray.PushBack(PValue(!quote), sourceNodeAllocator);

                includes.emplace_back(str);
            }
        }
    }
}

bool SourceNode::checkHeaderFiles2(const Node *compareNode, bool alsoCheckHeaderUnit) const
{
    namespace SourceFiles = Indices::CppTarget::BuildCache::SourceFiles;
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;
    namespace SMRules = ModuleFiles::SmRules;
    namespace SingleHeaderUnitDep = SMRules::SingleHeaderUnitDep;

    vector<Node *> headerFilesUnChecked;
    // Use as stack
    vector<HeaderUnitIndexInfo> depthSearchStack;
    depthSearchStack.reserve(20);

    for (PValue &pValue : (*sourceJson)[ModuleFiles::smRules][SMRules::headerUnitArray].GetArray())
    {
        depthSearchStack.emplace_back(pValue[SingleHeaderUnitDep::targetIndex].GetUint64(),
                                      pValue[SingleHeaderUnitDep::myIndex].GetUint64());
    }

    while (!depthSearchStack.empty())
    {
        HeaderUnitIndexInfo info = depthSearchStack.back();
        depthSearchStack.pop_back();

        // Node *node = Node::nodeIndices[]
    }
    if (alsoCheckHeaderUnit)
    {
        for (PValue &pValue : (*sourceJson)[ModuleFiles::smRules][SMRules::headerUnitArray].GetArray())
        {
            bool systemCheckCompleted = false;
            Node *headerNode = Node::tryGetNodeFromPValue(systemCheckCompleted,
                                                          pValue[SMRules::SingleHeaderUnitDep::fullPath], true, true);
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
    }

    if (!headerFilesUnChecked.empty())
    {
        vector<Node *> stillUnchecked;
        stillUnchecked.reserve(headerFilesUnChecked.size());

        while (true)
        {
            bool zeroLeft = true;
            for (Node *headerNode : headerFilesUnChecked)
            {
                if (atomic_ref(headerNode->systemCheckCompleted).load())
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
                    stillUnchecked.emplace_back(headerNode);
                    zeroLeft = false;
                }
            }
            if (zeroLeft)
            {
                break;
            }
            headerFilesUnChecked.swap(stillUnchecked);
            stillUnchecked.clear();
        }
    }

    return false;
}

bool SourceNode::checkHeaderFiles(const Node *compareNode, bool alsoCheckHeaderUnit) const
{
    namespace SourceFiles = Indices::CppTarget::BuildCache::SourceFiles;
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;
    namespace SMRules = ModuleFiles::SmRules;

    vector<Node *> headerFilesUnChecked;

    if (alsoCheckHeaderUnit)
    {
        headerFilesUnChecked.reserve(
            std::max((*sourceJson)[SourceFiles::headerFiles].GetArray().Size(),
                     (*sourceJson)[ModuleFiles::smRules][ModuleFiles::ModuleData::headerUnitArray].GetArray().Size()));
    }
    else
    {
        headerFilesUnChecked.reserve((*sourceJson)[SourceFiles::headerFiles].GetArray().Size());
    }
    headerFilesUnChecked.reserve((*sourceJson)[SourceFiles::headerFiles].GetArray().Size());

    for (PValue &pValue : (*sourceJson)[SourceFiles::headerFiles].GetArray())
    {
        bool systemCheckCompleted = false;
        Node *headerNode = Node::tryGetNodeFromPValue(systemCheckCompleted, pValue, true, true);
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

    if (alsoCheckHeaderUnit)
    {
        for (PValue &pValue : (*sourceJson)[ModuleFiles::smRules][SMRules::headerUnitArray].GetArray())
        {
            bool systemCheckCompleted = false;
            Node *headerNode = Node::tryGetNodeFromPValue(systemCheckCompleted,
                                                          pValue[SMRules::SingleHeaderUnitDep::fullPath], true, true);
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
    }

    if (!headerFilesUnChecked.empty())
    {
        vector<Node *> stillUnchecked;
        stillUnchecked.reserve(headerFilesUnChecked.size());

        while (true)
        {
            bool zeroLeft = true;
            for (Node *headerNode : headerFilesUnChecked)
            {
                if (atomic_ref(headerNode->systemCheckCompleted).load())
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
                    stillUnchecked.emplace_back(headerNode);
                    zeroLeft = false;
                }
            }
            if (zeroLeft)
            {
                break;
            }
            headerFilesUnChecked.swap(stillUnchecked);
            stillUnchecked.clear();
        }
    }

    return false;
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

InclNodePointerTargetMap SourceNode::findHeaderUnitTarget(Node *headerUnitNode)
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
        printErrorMessageColor(fmt::format("Could not find the target for Header Unit\n{}\ndiscovered in file\n{}\nin "
                                           "Target\n{}.\nSearched for header-unit target in the following reqHuDirs.\n",
                                           headerUnitNode->filePath, node->filePath, target->name),
                               settings.pcSettings.toolErrorOutput);
        for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
        {
            printErrorMessage(fmt::format("HuDirTarget {} inclNode {}\n", targetLocal->name, inclNode.node->filePath));
        }
        throw std::exception();
    }
    return {nodeDir, huDirTarget};
}

void SourceNode::setSourceNodeFileStatus()
{
    namespace SourceFiles = Indices::CppTarget::BuildCache::SourceFiles;

    if ((*sourceJson)[SourceFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
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

    if (checkHeaderFiles(objectFileOutputFilePath, false))
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

SMFile::SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_) : SourceNode(target_, node_), type(type_)
{
    assert(type == SM_FILE_TYPE::HEADER_UNIT_DISABLED &&
           "This constructor should only be used in GenerateModeData::YES mode.\n");
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_, bool olderHeaderUnit)
    : SourceNode(target_, node_, false, false, false), type(type_)
{
}

void SMFile::setSMRulesJson(pstring_view smRulesJson)
{
    smRuleFileBuffer = std::make_unique<vector<pchar>>(smRulesJson.size() + 1);
    (*smRuleFileBuffer)[smRulesJson.size()] = '\0';

    for (uint64_t i = 0; i < smRulesJson.size(); ++i)
    {
        (*smRuleFileBuffer)[i] = smRulesJson[i];
    }

    (*sourceJson)[Indices::CppTarget::BuildCache::ModuleFiles::smRules] =
        PDocument(kArrayType).ParseInsitu(&(*smRuleFileBuffer)[0]).Move();

    isSMRulesJsonSet = true;
}

void SMFile::checkHeaderFilesIfSMRulesJsonSet()
{
    if (isSMRulesJsonSet)
    {
        // Check all header-files and set fileStatus to true.
        namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;

        if ((*sourceJson)[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
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
        if (checkHeaderFiles(objectFileOutputFilePath, false))
        {
            atomic_ref(fileStatus).store(true);
        }
    }
}

void SMFile::setLogicalNameAndAddToRequirePath()
{
    namespace ModuleFile = Indices::CppTarget::BuildCache::ModuleFiles;
    namespace SMRules = ModuleFile::SmRules;

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

void SMFile::updateBTarget(Builder &builder, const unsigned short round)
{
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;

    // Danger Following is executed concurrently
    RealBTarget &realBTarget = realBTargets[round];
    if (round == 2 && type == SM_FILE_TYPE::HEADER_UNIT)
    {
        const_cast<Node *>(node)->ensureSystemCheckCalled(true, true);

        sourceJson = &(*target->targetBuildCache)[Indices::CppTarget::BuildCache::headerUnits][headerUnitsIndex];

        objectFileOutputFilePath = Node::getNodeFromNormalizedString(
            target->buildCacheFilesDirPath + node->getFileName() + ".m.o", true, true);

        if ((*sourceJson)[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
            isSMRuleFileOutdated = true;
            isSMRuleFileOutdatedCallCompleted = true;
            return;
        }

        if (node->doesNotExist || objectFileOutputFilePath->doesNotExist ||
            node->lastWriteTime > objectFileOutputFilePath->lastWriteTime ||
            checkHeaderFiles(objectFileOutputFilePath, false))
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
                target->buildCacheFilesDirPath + node->getFileName() + ".m.o", true, true);

            if ((*sourceJson)[ModuleFiles::scanningCommandWithTool] != target->compileCommandWithTool.getHash())
            {
                isObjectFileOutdated = true;
                atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
                isSMRuleFileOutdated = true;
                atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            }

            if (node->doesNotExist || objectFileOutputFilePath->doesNotExist ||
                node->lastWriteTime > objectFileOutputFilePath->lastWriteTime ||
                checkHeaderFiles(objectFileOutputFilePath, false))
            {
                isObjectFileOutdated = true;
                atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            }
        }

        pstring smrulesFileOutputClang;
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
                (*sourceJson)[ModuleFiles::scanningCommandWithTool] = target->compileCommandWithTool.getHash();
                postCompile.parseHeaderDeps(*this, true, false);
            }
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
            if (isObjectFileOutdated)
            {
                saveSMRulesJsonToSourceJson(smrulesFileOutputClang);
            }
            setLogicalNameAndAddToRequirePath();

            for (PValue &requirePValue :
                 (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::moduleArray].GetArray())
            {
                namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;
                if (requirePValue[SingleModuleDep::logicalName] == PValue(ptoref(logicalName)))
                {
                    printErrorMessageColor(fmt::format("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                       target->getTarjanNodeName(), node->filePath),
                                           settings.pcSettings.toolErrorOutput);
                    throw std::exception();
                }
            }

            initializeHeaderUnits(builder);
            if (type != SM_FILE_TYPE::HEADER_UNIT)
            {
                setSMFileType(builder);
            }

            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            target->realBTargets[0].addDependency(*this);
        }
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        checkHeaderFilesIfSMRulesJsonSet();
        setFileStatusAndPopulateAllDependencies();

        if (!atomic_ref(fileStatus).load())
        {
            if ((*sourceJson)[ModuleFiles::compileCommandWithTool] != target->compileCommandWithTool.getHash())
            {
                atomic_ref(fileStatus).store(true);
            }
        }

        if (atomic_ref(fileStatus).load())
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
                    namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;
                    (*mapping.requireJson)[SingleModuleDep::fullPath] = mapping.objectFileOutputFilePath->getPValue();
                }
            }
        }
    }
    else if (!round)
    {
        if (target->configuration && target->configuration->evaluate(GenerateModuleData::YES))
        {
            SourceNode::updateBTarget(builder, 0);
        }
    }
}

void SMFile::saveSMRulesJsonToSourceJson(const pstring &smrulesFileOutputClang)
{
    namespace ModuleFile = Indices::CppTarget::BuildCache::ModuleFiles;
    namespace SMRules = ModuleFile::SmRules;

    const pstring smFilePath = target->buildCacheFilesDirPath + getLastNameAfterSlash(node->filePath) + ".smrules";
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
    prunedRules.PushBack(PValue(kArrayType), sourceNodeAllocator);

    for (auto it = rule.FindMember(ptoref(JConsts::requires_)); it != rule.MemberEnd(); ++it)
    {
        for (PValue &requirePValue : it->value.GetArray())
        {
            PValue &logicalName = requirePValue.FindMember(PValue(ptoref(JConsts::logicalName)))->value;

            if (auto sourcePathIt = requirePValue.FindMember(PValue(ptoref(JConsts::sourcePath)));
                sourcePathIt == requirePValue.MemberEnd())
            {
                prunedRules[SMRules::moduleArray].PushBack(PValue(kArrayType), sourceNodeAllocator);
                PValue &moduleDepPValue = *(prunedRules[SMRules::moduleArray].End() - 1);

                // If source-path does not exist, then it is not a header-unit
                // This source-path will be saved before saving and then will be checked in next invocations in
                // resolveRequirePaths function.
                moduleDepPValue.PushBack(Node::getType(), sourceNodeAllocator);
                moduleDepPValue.PushBack(logicalName, sourceNodeAllocator);
            }
            else
            {
                prunedRules[SMRules::headerUnitArray].PushBack(PValue(kArrayType), sourceNodeAllocator);
                PValue &headerUnitDepPValue = *(prunedRules[SMRules::headerUnitArray].End() - 1);

                // lower-cased before saving for further use
                pstring_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                lowerCasePStringOnWindows(const_cast<pchar *>(str.data()), str.size());
                const Node *halfHeaderUnitNode = Node::getHalfNodeFromNormalizedString(str);

                // fullPath
                headerUnitDepPValue.PushBack(halfHeaderUnitNode->getPValue(), sourceNodeAllocator);
                // logicalName
                headerUnitDepPValue.PushBack(logicalName, sourceNodeAllocator);
                // angle
                headerUnitDepPValue.PushBack(requirePValue.FindMember(PValue(ptoref(JConsts::lookupMethod)))->value ==
                                                 PValue(ptoref(JConsts::includeAngle)),
                                             sourceNodeAllocator);

                // These values are initialized later in initializeHeaderUnits.
                // targetIndex
                headerUnitDepPValue.PushBack(PValue(UINT64_MAX), ralloc);
                // myIndex
                headerUnitDepPValue.PushBack(PValue(UINT64_MAX), ralloc);
            }
        }
    }
}

void SMFile::initializeModuleJson()
{
    sourceJson->PushBack(node->getPValue(), sourceNodeAllocator);
    sourceJson->PushBack(PValue(kStringType), sourceNodeAllocator);
    sourceJson->PushBack(PValue(kArrayType), sourceNodeAllocator);
    sourceJson->PushBack(PValue(kArrayType), sourceNodeAllocator);
    sourceJson->PushBack(PValue(kStringType), sourceNodeAllocator);
}

void SMFile::initializeHeaderUnits(Builder &builder)
{
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;

    for (PValue &requirePValue : (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {

        namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

        if (requirePValue[SingleHeaderUnitDep::logicalName] == PValue(ptoref(logicalName)))
        {
            printErrorMessageColor(fmt::format("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                               target->getTarjanNodeName(), node->filePath),
                                   settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }

        Node *headerUnitNode = Node::getNodeFromPValue(requirePValue[SingleHeaderUnitDep::fullPath], true);
        auto [nodeDir, huDirTarget] = findHeaderUnitTarget(headerUnitNode);

        huDirTarget->headerUnitsMutex.lock();
        SMFile *headerUnit = nullptr;
        bool doLoad = false;
        if (const auto it = huDirTarget->headerUnits.find(headerUnitNode); it == huDirTarget->headerUnits.end())
        {
            headerUnit = new SMFile(huDirTarget, headerUnitNode);
            huDirTarget->headerUnits.emplace(headerUnit).first;

            huDirTarget->headerUnitsMutex.unlock();

            if (nodeDir->ignoreHeaderDeps)
            {
                headerUnit->ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
            }

            atomic_ref(headerUnit->headerUnitsIndex)
                .store(huDirTarget->newHeaderUnitsSize.fetch_add(1) + huDirTarget->oldHeaderUnits.size());
            headerUnit->sourceJson = new PValue(kArrayType);
            headerUnit->initializeModuleJson();

            headerUnit->type = SM_FILE_TYPE::HEADER_UNIT;
            headerUnit->addNewBTargetInFinalBTargets(builder);
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
                headerUnit->addNewBTargetInFinalBTargets(builder);
                headerUnit->realBTargets[0].addInTarjanNodeBTarget(0);
            }
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(
            headerUnit, HeaderUnitConsumer{requirePValue[SingleHeaderUnitDep::angle].GetBool(),
                                           requirePValue[SingleHeaderUnitDep::logicalName].GetString()});
        realBTargets[0].addDependency(*headerUnit);

        if (doLoad)
        {
            while (atomic_ref(headerUnit->headerUnitsIndex).load() == UINT64_MAX)
                ;
        }
        else
        {
            // We ourselves initialized and set headerUnitIndex
        }

        requirePValue[SingleHeaderUnitDep::targetIndex] = huDirTarget->tempCacheIndex;
        requirePValue[SingleHeaderUnitDep::myIndex] = headerUnit->headerUnitsIndex;
    }
}

void SMFile::addNewBTargetInFinalBTargets(Builder &builder)
{
    {
        std::lock_guard lk(builder.executeMutex);
        builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, this);
        target->adjustHeaderUnitsBTarget.realBTargets[builder.round].addDependency(*this);
        builder.updateBTargetsSizeGoal += 1;
    }
    builder.cond.notify_one();
}

void SMFile::setSMFileType(Builder &builder)
{
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;

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

void SMFile::isObjectFileOutdatedComparedToSourceFileAndItsDeps()
{
    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;
    namespace SingleHeaderUnitDep = ModuleFiles::SmRules::SingleHeaderUnitDep;

    if (atomic_ref(isObjectFileOutdatedCallCompleted).load())
    {
        return;
    }

    if (checkHeaderFiles(objectFileOutputFilePath, false))
    {
        isObjectFileOutdated = true;
        atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
        return;
    }

    for (PValue &pValue : (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        if (!cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetUint64()])
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetUint64()]
                                 ->oldHeaderUnits[pValue[SingleHeaderUnitDep::myIndex].GetUint64()];

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

    namespace ModuleFiles = Indices::CppTarget::BuildCache::ModuleFiles;
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
        target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".smrules", true, true);

    if (smRuleFileNode->doesNotExist || node->lastWriteTime > smRuleFileNode->lastWriteTime ||
        checkHeaderFiles(smRuleFileNode, false))
    {
        isSMRuleFileOutdated = true;
        atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
        return;
    }

    for (PValue &pValue : (*sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::headerUnitArray].GetArray())
    {
        if (!cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetUint64()])
        {
            isSMRuleFileOutdated = true;
            atomic_ref(isSMRuleFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = cppSourceTargets[pValue[SingleHeaderUnitDep::targetIndex].GetInt()]
                                 ->oldHeaderUnits[pValue[SingleHeaderUnitDep::myIndex].GetInt()];

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
            for (auto &h : smFile->headerUnitsConsumptionData)
            {
                headerUnitsConsumptionData.emplace(h);
            }

            if (!atomic_ref(fileStatus).load())
            {
                if (smFile->objectFileOutputFilePath->lastWriteTime > objectFileOutputFilePath->lastWriteTime)
                {
                    atomic_ref(fileStatus).store(true);
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
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");

        const HeaderUnitConsumer &headerUnitConsumer = dependentSMFile.headerUnitsConsumptionData.find(this)->second;
        pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
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
                pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
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