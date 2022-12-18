

#ifdef HMAKE_TRANSLATE_INCLUDE
import "BBuild.hpp";

import "fmt/color.h";
import "fmt/format.h";
import "fstream";
import "regex";
import <utility>;

#else
#include "BBuild.hpp"

#include <utility>

#include "fmt/color.h"
#include "fmt/format.h"
#include "fstream"
#include "regex"

#endif

using std::ifstream, std::ofstream, std::filesystem::copy_options, std::runtime_error, std::to_string,
    std::filesystem::create_directory, std::filesystem::directory_iterator, std::regex, std::filesystem::current_path,
    std::make_shared, std::make_pair, std::lock_guard, std::unique_ptr, std::make_unique, fmt::print,
    std::filesystem::file_time_type, std::filesystem::last_write_time, std::make_tuple, std::tie;

string getThreadId()
{
    string threadId;
    auto myId = std::this_thread::get_id();
    std::stringstream ss;
    ss << myId;
    threadId = ss.str();
    return threadId;
}

BIDD::BIDD(const string &path_, bool copy_) : path{path_}, copy{copy_}
{
}

void from_json(const Json &j, BIDD &p)
{
    p.copy = j.at(JConsts::copy).get<bool>();
    p.path = j.at(JConsts::path).get<string>();
}

void from_json(const Json &j, BLibraryDependency &p)
{
    p.preBuilt = j.at(JConsts::prebuilt).get<bool>();
    p.path = j.at(JConsts::path).get<string>();
    if (p.preBuilt)
    {
        if (j.contains(JConsts::importedFromOtherHmakePackage))
        {
            p.imported = j.at(JConsts::importedFromOtherHmakePackage).get<bool>();
            if (!p.imported)
            {
                p.hmakeFilePath = j.at(JConsts::hmakeFilePath).get<string>();
            }
        }
        else
        {
            // hbuild was ran in project variant mode.
            p.imported = true;
        }
    }
    p.copy = true;
}

void from_json(const Json &j, BCompileDefinition &p)
{
    p.name = j.at(JConsts::name).get<string>();
    p.value = j.at(JConsts::value).get<string>();
}

ParsedTarget::ParsedTarget(const string &targetFilePath_) : BTarget(this, ResultType::LINK)
{
    targetFilePath = targetFilePath_;
    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

    targetType = targetFileJson.at(JConsts::targetType).get<TargetType>();
    if (targetType != TargetType::EXECUTABLE && targetType != TargetType::STATIC && targetType != TargetType::SHARED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "BTargetType value in the targetFile is not correct.");
        exit(EXIT_FAILURE);
    }

    if (targetFileJson.at(JConsts::variant).get<string>() == JConsts::package)
    {
        packageMode = true;
    }
    else
    {
        packageMode = false;
    }
    if (packageMode)
    {
        copyPackage = targetFileJson.at(JConsts::packageCopy).get<bool>();
        if (copyPackage)
        {
            packageName = targetFileJson.at(JConsts::packageName).get<string>();
            packageCopyPath = targetFileJson.at(JConsts::packageCopyPath).get<string>();
            packageVariantIndex = targetFileJson.at(JConsts::packageVariantIndex).get<int>();
            packageTargetPath =
                packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" + targetName + "/";
        }
    }
    else
    {
        copyPackage = false;
    }
    outputName = targetFileJson.at(JConsts::outputName).get<string>();
    outputDirectory = targetFileJson.at(JConsts::outputDirectory).get<path>().string();
    compiler = targetFileJson.at(JConsts::compiler).get<Compiler>();
    linker = targetFileJson.at(JConsts::linker).get<Linker>();
    if (targetType == TargetType::STATIC)
    {
        archiver = targetFileJson.at(JConsts::archiver).get<Archiver>();
    }
    environment = targetFileJson.at(JConsts::environment).get<Environment>();
    compilerFlags = targetFileJson.at(JConsts::compilerFlags).get<string>();
    linkerFlags = targetFileJson.at(JConsts::linkerFlags).get<string>();
    // TODO: An Optimization
    //  If source for a target is collected from a sourceDirectory with some
    //  regex, then similar source should not be collected again.
    // TODO:
    // change this later to sourceFiles and uncomment the next line
    modulesSourceFiles =
        SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson, targetFilePath, "SOURCE_");
    /*modulesSourceFiles =
        SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson,
       targetFilePath, "MODULES_");*/

    libraryDependencies = targetFileJson.at(JConsts::libraryDependencies).get<vector<BLibraryDependency>>();
    if (packageMode)
    {
        includeDirectories = targetFileJson.at(JConsts::includeDirectories).get<vector<BIDD>>();
    }
    else
    {
        vector<string> includeDirs = targetFileJson.at(JConsts::includeDirectories).get<vector<string>>();
        for (auto &i : includeDirs)
        {
            includeDirectories.emplace_back(i, true);
        }
    }
    compilerTransitiveFlags = targetFileJson.at(JConsts::compilerTransitiveFlags).get<string>();
    linkerTransitiveFlags = targetFileJson.at(JConsts::linkerTransitiveFlags).get<string>();
    compileDefinitions = targetFileJson.at(JConsts::compileDefinitions).get<vector<BCompileDefinition>>();
    preBuildCustomCommands = targetFileJson.at(JConsts::preBuildCustomCommands).get<vector<string>>();
    postBuildCustomCommands = targetFileJson.at(JConsts::postBuildCustomCommands).get<vector<string>>();

    buildCacheFilesDirPath = (path(targetFilePath).parent_path() / ("Cache_Build_Files/")).generic_string();
    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at(JConsts::consumerDependencies).get<Json>();
    }
    // Parsing finished

    actualOutputName = getActualNameFromTargetName(targetType, Cache::osFamily, targetName);
    auto [pos, Ok] = variantFilePaths.emplace(path(targetFilePath).parent_path().parent_path().string() + "/" +
                                              (packageMode ? "packageVariant.hmake" : "projectVariant.hmake"));
    variantFilePath = &(*pos);
    if (!sourceFiles.empty() && !modulesSourceFiles.empty())
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "A Target can not have both module-source and regular-source. You "
              "can make regular-source dependency "
              "of module source.\nTarget: {}.\n",
              targetFilePath);
        exit(EXIT_FAILURE);
    }
}

void ParsedTarget::executePreBuildCommands() const
{
    if (!preBuildCustomCommands.empty() && settings.gpcSettings.preBuildCommandsStatement)
    {
        print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
              "Executing PRE_BUILD_CUSTOM_COMMANDS.\n");
    }
    for (auto &i : preBuildCustomCommands)
    {
        if (settings.gpcSettings.preBuildCommands)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)), pes, i + "\n");
        }
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
}

void ParsedTarget::executePostBuildCommands() const
{
    if (!postBuildCustomCommands.empty() && settings.gpcSettings.postBuildCommandsStatement)
    {
        print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
              "Executing POST_BUILD_CUSTOM_COMMANDS.\n");
    }
    for (auto &i : postBuildCustomCommands)
    {
        if (settings.gpcSettings.postBuildCommands)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)), pes, i + "\n");
        }
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
}

string &ParsedTarget::getCompileCommand()
{
    if (compileCommand.empty())
    {
        setCompileCommand();
    }
    return compileCommand;
}

void ParsedTarget::setCompileCommand()
{
    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    compileCommand = environment.compilerFlags + " ";
    compileCommand += compilerFlags + " ";
    compileCommand += compilerTransitiveFlags + " ";

    for (const auto &i : compileDefinitions)
    {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            compileCommand += "/D" + i.name + "=" + i.value + " ";
        }
        else
        {
            compileCommand += "-D" + i.name + "=" + i.value + " ";
        }
    }

    for (const auto &i : includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.path) + " ");
    }

    for (const auto &i : environment.includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
    }
}

