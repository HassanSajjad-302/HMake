#include "SMFile.hpp"

#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>

using std::filesystem::directory_entry, std::filesystem::file_type, std::tie, std::ifstream,
    std::filesystem::file_time_type;

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

Node::Node(const path &filePath_, bool isFile)
{
    if (directory_entry(filePath_).status().type() == (isFile ? file_type::regular : file_type::directory))
    {
        filePath = filePath_.generic_string();
    }
    else
    {
        print(stderr, "{} is not a {} file. File Type is {}\n", filePath_.generic_string(),
              isFile ? "regular" : "directory", getStatusString(filePath_));
        exit(EXIT_FAILURE);
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

static std::mutex nodeInsertMutex;
const Node *Node::getNodeFromString(const string &str, bool isFile)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal();

    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.
    std::lock_guard<std::mutex> lk(nodeInsertMutex);
    return allFiles.emplace(filePath, isFile).first.operator->();
}

bool operator<(const Node &lhs, const Node &rhs)
{
    return lhs.filePath < rhs.filePath;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

SourceNode::SourceNode(CppSourceTarget *target_, const string &filePath)
    : target(target_), node{Node::getNodeFromString(filePath, true)}
{
}

string SourceNode::getObjectFileOutputFilePath()
{
    return addQuotes(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o");
}

string SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint)
{
    return getReducedPath(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o", pathPrint);
}

void SourceNode::updateBTarget(unsigned short round, Builder &)
{
    if (!round && selectiveBuild)
    {
        RealBTarget &realBTarget = getRealBTarget(round);
        if (realBTarget.dependenciesExitStatus == EXIT_SUCCESS)
        {
            postCompile = std::make_shared<PostCompile>(target->updateSourceNodeBTarget(*this));
            postCompile->executePostCompileRoutineWithoutMutex(*this);
            realBTarget.exitStatus = postCompile->exitStatus;
            realBTarget.fileStatus = FileStatus::UPDATED;
        }
        else
        {
            realBTarget.exitStatus = EXIT_FAILURE;
        }
    }
}

void SourceNode::printMutexLockRoutine(unsigned short)
{
    RealBTarget &realBTarget = getRealBTarget(0);
    if (selectiveBuild && realBTarget.dependenciesExitStatus == EXIT_SUCCESS)
    {
        postCompile->executePrintRoutine(settings.pcSettings.compileCommandColor, false);
    }
}

void SourceNode::setSourceNodeFileStatus(const string &ex, unsigned short round)
{
    RealBTarget &realBTarget = getRealBTarget(round);
    realBTarget.fileStatus = FileStatus::UPDATED;

    path objectFilePath = path(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ex);

    if (!std::filesystem::exists(objectFilePath))
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    if (node->getLastUpdateTime() > objectFileLastEditTime)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {
        for (const Node *headerNode : headerDependencies)
        {
            if (headerNode->getLastUpdateTime() > objectFileLastEditTime)
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                break;
            }
        }
    }
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j[JConsts::srcFile] = sourceNode.node->filePath;
    j[JConsts::headerDependencies] = sourceNode.headerDependencies;
}

void to_json(Json &j, const SourceNode *smFile)
{
    j = *smFile;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node < rhs.node;
}

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, string logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

SMFile::SMFile(CppSourceTarget *target_, const string &srcPath) : SourceNode(target_, srcPath)
{
}

static std::mutex smFilesInternalMutex;
void SMFile::updateBTarget(unsigned short round, Builder &builder)
{
    // Danger Following is executed concurrently
    RealBTarget &realBTarget = getRealBTarget(round);
    if (round == 1)
    {
        if (generateSMFileInRoundOne)
        {
            postBasic = std::make_shared<PostBasic>(target->GenerateSMRulesFile(*this, true));
        }
        {
            // Maybe fine-grain this mutex by using multiple mutexes in following functions. And first use set::contain
            // and then set::emplace. Because set::contains is thread-safe while set::emplace isn't
            std::lock_guard<std::mutex> lk(smFilesInternalMutex);
            saveRequiresJsonAndInitializeHeaderUnits(builder);
        }
        setSMFileStatusRoundZero();
    }
    else if (!round && selectiveBuild)
    {
        if (realBTarget.dependenciesExitStatus == EXIT_SUCCESS)
        {
            postCompile = std::make_shared<PostCompile>(target->CompileSMFile(*this));
            realBTarget.exitStatus = postCompile->exitStatus;
            postCompile->executePostCompileRoutineWithoutMutex(*this);
        }
        else
        {
            realBTarget.exitStatus = EXIT_FAILURE;
        }
    }
    realBTarget.fileStatus = FileStatus::UPDATED;
}

void SMFile::printMutexLockRoutine(unsigned short round)
{
    if (round == 1 && generateSMFileInRoundOne)
    {
        postBasic->executePrintRoutine(settings.pcSettings.compileCommandColor, true);
    }
    else if (!round && selectiveBuild && getRealBTarget(0).dependenciesExitStatus == EXIT_SUCCESS)
    {
        postCompile->executePrintRoutine(settings.pcSettings.compileCommandColor, false);
    }
}

string SMFile::getObjectFileOutputFilePath()
{
    return standardHeaderUnit ? addQuotes(target->getSHUSPath() + path(node->filePath).filename().string() + ".o")
                              : SourceNode::getObjectFileOutputFilePath();
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint)
{
    return standardHeaderUnit
               ? getReducedPath(target->getSHUSPath() + path(node->filePath).filename().string() + ".o", pathPrint)
               : SourceNode::getObjectFileOutputFilePathPrint(pathPrint);
}

void SMFile::saveRequiresJsonAndInitializeHeaderUnits(Builder &builder)
{
    string smFilePath = target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".smrules";
    Json smFileJson;
    ifstream(smFilePath) >> smFileJson;
    Json rule = smFileJson.at("rules").get<Json>()[0];
    ModuleScopeData &moduleScopeData = CppSourceTarget::moduleScopes.find(target->moduleScope)->second;
    if (rule.contains("provides"))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
        if (auto [pos, ok] = moduleScopeData.requirePaths.emplace(logicalName, this); !ok)
        {
            const auto &[key, val] = *pos;
            // TODO
            // Mention the module scope too.
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                  target->moduleScope->getSubDirForTarget(), logicalName, node->filePath, val->node->filePath);
            exit(EXIT_FAILURE);
        }
    }
    requiresJson = std::move(rule.at("requires"));
    iterateRequiresJsonToInitializeNewHeaderUnits(moduleScopeData, builder);
}

