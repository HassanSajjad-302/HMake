#include "SMFile.hpp"

#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "Target.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <utility>

using std::filesystem::directory_entry;
using std::filesystem::file_type;

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

const Node *Node::getNodeFromString(const string &str, bool isFile)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath.lexically_normal();

    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.
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

CachedFile::CachedFile(const string &filePath) : node{Node::getNodeFromString(filePath, true)}
{
}

SourceNode::SourceNode(const string &filePath, ReportResultTo *reportResultTo_, const ResultType resultType_)
    : CachedFile(filePath), BTarget(reportResultTo_, resultType_)
{
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j[JConsts::srcFile] = sourceNode.node->filePath;
    j[JConsts::headerDependencies] = sourceNode.headerDependencies;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return &(lhs.node) < &(rhs.node);
}

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, string logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

bool operator<(const HeaderUnitConsumer &lhs, const HeaderUnitConsumer &rhs)
{
    return std::tie(lhs.angle, lhs.logicalName) < std::tie(rhs.angle, rhs.logicalName);
}

SMFile::SMFile(const string &srcPath, Target *target_)
    : SourceNode(srcPath, target_, ResultType::CPP_SMFILE), target{*target_}
{
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
        return infra ? "/interface " : "" + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        string angleStr = angle ? "angle " : "quote ";
        return infra ? "/exportHeader /headerName:" + angleStr : "" + str;
    }
    else
    {
        str = infra ? "/Fo" : "" + getReducedPath(outputFilesWithoutExtension + ".o", ccpSettings.objectFile);
        if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            return infra ? "/internalPartition " : "" + str;
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
        ifcFilePath = addQuotes((path(*(target.variantFilePath)).parent_path() / "shus/").generic_string() +
                                path(node->filePath).filename().string() + ".ifc");
    }
    else
    {
        ifcFilePath = addQuotes(target.buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc");
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
    string ifcFilePath = target.buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc";
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
            string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:" + angleStr + " ";
            if (!ccpSettings.infrastructureFlags)
            {
                str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
            }
        }
        return str;
    }
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
}

string SMFile::getModuleCompileCommandPrintLastHalf() const
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const BTarget *bTarget : allDependencies)
        {
            if (bTarget->resultType == ResultType::CPP_MODULE)
            {
                auto smFileDependency = static_cast<const SMFile *>(bTarget);
                moduleCompileCommandPrintLastHalf += smFileDependency->getRequireFlagPrint(*this);
            }
        }
    }

    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? target.getInfrastructureFlags() : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf +=
        getFlagPrint(target.buildCacheFilesDirPath + path(node->filePath).filename().string());
    return moduleCompileCommandPrintLastHalf;
}

SMFileVariantPathAndLogicalName::SMFileVariantPathAndLogicalName(const string &logicalName_,
                                                                 const string &variantFilePath_)
    : logicalName(logicalName_), variantFilePath(variantFilePath_)
{
}

bool SMFilePointerComparator::operator()(const SMFile *lhs, const SMFile *rhs) const
{
    return SMFilePathAndVariantPathComparator().operator()(*lhs, *rhs);
}

bool SMFilePathAndVariantPathComparator::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    if (&(lhs.node) != &(rhs.node))
    {
        return &(lhs.node) < &(rhs.node);
    }
    return lhs.target.variantFilePath < rhs.target.variantFilePath;
}

bool SMFileCompareLogicalName::operator()(const SMFileVariantPathAndLogicalName lhs,
                                          const SMFileVariantPathAndLogicalName rhs) const
{
    if (&(lhs.variantFilePath) != &(rhs.variantFilePath))
    {
        return (&(lhs.variantFilePath) < &(rhs.variantFilePath));
    }
    return (lhs.logicalName < rhs.logicalName);
}