void ParsedTarget::setSourceCompileCommandPrintFirstHalf()
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        sourceCompileCommandPrintFirstHalf +=
            getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    if (ccpSettings.environmentCompilerFlags)
    {
        sourceCompileCommandPrintFirstHalf += environment.compilerFlags + " ";
    }
    if (ccpSettings.compilerFlags)
    {
        sourceCompileCommandPrintFirstHalf += compilerFlags + " ";
    }
    if (ccpSettings.compilerTransitiveFlags)
    {
        sourceCompileCommandPrintFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : compileDefinitions)
    {
        if (ccpSettings.compileDefinitions)
        {
            if (compiler.bTFamily == BTFamily::MSVC)
            {
                sourceCompileCommandPrintFirstHalf += "/D " + addQuotes(i.name + "=" + addQuotes(i.value)) + " ";
            }
            else
            {
                sourceCompileCommandPrintFirstHalf += "-D" + i.name + "=" + i.value + " ";
            }
        }
    }

    for (const auto &i : includeDirectories)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(i.path, ccpSettings.projectIncludeDirectories) + " ";
        }
    }

    for (const auto &i : environment.includeDirectories)
    {
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() +
                getReducedPath(i.directoryPath.generic_string(), ccpSettings.environmentIncludeDirectories) + " ";
        }
    }
}

string &ParsedTarget::getSourceCompileCommandPrintFirstHalf()
{
    if (sourceCompileCommandPrintFirstHalf.empty())
    {
        setSourceCompileCommandPrintFirstHalf();
    }
    return sourceCompileCommandPrintFirstHalf;
}

void ParsedTarget::setLinkOrArchiveCommandAndPrint()
{
    const ArchiveCommandPrintSettings &acpSettings = settings.acpSettings;
    const LinkCommandPrintSettings &lcpSettings = settings.lcpSettings;

    if (targetType == TargetType::STATIC)
    {
        auto getLibraryPath = [&]() -> string {
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                return (path(outputDirectory) / (outputName + ".lib")).string();
            }
            else if (archiver.bTFamily == BTFamily::GCC)
            {
                return (path(outputDirectory) / ("lib" + outputName + ".a")).string();
            }
            return "";
        };

        if (acpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf +=
                getReducedPath(archiver.bTPath.make_preferred().string(), acpSettings.tool) + " ";
        }

        linkOrArchiveCommand = archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        }
        auto getArchiveOutputFlag = [&]() -> string {
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                return "/OUT:";
            }
            else if (archiver.bTFamily == BTFamily::GCC)
            {
                return " rcs ";
            }
            return "";
        };
        linkOrArchiveCommand += getArchiveOutputFlag();
        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += getArchiveOutputFlag();
        }

        linkOrArchiveCommand += addQuotes(getLibraryPath()) + " ";
        if (acpSettings.archive.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf += getReducedPath(getLibraryPath(), acpSettings.archive) + " ";
        }
    }
    else
    {
        if (lcpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf +=
                getReducedPath(linker.bTPath.make_preferred().string(), lcpSettings.tool) + " ";
        }

        linkOrArchiveCommand = linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";
        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";
        }

        linkOrArchiveCommand += linkerFlags + " ";
        if (lcpSettings.linkerFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += linkerFlags + " ";
        }

        linkOrArchiveCommand += linkerTransitiveFlags + " ";
        if (lcpSettings.linkerTransitiveFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += linkerTransitiveFlags + " ";
        }
    }

    auto getLinkFlag = [this](const string &libraryPath, const string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes(libraryPath + libraryName + ".lib") + " ";
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

    auto getLinkFlagPrint = [this](const string &libraryPath, const string &libraryName, const PathPrint &pathPrint) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return getReducedPath(libraryPath + libraryName + ".lib", pathPrint) + " ";
        }
        else
        {
            return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) + " ";
        }
    };

    for (BTarget *dependency : allDependencies)
    {
        if (dependencies.contains(dependency))
        {
            if (dependency->resultType == ResultType::LINK)
            {
                if (targetType != TargetType::STATIC)
                {
                    const auto *parsedTargetDependency = static_cast<const ParsedTarget *>(dependency);
                    linkOrArchiveCommand +=
                        getLinkFlag(parsedTargetDependency->outputDirectory, parsedTargetDependency->outputName);
                    linkOrArchiveCommandPrintFirstHalf +=
                        getLinkFlagPrint(parsedTargetDependency->outputDirectory, parsedTargetDependency->outputName,
                                         lcpSettings.libraryDependencies);
                }
            }
            else
            {
                ParsedTarget *parsedTarget;
                if (dependency->resultType == ResultType::SOURCENODE)
                {
                    parsedTarget = this;
                }
                else
                {
                    parsedTarget = &(static_cast<const SMFile *>(dependency)->parsedTarget);
                    assert(dependency->resultType == ResultType::CPP_MODULE &&
                           "ResultType must be either ResultType::SourceNode or ResultType::CPP_MODULE");
                }
                const PathPrint *pathPrint;
                if (targetType == TargetType::STATIC)
                {
                    pathPrint = &(acpSettings.objectFiles);
                }
                else
                {
                    pathPrint = &(lcpSettings.objectFiles);
                }
                const auto *sourceNodeDependency = static_cast<const SourceNode *>(dependency);
                linkOrArchiveCommand +=
                    addQuotes(parsedTarget->buildCacheFilesDirPath + sourceNodeDependency->node->fileName + ".o") + " ";
                if (pathPrint->printLevel != PathPrintLevel::NO)
                {
                    linkOrArchiveCommandPrintFirstHalf +=
                        getReducedPath(parsedTarget->buildCacheFilesDirPath + sourceNodeDependency->node->fileName +
                                           ".o",
                                       *pathPrint) +
                        " ";
                }
            }
        }
    }

    if (targetType != TargetType::STATIC)
    {
        for (auto &i : libraryDependencies)
        {
            if (i.preBuilt)
            {
                if (linker.bTFamily == BTFamily::MSVC)
                {
                    auto b = lcpSettings.libraryDependencies;
                    linkOrArchiveCommand += i.path + " ";
                    linkOrArchiveCommandPrintFirstHalf += getReducedPath(i.path + " ", b);
                }
                else
                {
                    string dir = path(i.path).parent_path().string();
                    string libName = path(i.path).filename().string();
                    libName.erase(0, 3);
                    libName.erase(libName.find('.'), 2);
                    linkOrArchiveCommand += getLinkFlag(dir, libName);
                    linkOrArchiveCommandPrintFirstHalf +=
                        getLinkFlagPrint(dir, libName, lcpSettings.libraryDependencies);
                }
            }
        }

        auto getLibraryDirectoryFlag = [this]() {
            if (compiler.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            else
            {
                return "-L";
            }
        };

        for (const auto &i : libraryDirectories)
        {
            linkOrArchiveCommand += getLibraryDirectoryFlag() + addQuotes(i) + " ";
            if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
            {
                linkOrArchiveCommandPrintFirstHalf +=
                    getLibraryDirectoryFlag() + getReducedPath(i, lcpSettings.libraryDirectories) + " ";
            }
        }

        for (const auto &i : environment.libraryDirectories)
        {
            linkOrArchiveCommand += getLibraryDirectoryFlag() + addQuotes(i.directoryPath.generic_string()) + " ";
            if (lcpSettings.environmentLibraryDirectories.printLevel != PathPrintLevel::NO)
            {
                linkOrArchiveCommandPrintFirstHalf +=
                    getLibraryDirectoryFlag() +
                    getReducedPath(i.directoryPath.generic_string(), lcpSettings.environmentLibraryDirectories) + " ";
            }
        }

        linkOrArchiveCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        }

        linkOrArchiveCommand += addQuotes((path(outputDirectory) / actualOutputName).string());
        if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf +=
                getReducedPath((path(outputDirectory) / actualOutputName).string(), lcpSettings.binary);
        }
    }
}

string &ParsedTarget::getLinkOrArchiveCommand()
{
    if (linkOrArchiveCommand.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }
    return linkOrArchiveCommand;
}

string &ParsedTarget::getLinkOrArchiveCommandPrintFirstHalf()
{
    if (linkOrArchiveCommandPrintFirstHalf.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }

    return linkOrArchiveCommandPrintFirstHalf;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

void from_json(const Json &j, Node *node)
{
    node = Node::getNodeFromString(j);
}

bool IndexInTopologicalSortComparator::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return lhs->indexInTopologicalSort < rhs->indexInTopologicalSort;
}

CachedFile::CachedFile(const string &filePath) : node{Node::getNodeFromString(filePath)}
{
}