void SMFile::initializeNewHeaderUnit(const Json &requireJson, ModuleScopeData &moduleScopeData, Builder &builder)
{
    string requireLogicalName = requireJson.at("logical-name").get<string>();
    if (requireLogicalName == logicalName)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", node->filePath);
        exit(EXIT_FAILURE);
    }

    string headerUnitPath = path(requireJson.at("source-path").get<string>()).lexically_normal().generic_string();

    bool isStandardHeaderUnit = false;
    bool isApplicationHeaderUnit = false;

    // The target from which this header-unit comes from or this target's moduleScope if header-unit is
    // a standard header-unit
    CppSourceTarget *ahuDirTarget = nullptr;
    if (isSubDirPathStandard(headerUnitPath, target->standardIncludes))
    {
        isStandardHeaderUnit = true;
        ahuDirTarget = target->moduleScope;
    }
    if (!isStandardHeaderUnit)
    {
        // Iterating over all header-unit-directories of the module-scope to find out which header-unit
        // directory this header-unit comes from and which target that header-unit-directory belongs to
        // if any
        for (auto &dir : moduleScopeData.appHUDirTarget)
        {
            const Node *dirNode = dir.first;
            path result = path(headerUnitPath).lexically_relative(dirNode->filePath).generic_string();
            if (!result.empty() && !result.generic_string().starts_with(".."))
            {
                isApplicationHeaderUnit = true;
                ahuDirTarget = dir.second;
            }
        }
    }
    if (!isStandardHeaderUnit && !isApplicationHeaderUnit)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Module Header Unit\n{}\n is neither a Standard Header Unit nor belongs to any Target "
              "Header Unit Includes of Module Scope\n{}\n",
              headerUnitPath, target->moduleScope->getTargetPointer());
        exit(EXIT_FAILURE);
    }

    const auto &[pos1, Ok1] = moduleScopeData.headerUnits.emplace(ahuDirTarget, headerUnitPath);
    if (!pos1->presentInSource)
    {
        auto &smFileHeaderUnit = const_cast<SMFile &>(*pos1);
        smFileHeaderUnit.presentInSource = true;
        smFileHeaderUnit.standardHeaderUnit = true;
        smFileHeaderUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        headerUnitsConsumptionMethods[&smFileHeaderUnit].emplace(
            requireJson.at("lookup-method").get<string>() == "include-angle", requireLogicalName);
        ahuDirTarget->applicationHeaderUnits.emplace(&smFileHeaderUnit);
        RealBTarget &smFileReal = smFileHeaderUnit.getRealBTarget(1);
        if (!smFileHeaderUnit.presentInCache)
        {
            smFileReal.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            smFileHeaderUnit.setSourceNodeFileStatus(".smrules", 1);
        }
        if (smFileReal.fileStatus == FileStatus::NEEDS_UPDATE)
        {
            smFileHeaderUnit.generateSMFileInRoundOne = true;
        }
        getRealBTarget(0).addDependency(smFileHeaderUnit);
        builder.addNewBTargetInFinalBTargets(&smFileHeaderUnit);
    }
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(ModuleScopeData &moduleScopeData, Builder &builder)
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        for (const Json &requireJson : requiresJson)
        {
            initializeNewHeaderUnit(requireJson, moduleScopeData, builder);
        }
    }
    else
    {
        // If following remains false then source is GlobalModuleFragment.
        bool hasLogicalNameRequireDependency = false;
        // If following is true then smFile is PartitionImplementation.
        bool hasPartitionExportDependency = false;
        for (const Json &requireJson : requiresJson)
        {
            string requireLogicalName = requireJson.at("logical-name").get<string>();
            if (requireLogicalName == logicalName)
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", node->filePath);
                exit(EXIT_FAILURE);
            }
            if (requireJson.contains("lookup-method"))
            {
                initializeNewHeaderUnit(requireJson, moduleScopeData, builder);
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

void SMFile::setSMFileStatusRoundZero()
{
    RealBTarget &realBTarget = getRealBTarget(0);
    if (realBTarget.fileStatus != FileStatus::NEEDS_UPDATE)
    {
        path objectFilePath;
        if (type == SM_FILE_TYPE::HEADER_UNIT && standardHeaderUnit)
        {
            objectFilePath = path(target->moduleScope->getSubDirForTarget() + "shus/" +
                                  path(node->filePath).filename().string() + ".o");
            if (!exists(objectFilePath))
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            return;
        }

        string fileName = path(node->filePath).filename().string();
        objectFilePath = path(target->buildCacheFilesDirPath + fileName + ".o");
        Node *smRuleNode =
            const_cast<Node *>(Node::getNodeFromString(target->buildCacheFilesDirPath + fileName + ".smrules", true));
        if (!exists(path(smRuleNode->filePath)))
        {
            print(stderr,
                  "Warning. Following smrules not found while checking the object-file-status which must had been "
                  "generated in round 1.\n{}\n",
                  smRuleNode->filePath);
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        if (!exists(objectFilePath))
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
        if (smRuleNode->getLastUpdateTime() > objectFileLastEditTime)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        }
    }
}

bool SMFile::isSubDirPathStandard(const path &headerUnitPath, set<const Node *> &standardIncludes)
{
    string headerUnitPathSMallCase = headerUnitPath.generic_string();
    for (auto &c : headerUnitPathSMallCase)
    {
        c = (char)::tolower(c);
    }
    for (const Node *stdInclude : standardIncludes)
    {
        string directoryPathSmallCase = stdInclude->filePath;
        for (auto &c : directoryPathSmallCase)
        {
            c = (char)::tolower(c);
        }
        path result = path(headerUnitPathSMallCase).lexically_relative(directoryPathSmallCase).generic_string();
        if (!result.empty() && !result.generic_string().starts_with(".."))
        {
            return true;
        }
    }
    return false;
}

void SMFile::duringSort(Builder &, unsigned short round)
{
    if (round)
    {
        return;
    }
    for (BTarget *bTarget : getRealBTarget(0).allDependencies)
    {
        if (auto *smFile = dynamic_cast<SMFile *>(bTarget); smFile)
        {
            for (auto &[headerUnitSMFile, headerUnitConsumerSet] : smFile->headerUnitsConsumptionMethods)
            {
                for (const HeaderUnitConsumer &headerUnitConsumer : headerUnitConsumerSet)
                {
                    headerUnitsConsumptionMethods.emplace(headerUnitSMFile, set<HeaderUnitConsumer>{})
                        .first->second.emplace(headerUnitConsumer);
                }
            }
        }
    }
}

string SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    string str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
                 addQuotes(outputFilesWithoutExtension + ".o");
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
        str = "/Fo" + addQuotes(outputFilesWithoutExtension + ".o");
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
        str += getReducedPath(outputFilesWithoutExtension + ".o", ccpSettings.objectFile);
    }

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
        str = infra ? "/Fo" : "" + getReducedPath(outputFilesWithoutExtension + ".o", ccpSettings.objectFile);
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
    string ifcFilePath;
    if (standardHeaderUnit)
    {
        ifcFilePath = addQuotes(target->getSHUSPath() + path(node->filePath).filename().string() + ".ifc");
    }
    else
    {
        ifcFilePath = addQuotes(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc");
    }
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, "HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
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
        print(stderr, "HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return ccpSettings.infrastructureFlags ? "/reference " : "" + getRequireIFCPathOrLogicalName(logicalName) + " ";
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        string str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            if (!ccpSettings.infrastructureFlags)
            {
                string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
        }
        return str;
    }
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
}

string SMFile::getModuleCompileCommandPrintLastHalf()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const BTarget *bTarget : getRealBTarget(0).allDependencies)
        {
            if (auto smFileDependency = static_cast<const SMFile *>(bTarget); smFileDependency)
            {
                moduleCompileCommandPrintLastHalf += smFileDependency->getRequireFlagPrint(*this);
            }
        }
    }

    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? target->getInfrastructureFlags() : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf +=
        getFlagPrint(target->buildCacheFilesDirPath + path(node->filePath).filename().string());
    return moduleCompileCommandPrintLastHalf;
}