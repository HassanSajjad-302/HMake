
#include "BBuild.hpp"

#include "fmt/color.h"
#include "fmt/format.h"
#include "fstream"
#include "regex"
#include <utility>

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
};

BIDD::BIDD(const string &path_, bool copy_) : path{path_}, copy{copy_}
{
}

void from_json(const Json &j, BIDD &p)
{
    p.copy = j.at("COPY").get<bool>();
    p.path = j.at("PATH").get<string>();
}

void from_json(const Json &j, BLibraryDependency &p)
{
    p.preBuilt = j.at("PREBUILT").get<bool>();
    p.path = j.at("PATH").get<string>();
    if (p.preBuilt)
    {
        if (j.contains("IMPORTED_FROM_OTHER_HMAKE_PACKAGE"))
        {
            p.imported = j.at("IMPORTED_FROM_OTHER_HMAKE_PACKAGE").get<bool>();
            if (!p.imported)
            {
                p.hmakeFilePath = j.at("HMAKE_FILE_PATH").get<string>();
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
    p.name = j.at("NAME").get<string>();
    p.value = j.at("VALUE").get<string>();
}

ParsedTarget::ParsedTarget(const string &targetFilePath_)
{
    targetFilePath = targetFilePath_;
    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

    targetType = targetFileJson.at("TARGET_TYPE").get<TargetType>();
    if (targetType != TargetType::EXECUTABLE && targetType != TargetType::STATIC && targetType != TargetType::SHARED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "BTargetType value in the targetFile is not correct.");
        exit(EXIT_FAILURE);
    }

    if (targetFileJson.at("VARIANT").get<string>() == "PACKAGE")
    {
        packageMode = true;
    }
    else
    {
        packageMode = false;
    }
    if (packageMode)
    {
        copyPackage = targetFileJson.at("PACKAGE_COPY").get<bool>();
        if (copyPackage)
        {
            packageName = targetFileJson.at("PACKAGE_NAME").get<string>();
            packageCopyPath = targetFileJson.at("PACKAGE_COPY_PATH").get<string>();
            packageVariantIndex = targetFileJson.at("PACKAGE_VARIANT_INDEX").get<int>();
            packageTargetPath =
                packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" + targetName + "/";
        }
    }
    else
    {
        copyPackage = false;
    }
    outputName = targetFileJson.at("OUTPUT_NAME").get<string>();
    outputDirectory = targetFileJson.at("OUTPUT_DIRECTORY").get<path>().string();
    compiler = targetFileJson.at("COMPILER").get<Compiler>();
    linker = targetFileJson.at("LINKER").get<Linker>();
    if (targetType == TargetType::STATIC)
    {
        archiver = targetFileJson.at("ARCHIVER").get<Archiver>();
    }
    environment = targetFileJson.at("ENVIRONMENT").get<Environment>();
    compilerFlags = targetFileJson.at("COMPILER_FLAGS").get<string>();
    linkerFlags = targetFileJson.at("LINKER_FLAGS").get<string>();
    // TODO: An Optimization
    //  If source for a target is collected from a sourceDirectory with some regex, then similar source should not be
    //  collected again.
    sourceFiles = SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson, targetFilePath, "SOURCE_");
    modulesSourceFiles =
        SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson, targetFilePath, "MODULES_");

    libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<vector<BLibraryDependency>>();
    if (packageMode)
    {
        includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<BIDD>>();
    }
    else
    {
        vector<string> includeDirs = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<string>>();
        for (auto &i : includeDirs)
        {
            includeDirectories.emplace_back(i, true);
        }
    }
    compilerTransitiveFlags = targetFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<string>();
    linkerTransitiveFlags = targetFileJson.at("LINKER_TRANSITIVE_FLAGS").get<string>();
    compileDefinitions = targetFileJson.at("COMPILE_DEFINITIONS").get<vector<BCompileDefinition>>();
    preBuildCustomCommands = targetFileJson.at("PRE_BUILD_CUSTOM_COMMANDS").get<vector<string>>();
    postBuildCustomCommands = targetFileJson.at("POST_BUILD_CUSTOM_COMMANDS").get<vector<string>>();

    buildCacheFilesDirPath = (path(targetFilePath).parent_path() / ("Cache_Build_Files/")).generic_string();
    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
    }
    // Parsing finished

    actualOutputName = getActualNameFromTargetName(targetType, Cache::osFamily, targetName);
    auto [pos, Ok] = variantFilePaths.emplace(path(targetFilePath).parent_path().parent_path().string() + "/" +
                                              (packageMode ? "packageVariant.hmake" : "projectVariant.hmake"));
    variantFilePath = &(*pos);
    if (!sourceFiles.empty() && !modulesSourceFiles.empty())
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "A Target can not have both module-source and regular-source. You can make regular-source dependency "
              "of module source.\nTarget: {}.\n",
              targetFilePath);
        exit(EXIT_FAILURE);
    }
    if (!modulesSourceFiles.empty())
    {
        isModule = true;
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

void ParsedTarget::setCompileCommand()
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

string &ParsedTarget::getCompileCommand()
{
    if (compileCommand.empty())
    {
        setCompileCommand();
    }
    return compileCommand;
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
    if (targetType == TargetType::STATIC)
    {
        const ArchiveCommandPrintSettings &acpSettings = settings.acpSettings;
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

        populateCommandAndPrintCommandWithObjectFiles(linkOrArchiveCommand, linkOrArchiveCommandPrintFirstHalf,
                                                      acpSettings.objectFiles);
    }
    else
    {
        const LinkCommandPrintSettings &lcpSettings = settings.lcpSettings;

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

        populateCommandAndPrintCommandWithObjectFiles(linkOrArchiveCommand, linkOrArchiveCommandPrintFirstHalf,
                                                      lcpSettings.objectFiles);

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

        auto getLinkFlagPrint = [this](const string &libraryPath, const string &libraryName,
                                       const PathPrint &pathPrint) {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return getReducedPath(libraryPath + libraryName + ".lib", pathPrint) + " ";
            }
            else
            {
                return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) +
                       " ";
            }
        };

        for (const ParsedTarget *parsedTargetDep : libraryParsedTargetDependencies)
        {
            linkOrArchiveCommand += getLinkFlag(parsedTargetDep->outputDirectory, parsedTargetDep->outputName);
            linkOrArchiveCommandPrintFirstHalf += getLinkFlagPrint(
                parsedTargetDep->outputDirectory, parsedTargetDep->outputName, lcpSettings.libraryDependencies);
        }

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

string &Node::getFileName()
{
    if (fileName.empty())
    {
        fileName = path(filePath).filename().string();
    }
    return fileName;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

void from_json(const Json &j, Node *node)
{
    node = Node::getNodeFromString(j);
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j["SRC_FILE"] = sourceNode.node;
    j["HEADER_DEPENDENCIES"] = sourceNode.headerDependencies;
}

void from_json(const Json &j, SourceNode &sourceNode)
{
    // from_json function of Node* did not work correctly, so not using it.*/
    // sourceNode.node = j.at("SRC_FILE").get<Node*>();
    // sourceNode.headerDependencies = j.at("HEADER_DEPENDENCIES").get<set<Node *>>();

    sourceNode.node = Node::getNodeFromString(j.at("SRC_FILE").get<string>());
    vector<string> headerDeps;
    headerDeps = j.at("HEADER_DEPENDENCIES").get<vector<string>>();
    for (const auto &h : headerDeps)
    {
        sourceNode.headerDependencies.emplace(Node::getNodeFromString(h));
    }
    sourceNode.presentInCache = true;
    sourceNode.presentInSource = false;
}

SourceNode &BTargetCache::addNodeInSourceFileDependencies(const string &str)
{
    SourceNode sourceNode{.node = Node::getNodeFromString(str)};
    auto [pos, ok] = sourceFileDependencies.emplace(sourceNode);
    if (!pos->targetCacheSearched)
    {
        pos->targetCacheSearched = true;
        pos->presentInCache = !ok;
        pos->presentInSource = true;
    }
    return const_cast<SourceNode &>(*pos);
}

void to_json(Json &j, const BTargetCache &bTargetCache)
{
    j["COMPILE_COMMAND"] = bTargetCache.compileCommand;
    j["LINK_COMMAND"] = bTargetCache.linkCommand;
    j["DEPENDENCIES"] = bTargetCache.sourceFileDependencies;
}

void from_json(const Json &j, BTargetCache &bTargetCache)
{
    bTargetCache.compileCommand = j["COMPILE_COMMAND"];
    bTargetCache.linkCommand = j["LINK_COMMAND"];
    bTargetCache.sourceFileDependencies = j.at("DEPENDENCIES").get<set<SourceNode>>();
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

    printCommand = move(printCommandFirstHalf);

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
      PostBasic(buildTool, commandFirstHalf, move(printCommandFirstHalf), buildCacheFilesDirPath, fileName, pathPrint,
                false)
{
}

bool PostCompile::checkIfFileIsInEnvironmentIncludes(const string &str)
{
    // If a file is in environment includes, it is not marked as dependency as an optimization.
    // If a file is in subdirectory of environment include, it is still marked as dependency.
    // It is not checked if any of environment includes is related(equivalent, subdirectory) with any of normal includes
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
        else if (*iter == sourceNode.node->getFileName())
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

    string headerFileContents =
        file_to_string(parsedTarget.buildCacheFilesDirPath + sourceNode.node->getFileName() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file. While last line is an empty line
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
    return tie(lhs.sourcePath, lhs.logicalName) < tie(lhs.sourcePath, lhs.logicalName);
}

void SMRuleRequires::populateFromJson(const Json &j, const string &smFilePath, const string &provideLogicalName)
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
        // This is header unit
        type = SM_REQUIRE_TYPE::HEADER_UNIT;
        j.at("lookup-method").get<string>() == "include-angle" ? angle = true : angle = false;
        sourcePath = j.at("source-path").get<string>();
    }
    else
    {
        type = logicalName.contains(':') ? SM_REQUIRE_TYPE::PARTITION_EXPORT : SM_REQUIRE_TYPE::PRIMARY_EXPORT;
    }
    {
        auto [pos, ok] = SMRuleRequires::setLogicalName->emplace(*this);
        if (!ok)
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In SMFile: \"{}\", 2 requires have same \"logical-name\" {}.\n", smFilePath, logicalName);
            exit(EXIT_FAILURE);
        }
    }

    if (type == SM_REQUIRE_TYPE::HEADER_UNIT)
    {
        auto [pos, ok] = SMRuleRequires::setHeaderUnits->emplace(*this);
        if (!ok)
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In SMFile: \"{}\", 2 requires have same \"source-path\" {} and \"logical-name\" {}.\n", smFilePath,
                  sourcePath, logicalName);
            exit(EXIT_FAILURE);
        }
    }
}

