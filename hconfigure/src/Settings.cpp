#include "Settings.hpp"
#include "JConsts.hpp"
#include "fmt/format.h"

using fmt::print;
void to_json(Json &json, const PathPrint &pathPrint)
{
    json[JConsts::pathPrintLevel] = pathPrint.printLevel;
    json[JConsts::depth] = pathPrint.depth;
    json[JConsts::addQuotes] = pathPrint.addQuotes;
}

void from_json(const Json &json, PathPrint &pathPrint)
{
    uint8_t level = json.at(JConsts::pathPrintLevel).get<uint8_t>();
    if (level < 0 || level > 2)
    {
        print(stderr, "Level should be in range 0-2\n");
        exit(EXIT_FAILURE);
    }
    pathPrint.printLevel = (PathPrintLevel)level;
    pathPrint.depth = json.at(JConsts::depth).get<int>();
    pathPrint.addQuotes = json.at(JConsts::addQuotes).get<bool>();
}

void to_json(Json &json, const CompileCommandPrintSettings &ccpSettings)
{
    json[JConsts::tool] = ccpSettings.tool;
    json[JConsts::environmentCompilerFlags] = ccpSettings.environmentCompilerFlags;
    json[JConsts::compilerFlags] = ccpSettings.compilerFlags;
    json[JConsts::compileDefinitions] = ccpSettings.compileDefinitions;
    json[JConsts::projectIncludeDirectories] = ccpSettings.projectIncludeDirectories;
    json[JConsts::environmentIncludeDirectories] = ccpSettings.environmentIncludeDirectories;
    json[JConsts::onlyLogicalNameOfRequireIfc] = ccpSettings.onlyLogicalNameOfRequireIFC;
    json[JConsts::requireIfcs] = ccpSettings.requireIFCs;
    json[JConsts::sourceFile] = ccpSettings.sourceFile;
    json[JConsts::infrastructureFlags] = ccpSettings.infrastructureFlags;
    json[JConsts::ifcOutputFile] = ccpSettings.ifcOutputFile;
    json[JConsts::objectFile] = ccpSettings.objectFile;
    json[JConsts::outputAndErrorFiles] = ccpSettings.outputAndErrorFiles;
    json[JConsts::pruneHeaderDependenciesFromMsvcOutput] = ccpSettings.pruneHeaderDepsFromMSVCOutput;
    json[JConsts::pruneCompiledSourceFileNameFromMsvcOutput] = ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput;
    json[JConsts::ratioForHmakeTime] = ccpSettings.ratioForHMakeTime;
    json[JConsts::showPercentage] = ccpSettings.showPercentage;
}

void from_json(const Json &json, CompileCommandPrintSettings &ccpSettings)
{
    ccpSettings.tool = json.at(JConsts::tool).get<PathPrint>();
    ccpSettings.tool.isTool = true;
    ccpSettings.environmentCompilerFlags = json.at(JConsts::environmentCompilerFlags).get<bool>();
    ccpSettings.compilerFlags = json.at(JConsts::compilerFlags).get<bool>();
    ccpSettings.compileDefinitions = json.at(JConsts::compileDefinitions).get<bool>();
    ccpSettings.projectIncludeDirectories = json.at(JConsts::projectIncludeDirectories).get<PathPrint>();
    ccpSettings.environmentIncludeDirectories = json.at(JConsts::environmentIncludeDirectories).get<PathPrint>();
    ccpSettings.onlyLogicalNameOfRequireIFC = json.at(JConsts::onlyLogicalNameOfRequireIfc).get<bool>();
    ccpSettings.requireIFCs = json.at(JConsts::requireIfcs).get<PathPrint>();
    ccpSettings.sourceFile = json.at(JConsts::sourceFile).get<PathPrint>();
    ccpSettings.infrastructureFlags = json.at(JConsts::infrastructureFlags).get<bool>();
    ccpSettings.ifcOutputFile = json.at(JConsts::ifcOutputFile).get<PathPrint>();
    ccpSettings.objectFile = json.at(JConsts::objectFile).get<PathPrint>();
    ccpSettings.outputAndErrorFiles = json.at(JConsts::outputAndErrorFiles).get<PathPrint>();
    ccpSettings.pruneHeaderDepsFromMSVCOutput = json.at(JConsts::pruneHeaderDependenciesFromMsvcOutput).get<bool>();
    ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput =
        json.at(JConsts::pruneCompiledSourceFileNameFromMsvcOutput).get<bool>();
    ccpSettings.ratioForHMakeTime = json.at(JConsts::ratioForHmakeTime).get<bool>();
    ccpSettings.showPercentage = json.at(JConsts::showPercentage).get<bool>();
}