SourceNode::SourceNode(const string &filePath, ReportResultTo *reportResultTo_, const ResultType resultType_)
    : CachedFile(filePath), BTarget(reportResultTo_, resultType_)
{
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j[JConsts::srcFile] = sourceNode.node;
    j[JConsts::headerDependencies] = sourceNode.headerDependencies;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node->filePath < rhs.node->filePath;
}

SourceNode &ParsedTarget::addNodeInSourceFileDependencies(const string &str)
{
    auto [pos, ok] = sourceFileDependencies.emplace(str, this, ResultType::SOURCENODE);
    auto &sourceNode = const_cast<SourceNode &>(*pos);
    sourceNode.presentInSource = true;
    return sourceNode;
}

SMFile &ParsedTarget::addNodeInModuleSourceFileDependencies(const std::string &str, bool angle)
{
    auto [pos, ok] = moduleSourceFileDependencies.emplace(str, this);
    auto &smFile = const_cast<SMFile &>(*pos);
    smFile.presentInSource = true;
    return smFile;
}

PostBasic::PostBasic(const BuildTool &buildTool, const string &commandFirstHalf, string printCommandFirstHalf,
                     const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint,
                     bool isTarget_)
    : isTarget{isTarget_}
{
    string str = isTarget ? "_t" : "";

    string outputFileName = buildCacheFilesDirPath + fileName + "_output" + str;
    string errorFileName = buildCacheFilesDirPath + fileName + "_error" + str;

    if (pathPrint.printLevel != PathPrintLevel::NO)
    {
        printCommandFirstHalf +=
            "> " + getReducedPath(outputFileName, pathPrint) + " 2>" + getReducedPath(errorFileName, pathPrint);
    }

    printCommand = std::move(printCommandFirstHalf);

#ifdef _WIN32
    path responseFile = path(buildCacheFilesDirPath + fileName + ".response");
    ofstream(responseFile) << commandFirstHalf;

    path toolPath = buildTool.bTPath;
    string cmdCharLimitMitigateCommand = addQuotes(toolPath.make_preferred().string()) + " @" +
                                         addQuotes(responseFile.generic_string()) + "> " + addQuotes(outputFileName) +
                                         " 2>" + addQuotes(errorFileName);
    bool success = system(addQuotes(cmdCharLimitMitigateCommand).c_str());
#else
    string finalCompileCommand = buildTool.bTPath.generic_string() + " " + commandFirstHalf + "> " +
                                 addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);
    bool success = system(finalCompileCommand.c_str());
#endif
    if (success == EXIT_SUCCESS)
    {
        successfullyCompleted = true;
        commandSuccessOutput = file_to_string(outputFileName);
    }
    else
    {
        successfullyCompleted = false;
        commandSuccessOutput = file_to_string(outputFileName);
        commandErrorOutput = file_to_string(errorFileName);
    }
}

void PostBasic::executePrintRoutine(uint32_t color, bool printOnlyOnError) const
{
    if (!printOnlyOnError)
    {
        print(fg(static_cast<fmt::color>(color)), pes, printCommand + " " + getThreadId() + "\n");
        if (successfullyCompleted)
        {
            if (!commandSuccessOutput.empty())
            {
                print(fg(static_cast<fmt::color>(color)), pes, commandSuccessOutput + "\n");
            }
            if (!commandErrorOutput.empty())
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                      commandErrorOutput + "\n");
            }
        }
    }
    if (!successfullyCompleted)
    {
        if (!commandSuccessOutput.empty())
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes, commandSuccessOutput + "\n");
        }
        if (!commandErrorOutput.empty())
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                  commandErrorOutput + "\n");
        }
    }
}

PostCompile::PostCompile(const ParsedTarget &parsedTarget_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint)
    : parsedTarget{const_cast<ParsedTarget &>(parsedTarget_)},
      PostBasic(buildTool, commandFirstHalf, std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
                pathPrint, false)
{
}

bool PostCompile::checkIfFileIsInEnvironmentIncludes(const string &str)
{
    //  Premature Optimization Haha
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the directories coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdirectory of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdirectory) with any of normal includes
    // or vice-versa.

    for (const auto &d : parsedTarget.environment.includeDirectories)
    {
        if (equivalent(d.directoryPath, path(str).parent_path()))
        {
            return true;
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode)
{
    vector<string> outputLines = split(commandSuccessOutput, "\n");
    string includeFileNote = "Note: including file:";

    for (auto iter = outputLines.begin(); iter != outputLines.end();)
    {
        if (iter->contains(includeFileNote))
        {
            unsigned long pos = iter->find_first_not_of(includeFileNote);
            pos = iter->find_first_not_of(" ", pos);
            iter->erase(iter->begin(), iter->begin() + (int)pos);
            if (!checkIfFileIsInEnvironmentIncludes(*iter))
            {
                Node *node = Node::getNodeFromString(*iter);
                sourceNode.headerDependencies.emplace(node);
            }
            if (settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
            {
                iter = outputLines.erase(iter);
            }
        }
        else if (*iter == sourceNode.node->fileName)
        {
            if (settings.ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput)
            {
                iter = outputLines.erase(iter);
            }
        }
        else
        {
            ++iter;
        }
    }

    string treatedOutput; // Output With All information of include files removed.
    for (const auto &i : outputLines)
    {
        treatedOutput += i;
    }
    commandSuccessOutput = treatedOutput;
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode)
{
    string headerFileContents = file_to_string(parsedTarget.buildCacheFilesDirPath + sourceNode.node->fileName + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file. While last line is
    // an empty line
    for (auto iter = headerDeps.begin() + 2; iter != headerDeps.end() - 1; ++iter)
    {
        unsigned long pos = iter->find_first_not_of(" ");
        string headerDep = iter->substr(pos, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos);
        if (!checkIfFileIsInEnvironmentIncludes(headerDep))
        {
            Node *node = Node::getNodeFromString(headerDep);
            sourceNode.headerDependencies.emplace(node);
        }
    }
}

void PostCompile::executePostCompileRoutineWithoutMutex(SourceNode &sourceNode)
{
    if (successfullyCompleted)
    {
        // Clearing old header-deps and adding the new ones.
        sourceNode.headerDependencies.clear();
    }
    if (parsedTarget.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode);
    }
    else if (parsedTarget.compiler.bTFamily == BTFamily::GCC && successfullyCompleted)
    {
        parseDepsFromGCCDepsOutput(sourceNode);
    }
}

bool LogicalNameComparator::operator()(const struct SMRuleRequires &lhs, const struct SMRuleRequires &rhs) const
{
    return lhs.logicalName < rhs.logicalName;
}

bool HeaderUnitComparator::operator()(const struct SMRuleRequires &lhs, const struct SMRuleRequires &rhs) const
{
    return tie(lhs.sourcePath, lhs.logicalName, lhs.angle) < tie(rhs.sourcePath, rhs.logicalName, rhs.angle);
}

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, const string &logicalName_)
    : angle{angle_}, logicalName{logicalName_}
{
}

bool operator<(const HeaderUnitConsumer &lhs, const HeaderUnitConsumer &rhs)
{
    return std::tie(lhs.angle, lhs.logicalName) < std::tie(rhs.angle, rhs.logicalName);
}

bool SMRuleRequires::populateFromJson(const Json &j, const string &smFilePath, const string &provideLogicalName)
{
    logicalName = j.at("logical-name").get<string>();
    if (logicalName == provideLogicalName)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Module {} can not depend on itself.\n", smFilePath);
        exit(EXIT_SUCCESS);
    }
    if (j.contains("lookup-method"))
    {
        type = SM_REQUIRE_TYPE::HEADER_UNIT;
        j.at("lookup-method").get<string>() == "include-angle" ? angle = true : angle = false;
        sourcePath = j.at("source-path").get<string>();
    }
    else
    {
        type = logicalName.contains(':') ? SM_REQUIRE_TYPE::PARTITION_EXPORT : SM_REQUIRE_TYPE::PRIMARY_EXPORT;
    }

    // TODO: The warning/error messages should point out to smfile path and smfile
    // source.
    bool ignore = false;
    if (type == SM_REQUIRE_TYPE::HEADER_UNIT)
    {
        auto [pos, ok] = setHeaderUnits->emplace(*this);
        if (!ok)
        {
            // This can happen where there can be 2 ditto entries of header-units for
            // msvc. build-system ignores it
            /* print(stderr,
               fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), "In
               SMFile: \"{}\", 2 requires have same \"source-path\" {} and
               \"logical-name\" {} and "
                   "\"lookup-method\" {}.\n",
                   smFilePath, sourcePath, logicalName, angle ? "angle" : "quote");*/
            ignore = true;
        }
    }
    else
    {
        auto [pos, ok] = setLogicalName->emplace(*this);
        if (!ok)
        {
            // Maybe ignore this too. For now build system enforces this not to be
            // true
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In SMFile: \"{}\", 2 requires have same \"logical-name\" {}.\n", smFilePath, logicalName);
            exit(EXIT_FAILURE);
        }
    }
    return ignore;
}