SMFile::SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_)
    : sourceNode{sourceNode_}, parsedTarget{parsedTarget_}
{
}

SMFile::SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_, bool angle_)
    : sourceNode{sourceNode_}, parsedTarget{parsedTarget_}, angle{angle_}
{
}

void SMFile::populateFromJson(const Json &j)
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
    if (rule.contains("requires"))
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
            requireDependencies[i].populateFromJson(requireJsons[i], sourceNode.node->filePath, logicalName);
            if (requireDependencies[i].type == SM_REQUIRE_TYPE::PARTITION_EXPORT)
            {
                type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
            }
        }
    }

    if (!hasProvide && !hasRequires)
    {
        type = SM_FILE_TYPE::HEADER_UNIT;
    }
    else if (hasProvide)
    {
        type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
    }
    else
    {
        if (type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
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
        return "/exportHeader /headerName:" + angleStr + str;
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

string SMFile::getRequireFlag(const string &ifcFilePath) const
{
    if (type == SM_FILE_TYPE ::NOT_ASSIGNED)
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
        string str = angle ? "angle" : "quote";
        return "/headerUnit:" + str + " " + logicalName + "=" + ifcFilePath;
    }
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
}

string SMFile::getRequireFlagPrint(const string &ifcFilePath) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&]() {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName
                   : logicalName + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };
    if (type == SM_FILE_TYPE ::NOT_ASSIGNED)
    {
        print(stderr, "HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return ccpSettings.infrastructureFlags ? "/reference " : "" + getRequireIFCPathOrLogicalName();
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        string str = angle ? "angle" : "quote";
        return ccpSettings.infrastructureFlags ? "/headerUnit:" + str + " " : "" + getRequireIFCPathOrLogicalName();
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
        for (const SMFile *linkDependency : commandLineFileDependencies)
        {
            string ifcFilePath =
                linkDependency->parsedTarget.buildCacheFilesDirPath + linkDependency->fileName + ".ifc";
            moduleCompileCommandPrintLastHalf += linkDependency->getRequireFlagPrint(ifcFilePath) + " ";
        }
    }
    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? parsedTarget.getInfrastructureFlags() : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf += getFlagPrint(parsedTarget.buildCacheFilesDirPath + fileName);
    return moduleCompileCommandPrintLastHalf;
}

