
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

using std::filesystem::directory_entry, std::filesystem::file_type, std::tie, std::ifstream,
    std::filesystem::file_time_type, std::exception, std::lock_guard;

string getStatusString(const path &p)
{
    switch (status(p).type())
    {
    case file_type::none:
        return " has `not-evaluated-yet` type";
    case file_type::not_found:
        return " does not exist";
    case file_type::regular:
        return " is a regular file";
    case file_type::directory:
        return " is a directory";
    case file_type::symlink:
        return " is a symlink";
    case file_type::block:
        return " is a block device";
    case file_type::character:
        return " is a character device";
    case file_type::fifo:
        return " is a named IPC pipe";
    case file_type::socket:
        return " is a named IPC socket";
    case file_type::unknown:
        return " has `unknown` type";
    default:
        return " has `implementation-defined` type";
    }
}

bool CompareNode::operator()(const Node &lhs, const Node &rhs) const
{
    return lhs.filePath < rhs.filePath;
}

bool CompareNode::operator()(const string &lhs, const Node &rhs) const
{
    return lhs < rhs.filePath;
}

bool CompareNode::operator()(const Node &lhs, const string &rhs) const
{
    return lhs.filePath < rhs;
}

Node::Node(const path &filePath_, bool isFile, bool mayNotExist)
{
    std::filesystem::file_type nodeType = directory_entry(filePath_).status().type();
    if (nodeType == (isFile ? file_type::regular : file_type::directory))
    {
        filePath = filePath_.generic_string();
    }
    else
    {
        if (!mayNotExist || nodeType != file_type::not_found)
        {
            printErrorMessage(fmt::format("{} is not a {} file. File Type is {}\n", filePath_.generic_string(),
                                          isFile ? "regular" : "directory", getStatusString(filePath_)));
            throw std::exception();
        }
        doesNotExist = true;
    }
}

std::filesystem::file_time_type Node::getLastUpdateTime() const
{
    if (!isUpdated)
    {
        const_cast<std::filesystem::file_time_type &>(lastUpdateTime) = last_write_time(path(filePath));
        const_cast<bool &>(isUpdated) = true;
    }
    return lastUpdateTime;
}

path Node::getFinalNodePathFromString(const string &str)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal().generic_string();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        string lowerCase = filePath.string();
        for (char &c : lowerCase)
        {
            // Warning: assuming paths to be ASCII
            c = tolower(c);
        }
        filePath = lowerCase;
    }
    return filePath;
}

static mutex nodeInsertMutex;
Node *Node::getNodeFromString(const string &str, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromString(str);

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.
    lock_guard<mutex> lk(nodeInsertMutex);
    if (auto it = allFiles.find(filePath.string()); it != allFiles.end())
    {
        return const_cast<Node *>(it.operator->());
    }
    else
    {
        Node node(filePath, isFile, mayNotExist);
        return const_cast<Node *>(allFiles.emplace(std::move(node)).first.operator->());
    }
}

bool operator<(const Node &lhs, const Node &rhs)
{
    return lhs.filePath < rhs.filePath;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

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
}

string SourceNode::getObjectFileOutputFilePath() const
{
    return target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o";
}

string SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o", pathPrint);
}

string SourceNode::getTarjanNodeName() const
{
    return node->filePath;
}

void SourceNode::updateBTarget(Builder &, unsigned short round)
{
    if (!round && selectiveBuild)
    {
        RealBTarget &realBTarget = getRealBTarget(round);
        PostCompile postCompile = target->updateSourceNodeBTarget(*this);
        postCompile.parseHeaderDeps(*this, round);
        realBTarget.exitStatus = postCompile.exitStatus;
        // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
        // cached compile-command would be different
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            // Mutex is needed because while parallel reading is safe, parallel read-write is not safe
            lock_guard<mutex> lk(buildCacheMutex);
            sourceJson->at(JConsts::compileCommand) =
                target->compiler.bTPath.generic_string() + " " + target->compileCommand;
        }
        lock_guard<mutex> lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
}