SMFile::SMFile(const string &srcPath, ParsedTarget *parsedTarget_)
    : SourceNode(srcPath, parsedTarget_, ResultType::CPP_SMFILE), parsedTarget{*parsedTarget_}
{
}

void SMFile::populateFromJson(const Json &j, struct Builder &builder)
{
    bool hasRequires = false;
    Json rule = j.at("rules").get<Json>()[0];
    if (rule.contains("provides"))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
    }

    set<SMRuleRequires, LogicalNameComparator> setLogicalName;
    set<SMRuleRequires, HeaderUnitComparator> setHeaderUnit;
    bool allRequireDependenciesAreHeaderUnits = true;
    if (auto it = rule.find("requires"); it != rule.end() && !it->empty())
    {
        hasRequires = true;
        SMRuleRequires::setLogicalName = &setLogicalName;
        SMRuleRequires::setHeaderUnits = &setHeaderUnit;
        vector<Json> requireJsons = rule.at("requires").get<vector<Json>>();
        if (requireJsons.empty())
        {
            hasRequires = false;
        }
        requireDependencies.resize(requireJsons.size());
        for (unsigned long i = 0; i < requireJsons.size(); ++i)
        {
            bool ignore = requireDependencies[i].populateFromJson(requireJsons[i], node->filePath, logicalName);
            if (ignore)
            {
                // TODO:
                //  Just ignoring this file but not removing it from the
                //  requireDependencies.
                continue;
            }
            if (requireDependencies[i].type == SM_REQUIRE_TYPE::HEADER_UNIT)
            {
                string headerUnitPath = path(requireDependencies[i].sourcePath).lexically_normal().generic_string();
                const auto &[pos1, Ok1] = builder.headerUnits.emplace(headerUnitPath, &parsedTarget);
                auto &smFile = const_cast<SMFile &>(*pos1);
                if (Ok1)
                {
                    // TODO
                    //  This should not fail because an SMFile cannot be both, a headerUnit and an SMFile
                    builder.smFiles.emplace(&smFile);
                    smFile.type = SM_FILE_TYPE::HEADER_UNIT;
                    headerUnitsConsumptionMethods[&smFile].emplace(requireDependencies[i].angle,
                                                                   requireDependencies[i].logicalName);
                    for (auto &dir : parsedTarget.environment.includeDirectories)
                    {
                        if (headerUnitPath.starts_with(dir.directoryPath.generic_string()))
                        {
                            smFile.standardHeaderUnit = true;
                            break;
                        }
                    }
                }
                else
                {
                    // Will reach here whenever a header-unit is added in source which was
                    // also present in cache.
                    static_assert(true);
                }

                addDependency(smFile);
                // TODO:
                // Check for update
                smFile.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            else
            {
                allRequireDependenciesAreHeaderUnits = false;
                if (requireDependencies[i].type == SM_REQUIRE_TYPE::PARTITION_EXPORT)
                {
                    type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
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
        if (allRequireDependenciesAreHeaderUnits)
        {
            type = SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT;
        }
        else
        {
            if (type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
            {
                type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
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
    string ifcFilePath = addQuotes(parsedTarget.buildCacheFilesDirPath + node->fileName + ".ifc");
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
               "SMFile referencing a headerUnit for which there is not consumption methon");
        string str;
        for (const auto &it : dependentSMFile.headerUnitsConsumptionMethods)
        {
            for (const HeaderUnitConsumer &headerUnitConsumer : it.second)
            {
                string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:";
                str += angleStr + " ";
                str += headerUnitConsumer.logicalName + "=" + ifcFilePath;
            }
        }
        return str;
    }
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    string ifcFilePath = parsedTarget.buildCacheFilesDirPath + node->fileName + ".ifc";
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
               "SMFile referencing a headerUnit for which there is not consumption methon");
        string str;
        for (const auto &it : dependentSMFile.headerUnitsConsumptionMethods)
        {
            for (const HeaderUnitConsumer &headerUnitConsumer : it.second)
            {
                string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
                if (!ccpSettings.infrastructureFlags)
                {
                    str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
                }
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

    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? parsedTarget.getInfrastructureFlags() : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf += getFlagPrint(parsedTarget.buildCacheFilesDirPath + node->fileName);
    return moduleCompileCommandPrintLastHalf;
}

BTarget::BTarget(ReportResultTo *reportResultTo_, const ResultType resultType_)
    : reportResultTo{reportResultTo_}, resultType{resultType_}
{
    id = total++;
}

void BTarget::addDependency(BTarget &dependency)
{
    dependencies.emplace(&dependency);
    dependency.dependents.emplace(this);
    ++dependenciesSize;
    if (!tarjanNode)
    {
        tarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(this).first.operator->());
    }
    if (!dependency.tarjanNode)
    {
        dependency.tarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(&dependency).first.operator->());
    }
    tarjanNode->deps.emplace_back(dependency.tarjanNode);
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

SMFileVariantPathAndLogicalName::SMFileVariantPathAndLogicalName(const string &logicalName_,
                                                                 const string &variantFilePath_)
    : logicalName(logicalName_), variantFilePath(variantFilePath_)
{
}

bool SMFilePointerComparator::operator()(const SMFile *lhs, const SMFile *rhs) const
{
    return tie(lhs->node->filePath, lhs->parsedTarget.variantFilePath) <
           tie(rhs->node->filePath, rhs->parsedTarget.variantFilePath);
}

bool SMFilePathAndVariantPathComparator::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    return tie(lhs.node->filePath, lhs.parsedTarget.variantFilePath) <
           tie(rhs.node->filePath, rhs.parsedTarget.variantFilePath);
}

bool SMFileCompareLogicalName::operator()(const SMFileVariantPathAndLogicalName lhs,
                                          const SMFileVariantPathAndLogicalName rhs) const
{
    return (&(lhs.variantFilePath) < &(rhs.variantFilePath)) || (lhs.logicalName < rhs.logicalName);
}

void Node::checkIfNotUpdatedAndUpdate()
{
    if (!isUpdated)
    {
        lastUpdateTime = last_write_time(path(filePath));
        isUpdated = true;
    }
}

bool std::less<Node *>::operator()(const Node *lhs, const Node *rhs) const
{
    return lhs->filePath < rhs->filePath;
}

Node::Node(string filePath_) : filePath(std::move(filePath_)), fileName(path(filePath).filename().string())
{
}

Node *Node::getNodeFromString(const string &str)
{
    Node *tempNode = new Node{str};

    if (auto [pos, ok] = allFiles.emplace(tempNode); !ok)
    {
        // Means it already exists
        delete tempNode;
        return *pos;
    }
    else
    {
        return tempNode;
    }
}

void ParsedTarget::setSourceNodeFileStatus(SourceNode &sourceNode, bool angle = false) const
{
    sourceNode.fileStatus = FileStatus::UPDATED;

    path objectFilePath = path(buildCacheFilesDirPath + sourceNode.node->fileName + ".o");

    if (!std::filesystem::exists(objectFilePath))
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    sourceNode.node->checkIfNotUpdatedAndUpdate();
    if (sourceNode.node->lastUpdateTime > objectFileLastEditTime)
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {
        for (auto &d : sourceNode.headerDependencies)
        {
            d->checkIfNotUpdatedAndUpdate();
            if (d->lastUpdateTime > objectFileLastEditTime)
            {
                sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
                break;
            }
        }
    }
}

void ParsedTarget::checkForRelinkPrebuiltDependencies()
{
    path outputPath = path(outputDirectory) / actualOutputName;
    if (!std::filesystem::exists(outputPath))
    {
        fileStatus = FileStatus::NEEDS_UPDATE;
    }
    for (const auto &i : libraryDependencies)
    {
        if (i.preBuilt)
        {
            if (!std::filesystem::exists(path(i.path)))
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "Prebuilt Library {} Does Not Exist.\n", i.path);
                exit(EXIT_FAILURE);
            }
            if (fileStatus != FileStatus::NEEDS_UPDATE)
            {
                if (last_write_time(i.path) > last_write_time(outputPath))
                {
                    fileStatus = FileStatus::NEEDS_UPDATE;
                }
            }
        }
    }
}

void ParsedTarget::checkForPreBuiltAndCacheDir()
{
    checkForRelinkPrebuiltDependencies();
    if (!std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
    }

    setCompileCommand();
    if (std::filesystem::exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        string str = targetCacheJson.at(JConsts::compileCommand).get<string>();
        linkCommand = targetCacheJson.at(JConsts::linkCommand).get<string>();
        auto initializeSourceNodePointer = [](SourceNode *sourceNode, const Json &j) {
            for (const Json &headerFile : j.at(JConsts::headerDependencies))
            {
                sourceNode->headerDependencies.emplace(Node::getNodeFromString(headerFile));
            }
            sourceNode->presentInCache = true;
        };
        if (str == compileCommand)
        {
            for (const Json &j : targetCacheJson.at(JConsts::sourceDependencies))
            {
                initializeSourceNodePointer(
                    const_cast<SourceNode *>(
                        sourceFileDependencies.emplace(j.at(JConsts::srcFile), this, ResultType::SOURCENODE)
                            .first.
                            operator->()),
                    j);
            }
            for (const Json &j : targetCacheJson.at(JConsts::moduleDependencies))
            {
                initializeSourceNodePointer(
                    const_cast<SMFile *>(
                        moduleSourceFileDependencies.emplace(j.at(JConsts::srcFile), this).first.operator->()),
                    j);
            }
        }
    }
}

void ParsedTarget::populateSetTarjanNodesSourceNodes(Builder &builder)
{
    for (const auto &sourceFile : sourceFiles)
    {
        SourceNode &sourceNode = addNodeInSourceFileDependencies(sourceFile);
        if (sourceNode.presentInSource != sourceNode.presentInCache)
        {
            sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            setSourceNodeFileStatus(sourceNode);
        }
        if (sourceNode.fileStatus == FileStatus::NEEDS_UPDATE)
        {
            addDependency(sourceNode);
        }
    }

    /*    // Removing unused object-files from buildCacheFilesDirPath as they later
        // affect linker command with *.o option.
        set<string> tmpObjectFiles;
        for (const auto &src : sourceFileDependencies)
        {
            if (src.presentInSource == src.presentInCache)
            {
                tmpObjectFiles.emplace(src.node->fileName + ".o");
            }
        }

        for (const auto &j : directory_iterator(path(buildCacheFilesDirPath)))
        {
            if (!j.path().filename().string().ends_with(".o"))
            {
                continue;
            }
            if (!tmpObjectFiles.contains(j.path().filename().string()))
            {
                std::filesystem::remove(j.path());
            }
        }*/
}

void ParsedTarget::populateRequirePathsAndSMFilesWithHeaderUnitsMap(
    Builder &builder, map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (const auto &sourceFile : modulesSourceFiles)
    {
        SMFile &smFile = addNodeInModuleSourceFileDependencies(sourceFile, false);

        string smFilePath = buildCacheFilesDirPath + smFile.node->fileName + ".smrules";
        Json smFileJson;
        ifstream(smFilePath) >> smFileJson;

        smFile.populateFromJson(smFileJson, builder);

        if (smFile.hasProvide)
        {
            if (auto [pos, ok] = requirePaths.emplace(
                    SMFileVariantPathAndLogicalName(smFile.logicalName, *variantFilePath), &smFile);
                !ok)
            {
                const auto &[key, val] = *pos;
                // TODO
                // Mention the module scope too.
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "Module {} Is Being Provided By 2 different files:\n1){}\n2){}\n", smFile.logicalName,
                      smFile.node->filePath, val->node->filePath);
                exit(EXIT_FAILURE);
            }
        }
    }
}

template <typename T, typename U = std::less<T>> std::pair<T *, bool> insert(set<T *, U> &containerSet, T *element)
{
    auto [pos, Ok] = containerSet.emplace(element);
    return make_pair(const_cast<T *>(*pos), Ok);
}

template <typename T, typename U = std::less<T>, typename... V>
std::pair<T *, bool> insert(set<T, U> &containerSet, V &&...elements)
{
    auto [pos, Ok] = containerSet.emplace(std::forward<V>(elements)...);
    return make_pair(const_cast<T *>(&(*pos)), Ok);
}

void ParsedTarget::populateParsedTargetModuleSourceFileDependenciesAndBuilderSMFiles(Builder &builder)
{
    for (const auto &sourceFile : modulesSourceFiles)
    {
        SMFile &smFile = addNodeInModuleSourceFileDependencies(sourceFile, false);
        if (auto [pos, Ok] = builder.smFiles.emplace(&smFile); Ok)
        {
            if (smFile.presentInSource != smFile.presentInCache)
            {
                smFile.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            else
            {
                // TODO: Audit the code
                //  This will check if the smfile of the module source file is outdated.
                //  If it is then scan-dependencies operation is performed on the module
                //  source.
                setSourceNodeFileStatus(smFile);
            }
            if (smFile.fileStatus == FileStatus::NEEDS_UPDATE)
            {
                smFile.resultType = ResultType::CPP_SMFILE;
                builder.finalBTargets.emplace_back(&smFile);
            }
        }
        else
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "Module Source {} is provided by this target {}. But, is also "
                  "provided by some other target in the "
                  "same variant",
                  sourceFile, targetFilePath);
        }
    }
}

string ParsedTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c /Tp";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it
        // prints 2 header deps in one line and no space in them so no way of
        // knowing whether this is a space in path or 2 different headers. Which
        // then breaks when last_write_time is checked for that path.
        return "-c -MMD";
    }
    return "";
}

string ParsedTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    string command;
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags() + " ";
    }
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? "/Fo" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command +=
            getReducedPath(buildCacheFilesDirPath + sourceNode.node->fileName + ".o", ccpSettings.objectFile) + " ";
    }
    return command;
}

PostCompile ParsedTarget::CompileSMFile(SMFile *smFile)
{
    // TODO: A temporary hack to support modules
    string finalCompileCommand = compileCommand + "  /std:c++latest /translateInclude  ";

    for (const BTarget *bTarget : smFile->allDependencies)
    {
        if (bTarget->resultType == ResultType::CPP_MODULE)
        {
            const auto *smFileDependency = static_cast<const SMFile *>(bTarget);
            finalCompileCommand += smFileDependency->getRequireFlag(*smFile) + " ";
        }
    }
    finalCompileCommand += getInfrastructureFlags() + addQuotes(smFile->node->filePath) + " ";

    // TODO:
    //  getFlag and getRequireFlags create confusion. Instead of getRequireFlags, only getFlag should be used.
    finalCompileCommand += smFile->getFlag(buildCacheFilesDirPath + smFile->node->fileName);

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile->getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       smFile->node->fileName,
                       settings.ccpSettings.outputAndErrorFiles};
}

PostCompile ParsedTarget::Compile(SourceNode &sourceNode)
{
    string compileFileName = sourceNode.node->fileName;

    string finalCompileCommand = compileCommand + " ";

    finalCompileCommand += getInfrastructureFlags() + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += "/Fo" + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }
    else
    {
        finalCompileCommand += "-o " + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                       buildCacheFilesDirPath,
                       compileFileName,
                       settings.ccpSettings.outputAndErrorFiles};
}

PostBasic ParsedTarget::Archive()
{
    return PostBasic(archiver, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(),
                     buildCacheFilesDirPath, targetName, settings.acpSettings.outputAndErrorFiles, true);
}

PostBasic ParsedTarget::Link()
{
    return PostBasic(linker, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(), buildCacheFilesDirPath,
                     targetName, settings.lcpSettings.outputAndErrorFiles, true);
}