bool SMFileCompareWithHeaderUnits::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    return tie(lhs.sourceNode, lhs.parsedTarget.variantFilePath, lhs.angle) <
           tie(rhs.sourceNode, rhs.parsedTarget.variantFilePath, rhs.angle);
}

bool SMFileCompareWithoutHeaderUnits::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    return tie(lhs.sourceNode, *lhs.parsedTarget.variantFilePath) <
           tie(rhs.sourceNode, *rhs.parsedTarget.variantFilePath);
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

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node->filePath < rhs.node->filePath;
}

Node *Node::getNodeFromString(const string &str)
{
    Node *tempNode = new Node{.filePath = str};

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

void ParsedTarget::setSourceNodeCompilationStatus(SourceNode &sourceNode, bool angle = false) const
{
    sourceNode.compilationStatus = FileStatus::UPDATED;

    string fileName = sourceNode.headerUnit ? sourceNode.node->getFileName() + (angle ? "_angle" : "_quote")
                                            : sourceNode.node->getFileName();
    path objectFilePath = path(buildCacheFilesDirPath + fileName + ".o");

    if (!std::filesystem::exists(objectFilePath))
    {
        sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    sourceNode.node->checkIfNotUpdatedAndUpdate();
    if (sourceNode.node->lastUpdateTime > objectFileLastEditTime)
    {
        sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
    }
    else
    {
        for (auto &d : sourceNode.headerDependencies)
        {
            d->checkIfNotUpdatedAndUpdate();
            if (d->lastUpdateTime > objectFileLastEditTime)
            {
                sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
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
        buildNode.needsLinking = true;
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
            if (!buildNode.needsLinking)
            {
                if (last_write_time(i.path) > last_write_time(outputPath))
                {
                    buildNode.needsLinking = true;
                }
            }
        }
    }
}

void ParsedTarget::populateSourceTree()
{
    checkForRelinkPrebuiltDependencies();
    if (std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        if (std::filesystem::exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
        {
            Json targetCacheJson;
            ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
            buildNode.targetCache = targetCacheJson;
            if (buildNode.targetCache.compileCommand != getCompileCommand())
            {
                buildNode.targetCache.sourceFileDependencies.clear();
            }
        }
    }
    else
    {
        create_directory(buildCacheFilesDirPath);
    }

    for (const auto &sourceFile : sourceFiles)
    {
        SourceNode &sourceNode = buildNode.targetCache.addNodeInSourceFileDependencies(sourceFile);
        if (sourceNode.presentInSource != sourceNode.presentInCache)
        {
            sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
        }
        else
        {
            setSourceNodeCompilationStatus(sourceNode);
        }
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            buildNode.outdatedFiles.emplace_back(&sourceNode);
        }
    }
    buildNode.compilationNotStartedSize = buildNode.outdatedFiles.size();
    buildNode.compilationNotCompletedSize = buildNode.compilationNotStartedSize;
    if (buildNode.compilationNotStartedSize)
    {
        buildNode.needsLinking = true;
    }
    buildNode.targetCache.compileCommand = getCompileCommand();

    // Removing unused object-files from buildCacheFilesDirPath as they later affect linker command with *.o option.
    set<string> tmpObjectFiles;
    for (const auto &src : buildNode.targetCache.sourceFileDependencies)
    {
        if (src.presentInSource == src.presentInCache)
        {
            tmpObjectFiles.emplace(src.node->getFileName() + ".o");
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
    }

    if (!buildNode.needsLinking)
    {
        if (getLinkOrArchiveCommand() != buildNode.targetCache.linkCommand)
        {
            buildNode.needsLinking = true;
        }
    }

    if (buildNode.needsLinking)
    {
        for (ParsedTarget *parsedTarget : buildNode.linkDependents)
        {
            parsedTarget->buildNode.needsLinking = true;
            ++(parsedTarget->buildNode.needsLinkDependenciesSize);
        }
    }
}

void ParsedTarget::populateModuleTree(Builder &builder, map<string *, map<string, SourceNode *>> &requirePaths,
                                      set<TarjanNode<SMFile>> &tarjanNodesSMFiles)
{
    checkForRelinkPrebuiltDependencies();
    if (std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        if (std::filesystem::exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
        {
            Json targetCacheJson;
            ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
            buildNode.targetCache = targetCacheJson;
            if (buildNode.targetCache.compileCommand != getCompileCommand())
            {
                buildNode.targetCache.sourceFileDependencies.clear();
            }
        }
    }
    else
    {
        create_directory(buildCacheFilesDirPath);
    }

    set<SMFile, SMFileCompareWithoutHeaderUnits> &smFilesWithoutHeaderUnits =
        builder.smFilesWithoutHeaderUnits[const_cast<string *>(variantFilePath)];
    set<SMFile, SMFileCompareWithHeaderUnits> &smFilesWithHeaderUnits =
        builder.smFilesWithHeaderUnits[const_cast<string *>(variantFilePath)];
    map<string, SourceNode *> &requirePathsMap = requirePaths[const_cast<string *>(variantFilePath)];
    for (const auto &sourceFile : modulesSourceFiles)
    {
        SourceNode &sourceNode = buildNode.targetCache.addNodeInSourceFileDependencies(sourceFile);
        if (auto [smFilePos, Ok] = smFilesWithoutHeaderUnits.emplace(SMFile(sourceNode, *this)); Ok)
        {
            if (sourceNode.presentInSource != sourceNode.presentInCache)
            {
                sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            }
            else
            {
                // For Header_Unit, we will recheck compilation-status later. We could not have avoided this check. It
                // will always return FileStatus::NEEDS_RECOMPILE as headerUnit object-file has extension including info
                // about whether it's a quoted or angled header-unit.
                setSourceNodeCompilationStatus(sourceNode);
            }
            if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
            {
                // TODO: Provide printOnlyOnError As Settings.
                PostBasic postBasic = GenerateSMRulesFile(sourceNode, true);
                postBasic.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                if (!postBasic.successfullyCompleted)
                {
                    exit(EXIT_FAILURE);
                }
            }

            string smFilePath = buildCacheFilesDirPath + sourceNode.node->getFileName() + ".smrules";

            Json smFileJson;
            ifstream(smFilePath) >> smFileJson;

            auto &smFile = const_cast<SMFile &>(*smFilePos);
            smFile.populateFromJson(smFileJson);

            if (smFile.hasProvide)
            {
                if (auto [pos, ok] =
                        requirePathsMap.insert({smFile.logicalName, const_cast<SourceNode *>(&sourceNode)});
                    !ok)
                {
                    const auto &[key, val] = *pos;
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Module {} Is Being Provided By 2 different files:\n1){}\n2){}\n", val->node->filePath, key,
                          sourceNode.node->filePath);
                    exit(EXIT_FAILURE);
                }
            }
            if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
            {
                sourceNode.headerUnit = true;
                sourceNode.materialize = false;
                smFile.materialize = false;
                smFile.angle = true;
                auto [smFile1, OkSMFile1] = smFilesWithHeaderUnits.emplace(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile1));
                if (!OkSMFile1)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Angled Header-Unit {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
                smFile.angle = false;
                auto [smFile2, OkSMFile2] = smFilesWithHeaderUnits.emplace(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile2));
                if (!OkSMFile2)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Quoted Header-Unit {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
            }
            else
            {
                smFile.angle = false;
                auto [smFile1, OkSMFile1] = smFilesWithHeaderUnits.emplace(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile1));
                if (!OkSMFile1)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Module Source {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
            }
        }
        else
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "Module Source {} is provided by this target {}. But, is also provided by some other target in same "
                  "variant",
                  sourceFile, targetFilePath);
        }
    }

    if (!buildNode.needsLinking)
    {
        if (getLinkOrArchiveCommand() != buildNode.targetCache.linkCommand)
        {
            buildNode.needsLinking = true;
        }
    }
    buildNode.targetCache.compileCommand = getCompileCommand();
}

string ParsedTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c /Tp";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it prints 2 header deps in one line and
        // no space in them so no way of knowing whether this is a space in path or 2 different headers. Which then
        // breaks when last_write_time is checked for that path.
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
            getReducedPath(buildCacheFilesDirPath + sourceNode.node->getFileName() + ".o", ccpSettings.objectFile) +
            " ";
    }
    return command;
}

PostCompile ParsedTarget::CompileSMFile(SMFile *smFile)
{
    string finalCompileCommand = getCompileCommand() + " ";

    for (const SMFile *lineDependency : smFile->commandLineFileDependencies)
    {
        string ifcFilePath =
            addQuotes(lineDependency->parsedTarget.buildCacheFilesDirPath + lineDependency->fileName + ".ifc");
        finalCompileCommand += lineDependency->getRequireFlag(ifcFilePath) + " ";
    }
    finalCompileCommand += getInfrastructureFlags() + addQuotes(smFile->sourceNode.node->filePath) + " ";

    finalCompileCommand += smFile->getFlag(buildCacheFilesDirPath + smFile->fileName);

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile->getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       smFile->fileName,
                       settings.ccpSettings.outputAndErrorFiles};
}

PostCompile ParsedTarget::Compile(SourceNode &sourceNode)
{
    string compileFileName = sourceNode.node->getFileName();

    string finalCompileCommand = getCompileCommand() + " ";

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

void ParsedTarget::populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                                 const PathPrint &objectFilesPathPrint)
{
    if (isModule)
    {
        stack<SMFile *> smFilesStack;

        for (SMFile *smFile : smFiles)
        {
            if (smFile->type == SM_FILE_TYPE::HEADER_UNIT)
            {
                if (!smFile->materialize)
                {
                    continue;
                }
            }
            smFilesStack.emplace(smFile);
        }

        vector<SMFile *> objectFiles;
        while (!smFilesStack.empty())
        {
            SMFile *smFile = smFilesStack.top();
            smFilesStack.pop();
            for (SMFile *smFileDep : smFile->fileDependencies)
            {
                smFilesStack.emplace(smFileDep);
            }
            if (auto it = find(objectFiles.begin(), objectFiles.end(), smFile); it == objectFiles.end())
            {
                objectFiles.emplace_back(smFile);
            }
            else
            {
                rotate(it, it + 1, objectFiles.end());
            }
        }

        for (SMFile *smFile : objectFiles)
        {
            command += addQuotes(smFile->parsedTarget.buildCacheFilesDirPath + smFile->fileName + ".o") + " ";
            if (objectFilesPathPrint.printLevel != PathPrintLevel::NO)
            {
                printCommand += getReducedPath(smFile->parsedTarget.buildCacheFilesDirPath + smFile->fileName + ".o",
                                               objectFilesPathPrint) +
                                " ";
            }
        }
    }
    else
    {
#ifdef _WIN32
        command += addQuotes(buildCacheFilesDirPath + "*.o") + " ";
        if (objectFilesPathPrint.printLevel != PathPrintLevel::NO)
        {
            printCommand += getReducedPath(buildCacheFilesDirPath + "*.o", objectFilesPathPrint) + " ";
        }
#else
        command += addQuotes(buildCacheFilesDirPath) + "*.o ";
        if (objectFilesPathPrint.printLevel != PathPrintLevel::NO)
        {
            string str = getReducedPath(buildCacheFilesDirPath, objectFilesPathPrint);
            if (!str.empty())
            {
                printCommand += str + "*.o ";
            }
        }
#endif
    }
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
        finalCompileCommand +=
            "/scanDependencies" + addQuotes(buildCacheFilesDirPath + sourceNode.node->getFileName() + ".smrules") + " ";
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Modules supported only on MSVC");
    }

    return printOnlyOnError
               ? PostBasic(compiler, finalCompileCommand, "", buildCacheFilesDirPath, sourceNode.node->getFileName(),
                           settings.ccpSettings.outputAndErrorFiles, true)
               : PostBasic(compiler, finalCompileCommand,
                           getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                           buildCacheFilesDirPath, sourceNode.node->getFileName(),
                           settings.ccpSettings.outputAndErrorFiles, true);
}