void to_json(Json &json, const ArchiveCommandPrintSettings &acpSettings)
{
    json[JConsts::tool] = acpSettings.tool;
    json[JConsts::infrastructureFlags] = acpSettings.infrastructureFlags;
    json[JConsts::objectFiles] = acpSettings.objectFiles;
    json[JConsts::archive] = acpSettings.archive;
    json[JConsts::outputAndErrorFiles] = acpSettings.outputAndErrorFiles;
}

void from_json(const Json &json, ArchiveCommandPrintSettings &acpSettings)
{
    acpSettings.tool = json.at(JConsts::tool).get<PathPrint>();
    acpSettings.tool.isTool = true;
    acpSettings.infrastructureFlags = json.at(JConsts::infrastructureFlags).get<bool>();
    acpSettings.objectFiles = json.at(JConsts::objectFiles).get<PathPrint>();
    acpSettings.archive = json.at(JConsts::archive).get<PathPrint>();
    acpSettings.outputAndErrorFiles = json.at(JConsts::outputAndErrorFiles).get<PathPrint>();
}

void to_json(Json &json, const LinkCommandPrintSettings &lcpSettings)
{
    json[JConsts::tool] = lcpSettings.tool;
    json[JConsts::infrastructureFlags] = lcpSettings.infrastructureFlags;
    json[JConsts::objectFiles] = lcpSettings.objectFiles;
    json[JConsts::libraryDependencies] = lcpSettings.libraryDependencies;
    json[JConsts::libraryDirectories] = lcpSettings.libraryDirectories;
    json[JConsts::environmentLibraryDirectories] = lcpSettings.standardLibraryDirectories;
    json[JConsts::binary] = lcpSettings.binary;
}

void from_json(const Json &json, LinkCommandPrintSettings &lcpSettings)
{
    lcpSettings.tool = json.at(JConsts::tool).get<PathPrint>();
    lcpSettings.tool.isTool = true;
    lcpSettings.infrastructureFlags = json.at(JConsts::infrastructureFlags).get<bool>();
    lcpSettings.objectFiles = json.at(JConsts::objectFiles).get<PathPrint>();
    lcpSettings.libraryDependencies = json.at(JConsts::libraryDependencies).get<PathPrint>();
    lcpSettings.libraryDirectories = json.at(JConsts::libraryDirectories).get<PathPrint>();
    lcpSettings.standardLibraryDirectories = json.at(JConsts::environmentLibraryDirectories).get<PathPrint>();
    lcpSettings.binary = json.at(JConsts::objectFiles).get<PathPrint>();
}

void to_json(Json &json, const PrintColorSettings &printColorSettings)
{
    json[JConsts::compileCommandColor] = printColorSettings.compileCommandColor;
    json[JConsts::archiveCommandColor] = printColorSettings.archiveCommandColor;
    json[JConsts::linkCommandColor] = printColorSettings.linkCommandColor;
    json[JConsts::toolErrorOutput] = printColorSettings.toolErrorOutput;
    json[JConsts::hbuildStatementOutput] = printColorSettings.hbuildStatementOutput;
    json[JConsts::hbuildSequenceOutput] = printColorSettings.hbuildSequenceOutput;
    json[JConsts::hbuildErrorOutput] = printColorSettings.hbuildErrorOutput;
}

void from_json(const Json &json, PrintColorSettings &printColorSettings)
{
    printColorSettings.compileCommandColor = json.at(JConsts::compileCommandColor).get<int>();
    printColorSettings.archiveCommandColor = json.at(JConsts::archiveCommandColor).get<int>();
    printColorSettings.linkCommandColor = json.at(JConsts::linkCommandColor).get<int>();
    printColorSettings.toolErrorOutput = json.at(JConsts::toolErrorOutput).get<int>();
    printColorSettings.hbuildStatementOutput = json.at(JConsts::hbuildStatementOutput).get<int>();
    printColorSettings.hbuildSequenceOutput = json.at(JConsts::hbuildSequenceOutput).get<int>();
    printColorSettings.hbuildErrorOutput = json.at(JConsts::hbuildErrorOutput).get<int>();
}