PostBasic ParsedTarget::GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError)
{
    string finalCompileCommand = getCompileCommand() + addQuotes(sourceNode.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        // TODO:
        //  Temporary hack for modules. /translateInclude should be provided as settings in hhelper.
        finalCompileCommand += " /std:c++latest /translateInclude /scanDependencies " +
                               addQuotes(buildCacheFilesDirPath + sourceNode.node->fileName + ".smrules") + " ";
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Modules supported only on MSVC");
    }

    return printOnlyOnError
               ? PostBasic(compiler, finalCompileCommand, "", buildCacheFilesDirPath, sourceNode.node->fileName,
                           settings.ccpSettings.outputAndErrorFiles, true)
               : PostBasic(compiler, finalCompileCommand,
                           getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                           buildCacheFilesDirPath, sourceNode.node->fileName + ".smrules",
                           settings.ccpSettings.outputAndErrorFiles, true);
}

TargetType ParsedTarget::getTargetType() const
{
    return targetType;
}

void ParsedTarget::pruneAndSaveBuildCache(const bool successful)
{
    // TODO:
    // Not dealing with module source
    for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
    {
        !it->presentInSource ? it = sourceFileDependencies.erase(it) : ++it;
    }
    if (!successful)
    {
        for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
        {
            it->fileStatus == FileStatus::NEEDS_UPDATE ? it = sourceFileDependencies.erase(it) : ++it;
        }
    }
    Json cacheFileJson;
    cacheFileJson[JConsts::compileCommand] = compileCommand;
    cacheFileJson[JConsts::linkCommand] = linkCommand;
    cacheFileJson[JConsts::sourceDependencies] = sourceFileDependencies;
    cacheFileJson[JConsts::moduleDependencies] = moduleSourceFileDependencies;
    ofstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) << cacheFileJson.dump(4);
}

void ParsedTarget::copyParsedTarget() const
{
    if (packageMode && copyPackage)
    {
        if (settings.gpcSettings.copyingPackage)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)), "Copying Package.\n");
        }
        string copyFrom;
        if (targetType != TargetType::SHARED)
        {
            copyFrom = outputDirectory + actualOutputName;
        }
        else
        {
            // Shared Libraries Not Supported Yet.
        }
        if (settings.gpcSettings.copyingTarget)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)),
                  "Copying Target {} From {} To {}.\n", targetName, copyFrom, packageTargetPath);
        }
        create_directories(path(packageTargetPath));
        copy(path(copyFrom), path(packageTargetPath), copy_options::update_existing);
        for (auto &i : includeDirectories)
        {
            if (i.copy)
            {
                string includeDirectoryCopyFrom = i.path;
                string includeDirectoryCopyTo = packageTargetPath + "include/";
                if (settings.gpcSettings.copyingTarget)
                {
                    print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
                          "Copying IncludeDirectory From {} To {}.\n", targetName, includeDirectoryCopyFrom,
                          includeDirectoryCopyTo);
                }
                create_directories(path(includeDirectoryCopyTo));
                copy(path(includeDirectoryCopyFrom), path(includeDirectoryCopyTo),
                     copy_options::update_existing | copy_options::recursive);
            }
        }
        string consumerTargetFileName;
        string consumerTargetFile = packageTargetPath + (targetName + ".hmake");
        ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);

        for (const auto &target : libraryDependencies)
        {
            if (target.preBuilt && !target.imported) // A prebuilt target is being packaged.
            {
                Json preBuiltTarget;
                ifstream(target.hmakeFilePath) >> preBuiltTarget;
                string preBuiltTargetName = preBuiltTarget[JConsts::name];
                copyFrom = preBuiltTarget[JConsts::path];
                string preBuiltPackageCopyPath = preBuiltTarget[JConsts::packageCopyPath];
                string targetInstallPath = packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" +
                                           preBuiltTargetName + "/";
                create_directories(path(targetInstallPath));
                copy(path(copyFrom), path(targetInstallPath), copy_options::update_existing);
                ofstream(targetInstallPath + preBuiltTargetName + ".hmake")
                    << preBuiltTarget[JConsts::consumerDependencies].dump(4);
            }
        }
    }
}

void ParsedTarget::execute(unsigned long fileTargetId)
{
}

void ParsedTarget::executePrintRoutine(unsigned long fileTargetId)
{
}

bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs)
{
    return lhs.targetFilePath < rhs.targetFilePath;
}

SystemHeaderUnit::SystemHeaderUnit(ParsedTarget &parsedTarget_, const string &filePath_)
    : parsedTarget{&parsedTarget_}, filePath{filePath_}
{
}

bool operator<(const SystemHeaderUnit &lhs, const SystemHeaderUnit &rhs)
{
    return std::tie(lhs.parsedTarget->variantFilePath, lhs.filePath) <
           std::tie(rhs.parsedTarget->variantFilePath, rhs.filePath);
}

ApplicationHeaderUnit::ApplicationHeaderUnit(ParsedTarget &parsedTarget_, const string &cachedFilePath_)
    : parsedTarget{&parsedTarget_}, CachedFile{cachedFilePath_}
{
}

bool operator<(const ApplicationHeaderUnit &lhs, const ApplicationHeaderUnit &rhs)
{
    return std::tie(lhs.parsedTarget->variantFilePath, lhs.node->filePath) <
           std::tie(rhs.parsedTarget->variantFilePath, rhs.node->filePath);
}

Builder::Builder(const set<string> &targetFilePaths)
{
    populateParsedTargetsSetAndSetTarjanNodesParsedTargets(targetFilePaths);
    map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> requirePaths;
    for (const ParsedTarget &parsedTarget_ : parsedTargetSet)
    {
        auto &parsedTarget = const_cast<ParsedTarget &>(parsedTarget_);
        parsedTarget.checkForPreBuiltAndCacheDir();
        parsedTarget.populateParsedTargetModuleSourceFileDependenciesAndBuilderSMFiles(*this);
    }
    finalBTargetsSizeGoal = finalBTargets.size();
    launchThreadsAndUpdateBTargets();
    checkForHeaderUnitsCache();
    createHeaderUnits();
    for (const ParsedTarget &parsedTarget1 : parsedTargetSet)
    {
        auto &parsedTarget = const_cast<ParsedTarget &>(parsedTarget1);
        parsedTarget.populateRequirePathsAndSMFilesWithHeaderUnitsMap(*this, requirePaths);
        parsedTarget.populateSetTarjanNodesSourceNodes(*this);
    }
    populateSetTarjanNodesBTargetsSMFiles(requirePaths);
    populateFinalBTargets();
    launchThreadsAndUpdateBTargets();
}

void Builder::populateParsedTargetsSetAndSetTarjanNodesParsedTargets(const set<string> &targetFilePaths)
{
    set<string> parsedVariantsForModules;
    // parsedTargetStack will only contain pointers to parsedTargetSet elements
    // for which libsParsed is false.
    stack<ParsedTarget *> parsedTargetsStack;

    auto checkVariantOfTargetForModules = [&](const string &variantFilePath) {
        if (!parsedVariantsForModules.contains(variantFilePath))
        {
            parsedVariantsForModules.emplace(variantFilePath);
            Json variantJson;
            ifstream(variantFilePath) >> variantJson;
            for (const string &targetFilePath : variantJson.at(JConsts::targetsWithModules).get<vector<string>>())
            {
                // We push on Stack so Regular Target Dependencies of this Module Target
                // are also added
                parsedTargetsStack.emplace(
                    const_cast<ParsedTarget *>(parsedTargetSet.emplace(targetFilePath).first.operator->()));
            }
        }
    };

    for (const string &targetFilePath : targetFilePaths)
    {
        auto [ptr, Ok] = parsedTargetSet.emplace(targetFilePath);
        if (Ok)
        {
            // TODO: Some modifications needed with module scope. Most probably module
            // scope will itself have a list of targets.
            checkVariantOfTargetForModules(*(ptr->variantFilePath));
            // The pointer may have already been pushed in
            // checkVariantOfTargetForModules but may have not.
            parsedTargetsStack.emplace(const_cast<ParsedTarget *>(ptr.operator->()));
        }
    }

    while (!parsedTargetsStack.empty())
    {
        ParsedTarget *ptr = parsedTargetsStack.top();
        parsedTargetsStack.pop();
        if (!ptr->libsParsed)
        {
            ptr->libsParsed = true;
            if (!ptr->tarjanNode)
            {
                ptr->tarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(ptr).first.operator->());
            }
            for (auto &dep : ptr->libraryDependencies)
            {
                if (dep.preBuilt)
                {
                    continue;
                }
                ParsedTarget *ptrDep = const_cast<ParsedTarget *>(parsedTargetSet.emplace(dep.path).first.operator->());
                if (ptr == ptrDep)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Library {} can not depend on itself.\n", ptr->targetFilePath);
                    exit(EXIT_SUCCESS);
                }

                if (!ptrDep->tarjanNode)
                {
                    ptrDep->tarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(ptrDep).first.operator->());
                }
                ptr->tarjanNode->deps.emplace_back(ptrDep->tarjanNode);
                ptr->dependencies.emplace(ptrDep);
                ptrDep->dependents.emplace(ptr);
                /*                if (ptrDep->isModule)
                                {
                                    print(stderr,
                   fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                                          "{}  Has Dependency {} Which Has Module
                   Source. A Target With Module Source Cannot " "Be A Dependency Of
                   Other Target.\n", ptr->targetFilePath, ptrDep->targetFilePath);
                                }*/
                if (!ptrDep->libsParsed)
                {
                    parsedTargetsStack.emplace(ptrDep);
                }
            }
        }
    }
}