TargetType ParsedTarget::getTargetType() const
{
    return targetType;
}

void ParsedTarget::pruneAndSaveBuildCache(const bool successful)
{
    for (auto it = buildNode.targetCache.sourceFileDependencies.begin();
         it != buildNode.targetCache.sourceFileDependencies.end();)
    {
        !it->presentInSource || (it->headerUnit && !it->materialize)
            ? it = buildNode.targetCache.sourceFileDependencies.erase(it)
            : ++it;
    }
    if (!successful)
    {
        for (auto it = buildNode.targetCache.sourceFileDependencies.begin();
             it != buildNode.targetCache.sourceFileDependencies.end();)
        {
            it->compilationStatus == FileStatus::NEEDS_RECOMPILE
                ? it = buildNode.targetCache.sourceFileDependencies.erase(it)
                : ++it;
        }
    }
    Json cacheFileJson = buildNode.targetCache;
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
                string preBuiltTargetName = preBuiltTarget["NAME"];
                copyFrom = preBuiltTarget["PATH"];
                string preBuiltPackageCopyPath = preBuiltTarget["PACKAGE_COPY_PATH"];
                string targetInstallPath = packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" +
                                           preBuiltTargetName + "/";
                create_directories(path(targetInstallPath));
                copy(path(copyFrom), path(targetInstallPath), copy_options::update_existing);
                ofstream(targetInstallPath + preBuiltTargetName + ".hmake")
                    << preBuiltTarget["CONSUMER_DEPENDENCIES"].dump(4);
            }
        }
    }
}

bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs)
{
    return lhs.targetFilePath < rhs.targetFilePath;
}

bool VariantParsedTargetStringComparator::operator()(const VariantParsedTargetString &lhs,
                                                     const VariantParsedTargetString &rhs) const
{
    bool index1 = lhs.index();
    bool index2 = rhs.index();
    if (index1 && index2)
    {
        return std::get<string>(lhs) < std::get<string>(rhs);
    }
    else if (index1 && !index2)
    {
        return std::get<string>(lhs) < std::get<ParsedTarget>(rhs).targetFilePath;
    }
    else if (!index1 && index2)
    {
        return std::get<ParsedTarget>(lhs).targetFilePath < std::get<string>(rhs);
    }
    else
    {
        return std::get<ParsedTarget>(lhs).targetFilePath < std::get<ParsedTarget>(rhs).targetFilePath;
    }
}

Builder::Builder(const set<string> &targetFilePaths)
{
    // This will only have targets with parsedTarget() called before and libraryDependenciesParsedTargets populated if
    // any.
    vector<ParsedTarget *> dirtyTargets;
    unsigned short threadLaunchCount;
    {
        // This will not be used later, Builder::SMFiles will be used.
        vector<ParsedTarget *> moduleTargets;
        populateParsedTargetSetAndModuleTargets(targetFilePaths, moduleTargets);
        for (ParsedTarget *t : sourceTargets)
        {
            t->populateSourceTree();
        }
        removeRedundantNodesFromSourceTree();
        sourceTargetsIndex = sourceTargets.size();
        unsigned short moduleThreadsNeeded = populateCanBeCompiledAndReturnModuleThreadsNeeded(moduleTargets);
        unsigned short sourceThreadsNeeded = 0;

        // This can happen when the target is deleted or one source files is removed in new configuration or a
        // prebuilt-library dependency is touched
        {
            for (ParsedTarget *sourceTarget : sourceTargets)
            {
                dirtyTargets.emplace_back(sourceTarget);
                unsigned short sourceThreadsTarget = sourceTarget->buildNode.outdatedFiles.size();
                sourceThreadsNeeded += sourceThreadsTarget;
                if (sourceTarget->buildNode.needsLinking && !sourceThreadsTarget &&
                    !sourceTarget->buildNode.needsLinkDependenciesSize)
                {
                    canBeLinked.emplace_back(sourceTarget);
                }
            }
            for (ParsedTarget *moduleTarget : moduleTargets)
            {
                if (moduleTarget->buildNode.needsLinking)
                {
                    dirtyTargets.emplace_back(moduleTarget);
                    if (!moduleTarget->smFilesToBeCompiledSize && !moduleTarget->buildNode.needsLinkDependenciesSize)
                    {
                        canBeLinked.emplace_back(moduleTarget);
                    }
                }
            }
        }

        // TODO
        // linkThreadsNeeded and moduleThreadsNeeded are not accurate. Will use this later.
        // https://cs.stackexchange.com/a/16829
        unsigned short totalThreadsNeeded = sourceThreadsNeeded + moduleThreadsNeeded + dirtyTargets.size();
        threadLaunchCount =
            totalThreadsNeeded > settings.maximumBuildThreads ? settings.maximumBuildThreads : totalThreadsNeeded;
    }
    totalTargetsNeedingLinking = dirtyTargets.size();

    for (const ParsedTarget *target : dirtyTargets)
    {
        target->executePreBuildCommands();
    }

    vector<thread *> threads;
    if (threadLaunchCount)
    {
        while (threads.size() != threadLaunchCount - 1)
        {
            threads.emplace_back(new thread{&Builder::buildSourceFiles, this});
        }
        if (threadLaunchCount > 1)
        {
            buildSourceFiles();
        }
    }

    for (thread *t : threads)
    {
        t->join();
        delete t;
    }

    for (const ParsedTarget *parsedTarget : dirtyTargets)
    {
        parsedTarget->executePostBuildCommands();
    }

    for (ParsedTarget *parsedTarget : dirtyTargets)
    {
        parsedTarget->copyParsedTarget();
    }
}