void SourceNode::setSourceNodeFileStatus(const string &ex, RealBTarget &realBTarget) const
{
    Node *objectFileNode = Node::getNodeFromString(
        target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ex, true, true);

    if (sourceJson->at(JConsts::compileCommand) !=
        target->compiler.bTPath.generic_string() + " " + target->compileCommand)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    if (objectFileNode->doesNotExist)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    if (node->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    for (const Json &str : sourceJson->at(JConsts::headerDependencies))
    {
        Node *headerNode = Node::getNodeFromString(str, true, true);
        if (headerNode->doesNotExist)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }

        if (headerNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
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

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, string logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

static mutex totalSMRuleFileCountMutex; // TODO: atomics should be used instead of this

void SMFile::decrementTotalSMRuleFileCount(Builder &builder)
{
    lock_guard<mutex> lk(totalSMRuleFileCountMutex);
    --target->moduleScopeData->totalSMRuleFileCount;
    if (!target->moduleScopeData->totalSMRuleFileCount)
    {
        // A smrule file was updated. And all module scope smrule files have been checked. Following is done to
        // write the CppSourceTarget .cache files. So, if because of an error during smrule generation of a file,
        // hmake is exiting after round 1, in next invocation, it won't generate the smrule of successfully
        // generated files.
        builder.addCppSourceTargetsInFinalBTargets(target->moduleScopeData->targets);
    }
}

static mutex smFilesInternalMutex;

void SMFile::updateBTarget(Builder &builder, unsigned short round)
{
    // Danger Following is executed concurrently
    RealBTarget &realBTarget = getRealBTarget(round);
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
            postCompile.parseHeaderDeps(*this, round);
            realBTarget.exitStatus = postCompile.exitStatus;
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            {
                // Maybe fine-grain this mutex by using multiple mutexes in following functions. And first use
                // set::contain and then set::emplace. Because set::contains is thread-safe while set::emplace isn't
                lock_guard<mutex> lk(smFilesInternalMutex);
                saveRequiresJsonAndInitializeHeaderUnits(builder);
                assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            }

            // if(builder.finalBTargetsSizeGoal == )
            target->getRealBTarget(0).addDependency(*this);
        }
        decrementTotalSMRuleFileCount(builder);
    }
    else if (!round && selectiveBuild && realBTarget.exitStatus == EXIT_SUCCESS)
    {
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
            // Mutex is needed because while parallel reading is safe, parallel read-write is not safe
            lock_guard<mutex> lk(buildCacheMutex);
            sourceJson->at(JConsts::compileCommand) =
                target->compiler.bTPath.generic_string() + " " + target->compileCommand;
            target->targetCacheChanged = true;
        }
    }
}