void Builder::checkForHeaderUnitsCache()
{
    // TODO:
    //  Currently using parsedTargetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    // TODO:
    // Should be iterating over headerUnits scope instead.
    for (const ParsedTarget &parsedTarget1 : parsedTargetSet)
    {
        const auto [pos, Ok] = addressed.emplace(parsedTarget1.variantFilePath);
        if (!Ok)
        {
            continue;
        }
        auto &parsedTarget = const_cast<ParsedTarget &>(parsedTarget1);
        // SHU system header units     AHU application header units
        path shuCachePath = path(**pos).parent_path() / "HMake_SHU/headerUnits.cache";
        path ahuCachePath = path(**pos).parent_path() / "HMake_AHU/headerUnits.cache";
        if (exists(shuCachePath))
        {
            Json shuCacheJson;
            ifstream(shuCachePath) >> shuCacheJson;
            for (auto &shu : shuCacheJson)
            {
                parsedTarget.moduleSourceFileDependencies.emplace(shu, &parsedTarget);
            }
        }
        else
        {
            create_directory(shuCachePath.parent_path());
        }
        if (exists(ahuCachePath))
        {
            Json ahuCacheJson;
            ifstream(shuCachePath) >> ahuCacheJson;
            for (auto &ahu : ahuCacheJson)
            {
                const auto &[pos1, Ok1] = parsedTarget.moduleSourceFileDependencies.emplace(ahu, &parsedTarget);
                const_cast<SMFile &>(*pos1).presentInCache = true;
            }
        }
        else
        {
            create_directory(ahuCachePath.parent_path());
        }
    }
}

void Builder::populateSetTarjanNodesBTargetsSMFiles(
    const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (SMFile *smFileConst : smFiles)
    {
        auto &smFile = const_cast<SMFile &>(*smFileConst);

        smFile.parsedTarget.addDependency(smFile);
        for (auto &dep : smFile.requireDependencies)
        {
            if (dep.type == SM_REQUIRE_TYPE::HEADER_UNIT)
            {
                /*                SMFile smFile1(dep.sourcePath, &(smFile.parsedTarget));
                                if (auto smFileDepIt = smFiles.find(&smFile1); smFileDepIt != smFiles.end())
                                {
                                    auto &smFileDep = const_cast<SMFile &>(**smFileDepIt);
                                    smFile.addDependency(smFileDep);
                                }
                                else
                                {
                                    // If a header-file include-path is not provided, then compiler will
                                    // give error when it is invoked with /scanDependencies option i.e. it
                                    // should never reach here.
                                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                                          "HMake Internal Error.\n");
                                    exit(EXIT_FAILURE);
                                }*/
            }
            else
            {
                if (auto it = requirePaths.find(
                        SMFileVariantPathAndLogicalName(dep.logicalName, *(smFile.parsedTarget.variantFilePath)));
                    it == requirePaths.end())
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "No File Provides This {}.\n", dep.logicalName);
                    exit(EXIT_FAILURE);
                }
                else
                {
                    SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                    smFile.addDependency(smFileDep);
                }
            }
        }
        smFile.parsedTarget.smFiles.emplace_back(&smFile);
        smFile.resultType = ResultType::CPP_MODULE;
    }
}

void Builder::populateFinalBTargets()
{
    TBT::tarjanNodes = &tarjanNodesBTargets;
    TBT::findSCCS();
    TBT::checkForCycle([](BTarget *parsedTarget) -> string { return "cycle check function not implemented"; }); // TODO
    unsigned long needsUpdate = 0;
    for (unsigned long i = 0; i < TBT::topologicalSort.size(); ++i)
    {
        BTarget *bTarget = TBT::topologicalSort[i];
        bTarget->indexInTopologicalSort = i;
        if (bTarget->resultType == ResultType::LINK)
        {
            auto &parsedTarget = static_cast<ParsedTarget &>(*bTarget);

            if (parsedTarget.fileStatus != FileStatus::NEEDS_UPDATE)
            {
                // TODO
                /*                if (parsedTarget.getLinkOrArchiveCommand() != parsedTarget.linkCommand)
                                {
                                    parsedTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                                }*/
            }
        }
        for (BTarget *dependent : bTarget->dependents)
        {
            if (bTarget->fileStatus == FileStatus::UPDATED)
            {
                --(dependent->dependenciesSize);
            }
            else if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
            {
                auto &dependentParsedTarget = static_cast<ParsedTarget &>(*dependent);
                dependentParsedTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
        }
        if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
        {
            ++needsUpdate;
            if (!bTarget->dependenciesSize)
            {
                finalBTargets.emplace_back(bTarget);
            }
        }
        for (BTarget *dependency : bTarget->dependencies)
        {
            bTarget->allDependencies.emplace(dependency);
            // Following assign all dependencies of the dependency to all dependencies of the bTarget. Because of
            // iteration in topologicalSort, indexInTopologicalSort for all dependencies would already be set.
            for (BTarget *dep : dependency->allDependencies)
            {
                bTarget->allDependencies.emplace(dep);
            }
        }
    }
    finalBTargetsSizeGoal = needsUpdate;
}

set<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at(JConsts::targets).get<set<string>>();
}

set<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec;
    if (isPackage)
    {
        vector<Json> pVariantJson = projectFileJson.at(JConsts::variants).get<vector<Json>>();
        for (const auto &i : pVariantJson)
        {
            vec.emplace_back(i.at(JConsts::index).get<string>());
        }
    }
    else
    {
        vec = projectFileJson.at(JConsts::variants).get<vector<string>>();
    }
    set<string> targetFilePaths;
    for (auto &i : vec)
    {
        for (const string &str :
             getTargetFilePathsFromVariantFile(i + (isPackage ? "/packageVariant.hmake" : "/projectVariant.hmake")))
        {
            targetFilePaths.emplace(str);
        }
    }
    return targetFilePaths;
}

mutex oneAndOnlyMutex;
mutex printMutex;

void Builder::launchThreadsAndUpdateBTargets()
{
    vector<thread *> threads;

    // TODO
    // Instead of launching all threads, only the required amount should be
    // launched. Following should be helpful for this calculation in DSL.
    // https://cs.stackexchange.com/a/16829
    unsigned short launchThreads = 1;
    if (launchThreads)
    {
        while (threads.size() != launchThreads - 1)
        {
            threads.emplace_back(new thread{&Builder::updateBTargets, this});
        }
        if (settings.maximumBuildThreads)
        {
            updateBTargets();
        }
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }
    finalBTargets.clear();
    finalBTargetsIndex = 0;
}