void Builder::populateParsedTargetSetAndModuleTargets(const set<string> &targetFilePaths,
                                                      vector<ParsedTarget *> &moduleTargets)
{
    set<string> parsedVariantsForModules;
    // parsedTargetStack will only contain pointers to parsedTargetSet elements for which libsParsed is false.
    stack<ParsedTarget *> parsedTargetsStack;

    // Calls parseTarget if target was not preset in parsedTargetSet
    auto getParsedTargetPointer = [this](const string &targetFilePath) -> tuple<ParsedTarget *, bool> {
        auto [foundIter, Ok] = parsedTargetSet.emplace(targetFilePath);
        if (foundIter.operator*().index())
        {
            const_cast<variant<ParsedTarget, string> &>(foundIter.operator*()) = ParsedTarget(targetFilePath);
            return make_tuple(const_cast<ParsedTarget *>(&(std::get<0>(foundIter.operator*()))), true);
        }
        return make_tuple(const_cast<ParsedTarget *>(&(std::get<0>(foundIter.operator*()))), false);
    };

    auto checkVariantOfTargetForModules = [&](const string &variantFilePath) {
        if (!parsedVariantsForModules.contains(variantFilePath))
        {
            parsedVariantsForModules.emplace(variantFilePath);
            Json variantJson;
            ifstream(variantFilePath) >> variantJson;
            for (const string &targetFilePath : variantJson.at("TARGETS_WITH_MODULES").get<vector<string>>())
            {
                auto [ptr, Ok] = getParsedTargetPointer(targetFilePath);
                // We push on Stack so Regular Target Dependencies of this Module Target are also added
                parsedTargetsStack.emplace(ptr);
                moduleTargets.emplace_back(ptr);
            }
        }
    };

    for (const string &targetFilePath : targetFilePaths)
    {
        auto [ptr, Ok] = getParsedTargetPointer(targetFilePath);
        if (Ok)
        {
            if (ptr->isModule)
            {
                checkVariantOfTargetForModules(*(ptr->variantFilePath));
            }
            // The pointer may have already been pushed in checkVariantOfTargetForModules but may have not.
            parsedTargetsStack.emplace(ptr);
        }
    }

    set<TarjanNode<ParsedTarget>> tarjanNodesParsedTargets;
    while (!parsedTargetsStack.empty())
    {
        ParsedTarget *ptr = parsedTargetsStack.top();
        parsedTargetsStack.pop();
        if (!ptr->libsParsed)
        {
            for (auto &dep : ptr->libraryDependencies)
            {
                if (dep.preBuilt)
                {
                    continue;
                }
                auto [ptrDep, unused] = getParsedTargetPointer(dep.path);
                if (ptr == ptrDep)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Library {} can not depend on itself.\n", ptr->targetFilePath);
                    exit(EXIT_SUCCESS);
                }
                ptr->libraryParsedTargetDependencies.emplace(ptrDep);
                ptrDep->buildNode.linkDependents.emplace_back(ptr);
                if (ptrDep->isModule)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "{}  Has Dependency {} Which Has Module Source. A Target With Module Source Cannot "
                          "Be A Dependency Of Other Target.\n",
                          ptr->targetFilePath, ptrDep->targetFilePath);
                }
                parsedTargetsStack.emplace(ptrDep);
            }
            ptr->libsParsed = true;
            if (!ptr->isModule)
            {
                auto [pos, success] = tarjanNodesParsedTargets.emplace(ptr);
                auto &b = *pos;
                auto &tarjanNode = const_cast<TarjanNode<ParsedTarget> &>(b);
                for (ParsedTarget *dep : ptr->libraryParsedTargetDependencies)
                {
                    auto [posDep, OkDep] = tarjanNodesParsedTargets.emplace(dep);
                    auto &a = *posDep;
                    tarjanNode.deps.emplace_back(const_cast<TarjanNode<ParsedTarget> *>(&a));
                }
            }
        }
    }
    TarjanNode<ParsedTarget>::tarjanNodes = &tarjanNodesParsedTargets;
    TarjanNode<ParsedTarget>::findSCCS();

    TarjanNode<ParsedTarget>::checkForCycle([](ParsedTarget *parsedTarget) { return parsedTarget->targetFilePath; });
    sourceTargets = TarjanNode<ParsedTarget>::topologicalSort;
}