void SMFile::saveRequiresJsonAndInitializeHeaderUnits(Builder &builder)
{
    if (readJsonFromSMRulesFile)
    {
        string smFilePath = target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".smrules";
        ifstream(smFilePath) >> sourceJson->at(JConsts::smrules);
    }

    Json &rule = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
    if (rule.contains("provides"))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
        if (auto [pos, ok] = target->moduleScopeData->requirePaths.try_emplace(logicalName, this); !ok)
        {
            const auto &[key, val] = *pos;
            printErrorMessageColor(
                fmt::format(
                    "In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                    target->moduleScope->getSubDirForTarget(), logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            decrementTotalSMRuleFileCount(builder);
            throw std::exception();
        }
    }
    iterateRequiresJsonToInitializeNewHeaderUnits(builder);
}

void SMFile::initializeNewHeaderUnit(const Json &requireJson, Builder &builder)
{
    string requireLogicalName = requireJson.at("logical-name").get<string>();
    if (requireLogicalName == logicalName)
    {
        printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                           target->moduleScope->getSubDirForTarget(), node->filePath),
                               settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    string headerUnitPath = path(requireJson.at("source-path").get<string>()).lexically_normal().generic_string();

    // lexically_relative in SMFile::initializeNewHeaderUnit does not work otherwise
    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. Sometimes, it returns normal however.
        for (char &c : headerUnitPath)
        {
            // Warning: assuming paths to be ASCII
            c = tolower(c);
        }
    }

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const InclNode *nodeDir = nullptr;

    // Iterating over all header-unit-directories of the module-scope to find out which header-unit
    // directory this header-unit comes from and which target that header-unit-directory belongs to
    // if any
    for (const auto &[inclNode, targetLocal] : target->moduleScopeData->huDirTarget)
    {
        const InclNode *dirNode = inclNode;
        path result = path(headerUnitPath).lexically_relative(dirNode->node->filePath).generic_string();
        if (!result.empty() && !result.generic_string().starts_with(".."))
        {
            if (huDirTarget)
            {
                printErrorMessageColor(
                    fmt::format(
                        "Module Header Unit\n{}\n belongs to two Target Header Unit Includes\n{}\n{}\nof Module "
                        "Scope\n{}\n",
                        headerUnitPath, nodeDir->node->filePath, dirNode->node->filePath,
                        target->moduleScope->getTargetPointer()),
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
                headerUnitPath, target->moduleScope->getTargetPointer()),
            settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    Node *headerUnitNode = Node::getNodeFromString(headerUnitPath, true);

    set<SMFile, CompareSourceNode>::const_iterator headerUnitIt;
    if (auto it = huDirTarget->moduleScopeData->headerUnits.find(headerUnitNode);
        it == huDirTarget->moduleScopeData->headerUnits.end())
    {
        headerUnitIt = huDirTarget->moduleScopeData->headerUnits.emplace(huDirTarget, headerUnitNode).first;
        huDirTarget->headerUnits.emplace(&(*headerUnitIt));

        auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

        if (nodeDir->ignoreHeaderDeps)
        {
            headerUnit.ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
        }

        // No other thread during BTarget::updateBTarget in round 1 calls saveBuildCache() i.e. only the following
        // operation needs to be guarded by the mutex, otherwise all the targetBuildCache access would have been guarded
        // Also each smfile deals with its own cache json, so, only the emplacing in the cache needs to be guarded.
        {
            lock_guard<mutex> lk(buildCacheMutex);
            Json &headerUnitsJson = huDirTarget->targetBuildCache->operator[](JConsts::moduleDependencies);
            const auto &[pos, Ok] = headerUnitsJson.emplace(headerUnit.node->filePath, Json::object_t{});
            if (Ok)
            {
                Json &moduleJson = pos.value();
                moduleJson.emplace(JConsts::compileCommand, Json::string_t{});
                moduleJson.emplace(JConsts::headerDependencies, Json::array_t{});
                moduleJson.emplace(JConsts::smrules, Json::object_t{});
            }
            headerUnit.sourceJson = pos.operator->();
        }

        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        {
            lock_guard<mutex> lk(totalSMRuleFileCountMutex);
            ++target->moduleScopeData->totalSMRuleFileCount;
        }

        builder.addNewBTargetInFinalBTargets(&headerUnit);
    }
    else
    {
        headerUnitIt = it;
    }

    auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

    headerUnitsConsumptionMethods[&headerUnit].emplace(requireJson.at("lookup-method").get<string>() == "include-angle",
                                                       requireLogicalName);
    getRealBTarget(0).addDependency(headerUnit);
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder)
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        Json &rules = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
        if (auto it = rules.find(JConsts::requires_); it != rules.end())
        {
            for (const Json &requireJson : *it)
            {
                initializeNewHeaderUnit(requireJson, builder);
            }
        }
    }
    else
    {
        // If following remains false then source is GlobalModuleFragment.
        bool hasLogicalNameRequireDependency = false;
        // If following is true then smFile is PartitionImplementation.
        bool hasPartitionExportDependency = false;
        Json &rules = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
        if (auto it = rules.find(JConsts::requires_); it != rules.end())
        {
            for (const Json &requireJson : *it)
            {
                string requireLogicalName = requireJson.at("logical-name").get<string>();
                if (requireLogicalName == logicalName)
                {
                    printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                                       target->moduleScope->getSubDirForTarget(), node->filePath),
                                           settings.pcSettings.toolErrorOutput);
                    decrementTotalSMRuleFileCount(builder);
                    throw std::exception();
                }
                if (requireJson.contains("lookup-method"))
                {
                    initializeNewHeaderUnit(requireJson, builder);
                }
                else
                {
                    hasLogicalNameRequireDependency = true;
                    if (requireLogicalName.contains(':'))
                    {
                        hasPartitionExportDependency = true;
                    }
                }
            }
        }
        if (hasProvide)
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
    RealBTarget &realBTarget = getRealBTarget(0);

    if (sourceJson->at(JConsts::compileCommand) !=
        target->compiler.bTPath.generic_string() + " " + target->compileCommand)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        readJsonFromSMRulesFile = true;
        return true;
    }
    else
    {
        Node *objectFileNode = Node::getNodeFromString(
            target->buildCacheFilesDirPath + path(node->filePath).filename().string() + (".m.o"), true, true);

        if (objectFileNode->doesNotExist)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            if (node->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            else
            {
                for (const Json &str : sourceJson->at(JConsts::headerDependencies))
                {
                    Node *headerNode = Node::getNodeFromString(str, true, true);
                    if (headerNode->doesNotExist)
                    {
                        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                        break;
                    }

                    if (headerNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
                    {
                        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                        break;
                    }
                }
            }
        }
    }

    if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
    {
        readJsonFromSMRulesFile = true;

        // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
        // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the .m.o file

        Node *smRuleFileNode = Node::getNodeFromString(
            target->buildCacheFilesDirPath + path(node->filePath).filename().string() + (".smrules"), true, true);

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
                for (const Json &str : sourceJson->at(JConsts::headerDependencies))
                {
                    Node *headerNode = Node::getNodeFromString(str, true, true);
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

string SMFile::getObjectFileOutputFilePath() const
{
    return target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".m.o";
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".m.o",
                          pathPrint);
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::duringSort(Builder &builder, unsigned short round)
{
    if (!round)
    {
        path objectFilePath;

        RealBTarget &realBTarget = getRealBTarget(0);
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

                if (realBTarget.fileStatus == FileStatus::UPDATED)
                {
                    Node *objectFileNode = Node::getNodeFromString(
                        target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".m.o", true);

                    string depFileName = path(smFile->node->filePath).filename().string();
                    Node *depObjectFileNode = Node::getNodeFromString(
                        smFile->target->buildCacheFilesDirPath + depFileName + ".m.o", true, true);

                    if (depObjectFileNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
                    {
                        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                        return;
                    }
                }
            }
        };

        // TODO
        //  Following could be moved to updateBTarget, so that it could be parallel. Same in LinkOrArchiveTarget. So,
        //  this function can be removed. preSort function will set the RealBTarget round 0 status to NEEDS_UPDATE, so
        //  that following could be called in updateBTarget. dependencyNeedsUpdate variable in RealBTarget will be used
        //  to determine whether the file needs an update because of its dependencies or not.
        for (auto &[dependency, ignore] : getRealBTarget(0).dependencies)
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
    BTarget::duringSort(builder, round);
}

string SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    string str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
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
        string angleStr = angle ? "angle " : "quote ";
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

string SMFile::getFlagPrint(const string &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    bool infra = ccpSettings.infrastructureFlags;

    string str = infra ? "/ifcOutput" : "";
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

string SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    string ifcFilePath = addQuotes(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc");

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
        string str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:";
            str += angleStr + " ";
            str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
        }
        return str;
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    string ifcFilePath = target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const string &logicalName_) {
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
            string str;
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
            string str;
            for (const HeaderUnitConsumer &headerUnitConsumer :
                 dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
            {
                if (ccpSettings.infrastructureFlags)
                {
                    string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
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

string SMFile::getModuleCompileCommandPrintLastHalf()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
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
        getFlagPrint(target->buildCacheFilesDirPath + path(node->filePath).filename().string());
    return moduleCompileCommandPrintLastHalf;
}