void to_json(Json &json, const GeneralPrintSettings &generalPrintSettings)
{
    json[JConsts::preBuildCommandsStatement] = generalPrintSettings.preBuildCommandsStatement;
    json[JConsts::preBuildCommands] = generalPrintSettings.preBuildCommands;
    json[JConsts::postBuildCommandsStatement] = generalPrintSettings.postBuildCommandsStatement;
    json[JConsts::postBuildCommands] = generalPrintSettings.postBuildCommands;
    json[JConsts::copyingTarget] = generalPrintSettings.copyingTarget;
    json[JConsts::threadId] = generalPrintSettings.threadId;
}

void from_json(const Json &json, GeneralPrintSettings &generalPrintSettings)
{
    generalPrintSettings.preBuildCommandsStatement = json.at(JConsts::preBuildCommandsStatement).get<bool>();
    generalPrintSettings.preBuildCommands = json.at(JConsts::preBuildCommands).get<bool>();
    generalPrintSettings.postBuildCommandsStatement = json.at(JConsts::postBuildCommandsStatement).get<bool>();
    generalPrintSettings.postBuildCommands = json.at(JConsts::postBuildCommands).get<bool>();
    generalPrintSettings.copyingTarget = json.at(JConsts::copyingTarget).get<bool>();
    generalPrintSettings.threadId = json.at(JConsts::threadId).get<bool>();
}

void to_json(Json &json, const Settings &settings_)
{
    json[JConsts::maximumBuildThreads] = settings_.maximumBuildThreads;
    json[JConsts::maximumLinkThreads] = settings_.maximumLinkThreads;
    json[JConsts::compilePrintSettings] = settings_.ccpSettings;
    json[JConsts::archivePrintSettings] = settings_.acpSettings;
    json[JConsts::linkPrintSettings] = settings_.lcpSettings;
    json[JConsts::printColorSettings] = settings_.pcSettings;
    json[JConsts::generalPrintSettings] = settings_.gpcSettings;
}

void from_json(const Json &json, Settings &settings_)
{
    settings_.maximumBuildThreads = json.at(JConsts::maximumBuildThreads).get<unsigned int>();
    settings_.maximumLinkThreads = json.at(JConsts::maximumLinkThreads).get<unsigned int>();
    settings_.ccpSettings = json.at(JConsts::compilePrintSettings).get<CompileCommandPrintSettings>();
    settings_.acpSettings = json.at(JConsts::archivePrintSettings).get<ArchiveCommandPrintSettings>();
    settings_.lcpSettings = json.at(JConsts::linkPrintSettings).get<LinkCommandPrintSettings>();
    settings_.pcSettings = json.at(JConsts::printColorSettings).get<PrintColorSettings>();
    settings_.gpcSettings = json.at(JConsts::generalPrintSettings).get<GeneralPrintSettings>();
}

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint)
{
    assert(pathPrint.printLevel != PathPrintLevel::NO &&
           "HMake Internal Error. Function getReducedPath() should not had been "
           "called if PrintLevel is NO.");

    if (pathPrint.printLevel == PathPrintLevel::FULL)
    {
        return subjectPath;
    }

    auto nthOccurrence = [](const string &str, const string &findMe, size_t nth) -> size_t {
        size_t count = 0;

        for (size_t i = 0; i < str.size(); ++i)
        {
            if (str.size() > i + findMe.size())
            {
                bool found = true;
                for (size_t j = 0; j < findMe.size(); ++j)
                {
                    if (str[i + j] != findMe[j])
                    {
                        found = false;
                        break;
                    }
                }
                if (found)
                {
                    ++count;
                    if (count == nth)
                    {
                        return (int)i;
                    }
                }
            }
            else
            {
                break;
            }
        }
        return 0;
    };

    auto countSubstring = [](const string &str, const string &sub) -> size_t {
        if (sub.length() == 0)
            return 0;
        size_t count = 0;
        for (size_t offset = str.find(sub); offset != string::npos; offset = str.find(sub, offset + sub.length()))
        {
            ++count;
        }
        return count;
    };

    string str = subjectPath;
    unsigned int finalDepth = pathPrint.depth;
    if (pathPrint.isDirectory)
    {
        finalDepth += 1;
    }
    bool toolOnWindows = false;
#ifdef _WIN32
    if (pathPrint.isTool)
    {
        toolOnWindows = true;
    }
#endif
    size_t count = countSubstring(str, toolOnWindows ? "\\" : "/");
    if (finalDepth >= count)
    {
        return str;
    }
    size_t index = nthOccurrence(str, toolOnWindows ? "\\" : "/", count - finalDepth);
    if (!index)
    {
        return str;
    }
    else
    {
        return str.substr(index + 1, str.size() - 1);
    }
}