int Builder::populateCanBeCompiledAndReturnModuleThreadsNeeded(const vector<ParsedTarget *> &moduleTargets)
{
    // We now know all the nodes that need relink. Based on that we can know what if some modules need relink

    map<string *, map<string, SourceNode *>> requirePaths;
    set<TarjanNode<SMFile>> tarjanNodesSMFiles;

    for (ParsedTarget *moduleTarget : moduleTargets)
    {
        moduleTarget->populateModuleTree(*this, requirePaths, tarjanNodesSMFiles);
    }

    for (auto &smFilesWithHeaderUnitsPair : smFilesWithHeaderUnits)
    {
        set<SMFile, SMFileCompareWithHeaderUnits> &smFilesWithHeaderUnitsSet = smFilesWithHeaderUnitsPair.second;
        map<string, SourceNode *> &requirePathsMap = requirePaths[smFilesWithHeaderUnitsPair.first];
        for (auto &smFileConst : smFilesWithHeaderUnitsSet)
        {
            auto &smFile = const_cast<SMFile &>(smFileConst);
            auto smFileTarjanNodeIt = tarjanNodesSMFiles.find(TarjanNode(&smFile));

            for (auto &dep : smFile.requireDependencies)
            {
                if (dep.type == SM_REQUIRE_TYPE::HEADER_UNIT)
                {

                    Node node{.filePath = path(dep.sourcePath).generic_string()};
                    SourceNode sourceNode{.node = &node};
                    SourceNode &depSMFileSourceNode = *(const_cast<SourceNode *>(&sourceNode));
                    if (auto smFileDepIt =
                            smFilesWithHeaderUnitsSet.find(SMFile(depSMFileSourceNode, smFile.parsedTarget, dep.angle));
                        smFileDepIt != smFilesWithHeaderUnitsSet.end())
                    {
                        auto &smFileDep = const_cast<SMFile &>(*smFileDepIt);
                        smFileDep.materialize = true;
                        smFileDep.sourceNode.materialize = true;
                        smFileDep.logicalName = dep.logicalName;
                        smFile.fileDependencies.emplace_back(&smFileDep);
                        smFileDep.fileDependents.emplace_back(&smFile);
                        auto smFileTarjanNodeDepIt = tarjanNodesSMFiles.find(TarjanNode(&smFileDep));
                        smFileTarjanNodeIt->deps.emplace_back(
                            const_cast<TarjanNode<SMFile> *>(&(*smFileTarjanNodeDepIt)));
                    }
                    else
                    {
                        // TODO
                        // Currently, Include-Header-Units(Header-Units not part of source but coming from
                        // Include-Directory) are not supported, but only Source-Header-Units are supported(Header-Units
                        // also part of module-source). It can be supported here.
                        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                              "No File Provides This {}.\n", dep.logicalName);
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    if (auto it = requirePathsMap.find(dep.logicalName); it == requirePathsMap.end())
                    {
                        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                              "No File Provides This {}.\n", dep.logicalName);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        SourceNode &depSMFileSourceNode = *(const_cast<SourceNode *>(it->second));
                        auto smFileDepIt =
                            smFilesWithHeaderUnitsSet.find(SMFile(depSMFileSourceNode, smFile.parsedTarget, false));
                        auto &smFileDep = const_cast<SMFile &>(*smFileDepIt);
                        smFile.fileDependencies.emplace_back(&smFileDep);
                        smFileDep.fileDependents.emplace_back(&smFile);
                        auto smFileTarjanNodeDepIt = tarjanNodesSMFiles.find(TarjanNode(&smFileDep));
                        smFileTarjanNodeIt->deps.emplace_back(
                            const_cast<TarjanNode<SMFile> *>(&(*smFileTarjanNodeDepIt)));
                    }
                }
            }
            smFile.parsedTarget.smFiles.emplace_back(&smFile);
        }
    }

    TarjanNode<SMFile>::tarjanNodes = &tarjanNodesSMFiles;
    TarjanNode<SMFile>::findSCCS();
    TarjanNode<SMFile>::checkForCycle([](SMFile *smFile) { return smFile->sourceNode.node->filePath; });

    vector<SMFile *> finalSMFiles = TarjanNode<SMFile>::topologicalSort;

    unsigned short moduleThreadsNeeded = 0;
    for (auto it = finalSMFiles.begin(); it != finalSMFiles.end();)
    {
        SMFile *smFile = it.operator*();
        smFile->fileName = smFile->sourceNode.node->fileName;
        if (smFile->type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smFile->fileName += smFile->angle ? "_angle" : "_quote";
            if (smFile->materialize)
            {
                if (smFile->sourceNode.presentInSource != smFile->sourceNode.presentInCache)
                {
                    smFile->sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                }
                else
                {
                    smFile->parsedTarget.setSourceNodeCompilationStatus(smFile->sourceNode, smFile->angle);
                }
            }
            else
            {
                smFile->sourceNode.compilationStatus = FileStatus::UPDATED;
            }
        }
        for (SMFile *dependent : smFile->fileDependents)
        {
            if (smFile->sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
            {
                dependent->sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                ++dependent->needsCompileDependenciesSize;
            }
            dependent->commandLineFileDependencies.emplace(smFile);
            for (SMFile *dependency : smFile->commandLineFileDependencies)
            {
                if (dependency->type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION &&
                    dependency->type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    dependent->commandLineFileDependencies.emplace(dependency);
                }
            }
        }
        if (smFile->sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            ++moduleThreadsNeeded;
            smFile->parsedTarget.buildNode.needsLinking = true;
            ++smFile->parsedTarget.smFilesToBeCompiledSize;
            if (!smFile->needsCompileDependenciesSize)
            {
                canBeCompiledModule.emplace_back(const_cast<SMFile *>(smFile));
            }
            ++it;
        }
        else
        {
            it = finalSMFiles.erase(it);
        }
    }
    canBeCompiledModule.reserve(finalSMFiles.size());
    finalSMFilesSize = finalSMFiles.size();
    return moduleThreadsNeeded;
}

void Builder::removeRedundantNodesFromSourceTree()
{
    auto it = sourceTargets.begin();

    while (it != sourceTargets.end())
    {
        (*it)->buildNode.needsLinking ? ++it : it = sourceTargets.erase(it);
    }
}

set<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at("TARGETS").get<set<string>>();
}

set<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec;
    if (isPackage)
    {
        vector<Json> pVariantJson = projectFileJson.at("VARIANTS").get<vector<Json>>();
        for (const auto &i : pVariantJson)
        {
            vec.emplace_back(i.at("INDEX").get<string>());
        }
    }
    else
    {
        vec = projectFileJson.at("VARIANTS").get<vector<string>>();
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

void Builder::buildSourceFiles()
{
    oneAndOnlyMutex.lock();
    linkTargets();
    while (sourceTargetsIndex)
    {
        ParsedTarget *parsedTarget = sourceTargets[sourceTargetsIndex - 1];
        BuildNode &buildNode = parsedTarget->buildNode;
        while (buildNode.compilationNotStartedSize)
        {
            SourceNode *sourceNode = buildNode.outdatedFiles[buildNode.compilationNotStartedSize - 1];
            --buildNode.compilationNotStartedSize;
            oneAndOnlyMutex.unlock();
            PostCompile postCompile = parsedTarget->Compile(*sourceNode);
            postCompile.executePostCompileRoutineWithoutMutex(*sourceNode);
            printMutex.lock();
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
            printMutex.unlock();
            oneAndOnlyMutex.lock();
            if (!postCompile.successfullyCompleted)
            {
                parsedTarget->pruneAndSaveBuildCache(false);
                exit(EXIT_FAILURE);
            }
            sourceNode->compilationStatus = FileStatus::UPDATED;
            --buildNode.compilationNotCompletedSize;
            if (!buildNode.compilationNotCompletedSize && !parsedTarget->buildNode.needsLinkDependenciesSize)
            {
                canBeLinked.emplace_back(parsedTarget);
                linkTargets();
            }
        }
        if (sourceTargetsIndex)
        {
            --sourceTargetsIndex;
        }
    }
    linkTargets();
    oneAndOnlyMutex.unlock();
    buildModuleFiles();
}

void Builder::buildModuleFiles()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        while (canBeCompiledModuleIndex < canBeCompiledModule.size())
        {
            SMFile *smFile = canBeCompiledModule[canBeCompiledModuleIndex];
            ParsedTarget &parsedTarget = smFile->parsedTarget;
            ++canBeCompiledModuleIndex;
            oneAndOnlyMutex.unlock();
            PostCompile postCompile = smFile->parsedTarget.CompileSMFile(smFile);
            postCompile.executePostCompileRoutineWithoutMutex(smFile->sourceNode);
            printMutex.lock();
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
            printMutex.unlock();
            if (!postCompile.successfullyCompleted)
            {
                parsedTarget.pruneAndSaveBuildCache(false);
                exit(EXIT_FAILURE);
            }
            smFile->sourceNode.compilationStatus = FileStatus::UPDATED;
            oneAndOnlyMutex.lock();
            for (SMFile *dependent : smFile->fileDependents)
            {
                --(dependent->needsCompileDependenciesSize);
                if (!dependent->needsCompileDependenciesSize)
                {
                    canBeCompiledModule.emplace_back(dependent);
                }
            }
            --parsedTarget.smFilesToBeCompiledSize;
            if (!parsedTarget.smFilesToBeCompiledSize && !parsedTarget.buildNode.needsLinkDependenciesSize)
            {
                canBeLinked.emplace_back(&parsedTarget);
                linkTargets();
            }
        }
        if (canBeCompiledModuleIndex == finalSMFilesSize)
        {
            loop = false;
            while (canBeLinkedIndex < totalTargetsNeedingLinking)
            {
                linkTargets();
                oneAndOnlyMutex.unlock();
                oneAndOnlyMutex.lock();
            }
        }
        oneAndOnlyMutex.unlock();
    }
}