void Builder::updateBTargets()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        while (finalBTargetsIndex < finalBTargets.size())
        {
            BTarget *bTarget = finalBTargets[finalBTargetsIndex];
            if (bTarget->resultType == ResultType::LINK || bTarget->resultType == ResultType::CPP_MODULE)
            {
            }
            ++finalBTargetsIndex;
            oneAndOnlyMutex.unlock();
            switch (bTarget->resultType)
            {
            case ResultType::LINK: {
                auto *parsedTarget = static_cast<ParsedTarget *>(bTarget->reportResultTo);
                unique_ptr<PostBasic> postLinkOrArchive;
                if (parsedTarget->getTargetType() == TargetType::STATIC)
                {
                    PostBasic postLinkOrArchive1 = parsedTarget->Archive();
                    postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                }
                else if (parsedTarget->getTargetType() == TargetType::EXECUTABLE)
                {
                    PostBasic postLinkOrArchive1 = parsedTarget->Link();
                    postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                }

                /*                if (postLinkOrArchive->successfullyCompleted)
                                {
                                    parsedTarget->targetCache.linkCommand =
                   parsedTarget->getLinkOrArchiveCommand();
                                }
                                else
                                {
                                    parsedTarget->targetCache.linkCommand = "";
                                }*/

                parsedTarget->pruneAndSaveBuildCache(true);

                printMutex.lock();
                if (parsedTarget->getTargetType() == TargetType::STATIC)
                {
                    postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, false);
                }
                else if (parsedTarget->getTargetType() == TargetType::EXECUTABLE)
                {
                    postLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, false);
                }
                printMutex.unlock();
                if (!postLinkOrArchive->successfullyCompleted)
                {
                    exit(EXIT_FAILURE);
                }
            }
            break;
            case ResultType::SOURCENODE: {
                auto *parsedTarget = static_cast<ParsedTarget *>(bTarget->reportResultTo);
                auto *sourceNode = static_cast<SourceNode *>(bTarget);
                PostCompile postCompile = parsedTarget->Compile(*sourceNode);
                postCompile.executePostCompileRoutineWithoutMutex(*sourceNode);
                printMutex.lock();
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                printMutex.unlock();
            }
            break;
            case ResultType::CPP_SMFILE: {
                auto *smFile = static_cast<SMFile *>(bTarget);
                PostBasic postBasic = smFile->parsedTarget.GenerateSMRulesFile(*smFile, true);
                printMutex.lock();
                postBasic.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                printMutex.unlock();
                if (!postBasic.successfullyCompleted)
                {
                    exit(EXIT_FAILURE);
                }
            }
            break;
            case ResultType::CPP_MODULE: {
                auto *smFile = static_cast<SMFile *>(bTarget);
                PostCompile postCompile = smFile->parsedTarget.CompileSMFile(smFile);
                postCompile.executePostCompileRoutineWithoutMutex(*smFile);
                printMutex.lock();
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                printMutex.unlock();
                if (!postCompile.successfullyCompleted)
                {
                    smFile->parsedTarget.pruneAndSaveBuildCache(false);
                    exit(EXIT_FAILURE);
                }
                smFile->fileStatus = FileStatus::UPDATED;
            }
            break;
            case ResultType::ACTIONTARGET:
                break;
            }
            oneAndOnlyMutex.lock();
            switch (bTarget->resultType)
            {
            case ResultType::LINK: {
                auto *parsedTarget = static_cast<ParsedTarget *>(bTarget->reportResultTo);
                parsedTarget->execute(bTarget->id);
                printMutex.lock();
                parsedTarget->executePrintRoutine(bTarget->id);
                printMutex.unlock();
            }
            break;
            case ResultType::SOURCENODE:
                break;
            case ResultType::CPP_SMFILE:
                break;
            case ResultType::CPP_MODULE:
                break;
            case ResultType::ACTIONTARGET:
                break;
            }
            for (BTarget *dependent : bTarget->dependents)
            {
                --(dependent->dependenciesSize);
                if (!dependent->dependenciesSize && dependent->fileStatus == FileStatus::NEEDS_UPDATE)
                {
                    finalBTargets.emplace_back(dependent);
                }
            }
        }
        if (finalBTargetsIndex == finalBTargetsSizeGoal)
        {
            loop = false;
        }
        oneAndOnlyMutex.unlock();
    }
}

void Builder::copyPackage(const path &packageFilePath)
{
    Json packageFileJson;
    ifstream(packageFilePath) >> packageFileJson;
    Json variants = packageFileJson.at(JConsts::variants).get<Json>();
    path packageCopyPath;
    bool packageCopy = packageFileJson.at(JConsts::packageCopy).get<bool>();
    if (packageCopy)
    {
        string packageName = packageFileJson.at(JConsts::name).get<string>();
        string version = packageFileJson.at(JConsts::version).get<string>();
        packageCopyPath = packageFileJson.at(JConsts::packageCopyPath).get<string>();
        packageCopyPath /= packageName;
        if (packageFileJson.at(JConsts::cacheIncludes).get<bool>())
        {
            Json commonFileJson;
            ifstream(current_path() / "Common.hmake") >> commonFileJson;
            Json commonJson;
            for (auto &i : commonFileJson)
            {
                Json commonJsonObject;
                int commonIndex = i.at(JConsts::index).get<int>();
                commonJsonObject[JConsts::index] = commonIndex;
                commonJsonObject[JConsts::variantsIndices] = i.at(JConsts::variantsIndices).get<Json>();
                commonJson.emplace_back(commonJsonObject);
                path commonIncludePath = packageCopyPath / "Common" / path(to_string(commonIndex)) / "include";
                create_directories(commonIncludePath);
                path dirCopyFrom = i.at(JConsts::path).get<string>();
                copy(dirCopyFrom, commonIncludePath, copy_options::update_existing | copy_options::recursive);
            }
        }
        path consumePackageFilePath = packageCopyPath / "cpackage.hmake";
        Json consumePackageFilePathJson;
        consumePackageFilePathJson[JConsts::name] = packageName;
        consumePackageFilePathJson[JConsts::version] = version;
        consumePackageFilePathJson[JConsts::variants] = variants;
        create_directories(packageCopyPath);
        ofstream(consumePackageFilePath) << consumePackageFilePathJson.dump(4);
    }
}
void Builder::createHeaderUnits()
{
    // TODO:
    //  Currently using parsedTargetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    for (const ParsedTarget &parsedTarget1 : parsedTargetSet)
    {
        const auto [pos, Ok] = addressed.emplace(parsedTarget1.variantFilePath);
        if (!Ok)
        {
            continue;
        }
        auto &parsedTarget = const_cast<ParsedTarget &>(parsedTarget1);
        // SHU system header units     AHU application header units
        path shuCachePath = path(**pos) / "HMake_SHU/headerUnits.cache";
        path ahuCachePath = path(**pos) / "HMake_AHU/headerUnits.cache";
        if (exists(shuCachePath))
        {
            Json shuCacheJson;
            ifstream(shuCachePath) >> shuCacheJson;
            for (auto &shu : shuCacheJson)
            {
                parsedTarget.moduleSourceFileDependencies.emplace(shu, &parsedTarget);
            }
        }
        if (exists(ahuCachePath))
        {
            Json ahuCacheJson;
            ifstream(shuCachePath) >> ahuCacheJson;
            for (auto &ahu : ahuCacheJson)
            {
                const auto &[pos1, Ok1] = parsedTarget.moduleSourceFileDependencies.emplace(ahu, &parsedTarget);
                const_cast<SMFile &>(*pos1).presentInCache = true;
            }
        }
    }
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

    auto nthOccurrence = [](const string &str, const string &findMe, unsigned int nth) -> int {
        unsigned long count = 0;

        for (int i = 0; i < str.size(); ++i)
        {
            if (str.size() > i + findMe.size())
            {
                bool found = true;
                for (unsigned long j = 0; j < findMe.size(); ++j)
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
                        return i;
                    }
                }
            }
            else
            {
                break;
            }
        }
        return -1;
    };

    auto countSubstring = [](const string &str, const string &sub) -> unsigned long {
        if (sub.length() == 0)
            return 0;
        int count = 0;
        for (int offset = (int)str.find(sub); offset != string::npos;
             offset = (int)str.find(sub, offset + sub.length()))
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
    unsigned long count = countSubstring(str, toolOnWindows ? "\\" : "/");
    if (finalDepth >= count)
    {
        return str;
    }
    size_t index = nthOccurrence(str, toolOnWindows ? "\\" : "/", count - finalDepth);
    return str.substr(index + 1, str.size() - 1);
}