void Builder::linkTargets()
{
    if (linkThreadsCount == settings.maximumLinkThreads)
    {
        return;
    }
    ++linkThreadsCount;
    bool loop = true;

    while (canBeLinkedIndex < canBeLinked.size())
    {
        ParsedTarget *parsedTarget = canBeLinked[canBeLinkedIndex];
        ++canBeLinkedIndex;
        oneAndOnlyMutex.unlock();

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

        if (postLinkOrArchive->successfullyCompleted)
        {
            parsedTarget->buildNode.targetCache.linkCommand = parsedTarget->getLinkOrArchiveCommand();
        }
        else
        {
            parsedTarget->buildNode.targetCache.linkCommand = "";
        }

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
        oneAndOnlyMutex.lock();
        for (ParsedTarget *dependent : parsedTarget->buildNode.linkDependents)
        {
            if (dependent->buildNode.needsLinking)
            {
                --(dependent->buildNode.needsLinkDependenciesSize);
                if (!dependent->buildNode.needsLinkDependenciesSize)
                {
                    if (dependent->isModule)
                    {
                        if (!dependent->smFilesToBeCompiledSize)
                        {
                            canBeLinked.emplace_back(dependent);
                        }
                    }
                    else
                    {
                        if (!dependent->buildNode.compilationNotCompletedSize)
                        {
                            canBeLinked.emplace_back(dependent);
                        }
                    }
                }
            }
        }
    }
    --linkThreadsCount;
}

// Lockless version. Will try it sometimes later. canBeCompiledIndex is made atomic. that's it.
/*void Builder::buildModuleFiles()
{
    bool loop = true;
    while (loop)
    {
        unsigned long temp = canBeCompiledIndex.load(std::memory_order_acquire);
        while (temp < canBeCompiled.size())
        {
            SMFile *smFile = canBeCompiled[temp];
            bool imp = canBeCompiledIndex.compare_exchange_strong(temp, temp + 1, std::memory_order_release,
                                                                  std::memory_order_relaxed);
            if (!imp)
            {
                break;
            }
            smFile->parsedTarget.CompileSMFile(smFile);
            for (SMFile *dependent : smFile->dependents)
            {
                ++(dependent->numberOfDependenciesCompiled);
                if (dependent->numberOfDependenciesCompiled == dependent->numberOfDependencies)
                {
                    canBeCompiled.emplace_back(dependent);
                }
            }
        }
        if (canBeCompiledIndex.load(std::memory_order_relaxed) == finalSMFilesSize)
        {
            loop = false;
        }
    }
}*/

void Builder::copyPackage(const path &packageFilePath)
{
    Json packageFileJson;
    ifstream(packageFilePath) >> packageFileJson;
    Json variants = packageFileJson.at("VARIANTS").get<Json>();
    path packageCopyPath;
    bool packageCopy = packageFileJson.at("PACKAGE_COPY").get<bool>();
    if (packageCopy)
    {
        string packageName = packageFileJson.at("NAME").get<string>();
        string version = packageFileJson.at("VERSION").get<string>();
        packageCopyPath = packageFileJson.at("PACKAGE_COPY_PATH").get<string>();
        packageCopyPath /= packageName;
        if (packageFileJson.at("CACHE_INCLUDES").get<bool>())
        {
            Json commonFileJson;
            ifstream(current_path() / "Common.hmake") >> commonFileJson;
            Json commonJson;
            for (auto &i : commonFileJson)
            {
                Json commonJsonObject;
                int commonIndex = i.at("INDEX").get<int>();
                commonJsonObject["INDEX"] = commonIndex;
                commonJsonObject["VARIANTS_INDICES"] = i.at("VARIANTS_INDICES").get<Json>();
                commonJson.emplace_back(commonJsonObject);
                path commonIncludePath = packageCopyPath / "Common" / path(to_string(commonIndex)) / "include";
                create_directories(commonIncludePath);
                path dirCopyFrom = i.at("PATH").get<string>();
                copy(dirCopyFrom, commonIncludePath, copy_options::update_existing | copy_options::recursive);
            }
        }
        path consumePackageFilePath = packageCopyPath / "cpackage.hmake";
        Json consumePackageFilePathJson;
        consumePackageFilePathJson["NAME"] = packageName;
        consumePackageFilePathJson["VERSION"] = version;
        consumePackageFilePathJson["VARIANTS"] = variants;
        create_directories(packageCopyPath);
        ofstream(consumePackageFilePath) << consumePackageFilePathJson.dump(4);
    }
}

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint)
{
    assert(pathPrint.printLevel != PathPrintLevel::NO &&
           "HMake Internal Error. Function getReducedPath() should not had been called if PrintLevel is NO.");

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
