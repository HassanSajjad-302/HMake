
#include "BBuild.hpp"

#include "fmt/color.h"
#include "fmt/format.h"
#include "fstream"
#include "iostream"
#include "regex"
#include <utility>

using std::ifstream, std::ofstream, std::filesystem::exists, std::filesystem::copy_options, std::runtime_error,
    std::cout, std::endl, std::to_string, std::filesystem::create_directory, std::filesystem::directory_iterator,
    std::regex, std::filesystem::current_path, std::cerr, std::make_shared, std::make_pair, std::lock_guard,
    std::unique_ptr, std::make_unique, fmt::print, std::filesystem::file_time_type;

void from_json(const Json &j, BTargetType &targetType)
{
    if (j == "EXECUTABLE")
    {
        targetType = BTargetType::EXECUTABLE;
    }
    else if (j == "STATIC")
    {
        targetType = BTargetType::STATIC;
    }
    else if (j == "SHARED")
    {
        targetType = BTargetType::SHARED;
    }
    else if (j == "PLIBRARY_STATIC")
    {
        targetType = BTargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = BTargetType::PLIBRARY_SHARED;
    }
}

void from_json(const Json &j, SourceDirectory &sourceDirectory)
{
    sourceDirectory.sourceDirectory = Directory(j.at("PATH").get<string>());
    sourceDirectory.regex = j.at("REGEX_STRING").get<string>();
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
        if (j.contains("IMPORTED"))
        {
            p.imported = j.at("IMPORTED").get<bool>();
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

ParsedTarget::ParsedTarget(const string &targetFilePath, vector<string> dependents)
{
    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

    targetType = targetFileJson.at("TARGET_TYPE").get<BTargetType>();
    if (targetType != BTargetType::EXECUTABLE && targetType != BTargetType::STATIC && targetType != BTargetType::SHARED)
    {
        cerr << "BTargetType value in the targetFile is not correct.";
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
            packageTargetPath = packageCopyPath / path(packageName) / path(to_string(packageVariantIndex)) / targetName;
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
    if (targetType == BTargetType::STATIC)
    {
        archiver = targetFileJson.at("ARCHIVER").get<Archiver>();
    }
    environment = targetFileJson.at("ENVIRONMENT").get<Environment>();
    compilerFlags = targetFileJson.at("COMPILER_FLAGS").get<string>();
    linkerFlags = targetFileJson.at("LINKER_FLAGS").get<string>();
    sourceFiles = targetFileJson.at("SOURCE_FILES").get<vector<string>>();
    sourceDirectories = targetFileJson.at("SOURCE_DIRECTORIES").get<vector<SourceDirectory>>();

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
            includeDirectories.push_back(BIDD{i, true});
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

    setActualOutputName();
    checkForCircularDependencies(dependents);
    dependents.emplace_back(actualOutputName);

    // TODO
    // Big todo: Currently totally ignoring prebuild libraries
    for (const auto &i : libraryDependencies)
    {
        libraryDependenciesBTargets.emplace_back(i.path, dependents);
    }
}

void ParsedTarget::checkForCircularDependencies(const vector<string> &dependents)
{
    if (find(dependents.begin(), dependents.end(), actualOutputName) != end(dependents))
    {
        cerr << "Circular Dependency Detected" << endl;
        exit(EXIT_FAILURE);
    }
}

void ParsedTarget::setActualOutputName()
{
    assert(targetType != BTargetType::PLIBRARY_SHARED);
    assert(targetType != BTargetType::PLIBRARY_STATIC);

    // TODO
    // This should be OS dependent instead of tools dependent or should it be not?
    // Add target OS target property
    if (targetType == BTargetType::EXECUTABLE)
    {
        actualOutputName = outputName + (compiler.bTFamily == BTFamily::MSVC ? ".exe" : "");
    }
    else if (targetType == BTargetType::STATIC)
    {
        actualOutputName = compiler.bTFamily == BTFamily::MSVC ? "" : "lib";
        actualOutputName += outputName;
        actualOutputName += compiler.bTFamily == BTFamily::MSVC ? ".lib" : ".a";
    }
    else
    {
    }
}

void ParsedTarget::executePreBuildCommands()
{
    // Execute the prebuild commands serially of this target and its dependencies.
    if (!preBuildCustomCommands.empty())
    {
        cout << "Executing PRE_BUILD_CUSTOM_COMMANDS" << endl;
    }
    for (auto &i : preBuildCustomCommands)
    {
        cout << i << endl;
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
    // TODO
    for (auto &i : libraryDependenciesBTargets)
    {
        i.executePreBuildCommands();
    }
    /*for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            BTarget buildTarget(i.path);
            buildTarget.executePreBuildCommands();
        }
        else
        {
            // TODO
            // Currently ignore prebuilt libs for time being
            string dir = path(i.path).parent_path().string();
            string libName = path(i.path).filename().string();
            libName.erase(0, 3);
            libName.erase(libName.find('.'), 2);

            if (!i.imported)
            {
                BPTarget(i.hmakeFilePath, i.path);
            }
        }
    }*/
}

bool ParsedTarget::checkIfAlreadyBuiltAndCreatNecessaryDirectories()
{
    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")) &&
        exists(path(outputDirectory) / actualOutputName))
    {
        return true;
    }
    else
    {
        if (!exists(buildCacheFilesDirPath))
        {
            create_directory(buildCacheFilesDirPath);
        }
        return false;
    }
}

void ParsedTarget::setCompileCommand()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
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

    compileCommand = addQuotes(compiler.bTPath.make_preferred().string()) + " ";
    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        compileCommandFirstHalf += getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    compileCommand += environment.compilerFlags + " ";
    if (ccpSettings.environmentCompilerFlags)
    {
        compileCommandFirstHalf += environment.compilerFlags + " ";
    }
    compileCommand += compilerFlags + " ";
    if (ccpSettings.compilerFlags)
    {
        compileCommandFirstHalf += compilerFlags + " ";
    }
    compileCommand += compilerTransitiveFlags + " ";
    if (ccpSettings.compilerTransitiveFlags)
    {
        compileCommandFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : compileDefinitions)
    {
        compileCommand += (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        if (ccpSettings.compileDefinitions)
        {
            compileCommandFirstHalf +=
                (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        }
    }

    for (const auto &i : includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.path) + " ");
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandFirstHalf.append(getIncludeFlag() +
                                           getReducedPath(i.path, ccpSettings.projectIncludeDirectories) + " ");
        }
    }

    for (const auto &i : environment.includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandFirstHalf.append(
                getIncludeFlag() +
                getReducedPath(i.directoryPath.generic_string(), ccpSettings.environmentIncludeDirectories) + " ");
        }
    }
}

void ParsedTarget::parseSourceDirectoriesAndFinalizeSourceFiles()
{
    for (const auto &i : sourceDirectories)
    {
        for (const auto &j : directory_iterator(i.sourceDirectory.directoryPath))
        {
            if (regex_match(j.path().filename().string(), regex(i.regex)))
            {
                sourceFiles.emplace_back(j.path().string());
            }
        }
    }
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
    /* from_json function of Node* did not work correctly, so not using it.*/
    // sourceNode.node = j.at("SRC_FILE").get<Node*>();
    // sourceNode.headerDependencies = j.at("HEADER_DEPENDENCIES").get<set<Node *>>();

    sourceNode.node = Node::getNodeFromString(j.at("SRC_FILE").get<string>());
    vector<string> headerDeps;
    headerDeps = j.at("HEADER_DEPENDENCIES").get<vector<string>>();
    for (const auto &h : headerDeps)
    {
        sourceNode.headerDependencies.insert(Node::getNodeFromString(h));
    }
    sourceNode.presentInCache = true;
}

SourceNode &BTargetCache::addNodeInSourceFileDependencies(const string &str)
{
    SourceNode sourceNode{.node = Node::getNodeFromString(str)};
    auto [pos, ok] = sourceFileDependencies.insert(sourceNode);
    if (ok)
    {
        pos->presentInCache = false;
    }
    else
    {
        pos->presentInCache = true;
    }
    pos->presentInSource = true;
    return const_cast<SourceNode &>(*pos);
}

bool BTargetCache::areFilesLeftToCompile() const
{
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            return true;
        }
    }
    return false;
}

bool BTargetCache::areAllFilesCompiled() const
{
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus != FileStatus::UPDATED)
        {
            return false;
        }
    }
    return true;
}

int BTargetCache::maximumThreadsNeeded() const
{
    int numberOfOutdatedFiles = 0;
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            ++numberOfOutdatedFiles;
        }
    }
    return numberOfOutdatedFiles;
}

SourceNode &BTargetCache::getNextNodeForCompilation()
{
    for (auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            return const_cast<SourceNode &>(sourceNode);
        }
    }
    cerr << "Internal Error: " << endl;
    cerr << "Should Not had reached here as when this function is called, we should have a sourcefile yet to be "
            "compiled."
         << endl;
    exit(-1);
}

void to_json(Json &j, const BTargetCache &bTargetCache)
{
    j["COMPILE_COMMAND"] = bTargetCache.compileCommand;
    j["DEPENDENCIES"] = bTargetCache.sourceFileDependencies;
}

void from_json(const Json &j, BTargetCache &bTargetCache)
{
    bTargetCache.compileCommand = j["COMPILE_COMMAND"];
    bTargetCache.sourceFileDependencies = j.at("DEPENDENCIES").get<set<SourceNode>>();
}
PostLinkOrArchive::PostLinkOrArchive(string commandFirstHalf, string printCommandFirstHalf,
                                     const string &buildCacheFilesDirPath, const string &fileName,
                                     const PathPrint &pathPrint, bool isTarget_)
    : isTarget{isTarget_}
{

    string str = isTarget ? "_t" : "";

    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;

    string outputFileName = (path(buildCacheFilesDirPath) / (fileName + "_output" + str)).generic_string();
    string errorFileName = (path(buildCacheFilesDirPath) / (fileName + "_error" + str)).generic_string();

    commandFirstHalf += "> " + addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);

    if (pathPrint.printLevel != PathPrintLevel::NO)
    {
        printCommandFirstHalf += "> " + getReducedPath(outputFileName, ccpSettings.outputAndErrorFiles) + " 2>" +
                                 getReducedPath(errorFileName, ccpSettings.outputAndErrorFiles);
    }

    printCommand = std::move(printCommandFirstHalf);

    commandFirstHalf = addQuotes(commandFirstHalf);
    if (system(commandFirstHalf.c_str()) == EXIT_SUCCESS)
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

void PostLinkOrArchive::executePrintRoutine(uint32_t color) const
{
    print(fg(static_cast<fmt::color>(color)), printCommand);
    print("\n");
    if (!commandSuccessOutput.empty())
    {
        print(fg(static_cast<fmt::color>(color)), commandSuccessOutput);
        print("\n");
    }
    if (!commandErrorOutput.empty())
    {
        print(fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), commandErrorOutput);
        print("\n");
    }
}

PostCompile::PostCompile(const ParsedTarget &parsedTarget_, string commandFirstHalf, string printCommandFirstHalf,
                         const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint)
    : parsedTarget{const_cast<ParsedTarget &>(parsedTarget_)},
      PostLinkOrArchive(std::move(commandFirstHalf), std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
                        pathPrint, false)
{
    isTarget = false;
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
    std::istringstream f(commandSuccessOutput);
    string line;
    vector<string> outputLines;
    while (std::getline(f, line))
    {
        outputLines.emplace_back(line);
    }
    string includeFileNote = "Note: including file:";

    for (auto iter = outputLines.begin(); iter != outputLines.end();)
    {
        if (iter->contains(includeFileNote))
        {
            size_t pos = iter->find_first_not_of(includeFileNote);
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
        else if (*iter == path(sourceNode.node->filePath).filename().string())
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
        treatedOutput.append(i);
        treatedOutput.append("\n");
    }
    commandSuccessOutput = treatedOutput;
}

void PostCompile::executePostCompileRoutineWithoutMutex(SourceNode &sourceNode)
{
    if (!successfullyCompleted)
    {
        return;
    }
    if (parsedTarget.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode);
    }
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

bool std::less<SourceNode>::operator()(const SourceNode &lhs, const SourceNode &rhs) const
{
    return std::less<Node *>{}(lhs.node, rhs.node);
}

Node *Node::getNodeFromString(const string &str)
{
    Node *tempNode = new Node{.filePath = str};

    if (auto [pos, ok] = allFiles.insert(tempNode); !ok)
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

void ParsedTarget::addAllSourceFilesInBuildNode(BTargetCache &bTargetCache)
{
    for (const auto &i : sourceFiles)
    {
        SourceNode &node = bTargetCache.addNodeInSourceFileDependencies(i);
        node.compilationStatus = FileStatus::NEEDS_RECOMPILE;
    }
}

void ParsedTarget::popularizeBuildTree(vector<BuildNode> &localBuildTree)
{
    for (auto it = localBuildTree.begin(); it != localBuildTree.end(); ++it)
    {
        if (it->target->outputDirectory == outputDirectory && it->target->actualOutputName == actualOutputName)
        {
            // If it exists before in the graph, move it to the end
            rotate(it, it + 1, localBuildTree.end());
            return;
        }
    }

    setCompileCommand();
    parseSourceDirectoriesAndFinalizeSourceFiles();

    BTargetCache bTargetCache;
    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        bTargetCache = targetCacheJson;
    }

    if (!exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
        addAllSourceFilesInBuildNode(bTargetCache);
    }
    else if (bTargetCache.compileCommand != compileCommand)
    {
        addAllSourceFilesInBuildNode(bTargetCache);
    }
    else
    {
        for (const auto &sourceFile : sourceFiles)
        {
            SourceNode &permSourceNode = bTargetCache.addNodeInSourceFileDependencies(sourceFile);
            permSourceNode.compilationStatus = FileStatus::UPDATED;

            path objectFilePath =
                path(buildCacheFilesDirPath + path(permSourceNode.node->filePath).filename().string() + ".o");

            if (!exists(objectFilePath))
            {
                permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                continue;
            }
            file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
            permSourceNode.node->checkIfNotUpdatedAndUpdate();
            if (permSourceNode.node->lastUpdateTime > objectFileLastEditTime)
            {
                permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            }
            else
            {
                for (auto &d : permSourceNode.headerDependencies)
                {
                    d->checkIfNotUpdatedAndUpdate();
                    if (d->lastUpdateTime > objectFileLastEditTime)
                    {
                        permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                        break;
                    }
                }
            }
        }
    }


    // Removing unused object-files from buildCacheFilesDirPath as they later affect linker command with *.o option.
    set<string> tmpObjectFiles;
    for (const auto &src : bTargetCache.sourceFileDependencies)
    {
        if (src.presentInSource == src.presentInCache)
        {
            tmpObjectFiles.insert(path(src.node->filePath).filename().string() + ".o");
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


    if (bTargetCache.maximumThreadsNeeded() > 0)
    {
        bTargetCache.compileCommand = compileCommand;
        relink = true;
        BuildNode buildNode;
        buildNode.target = this;
        buildNode.targetCache = bTargetCache;
        localBuildTree.emplace_back(buildNode);
    }
    else
    {
        relink = false;
        for (const auto &src : bTargetCache.sourceFileDependencies)
        {
            if (src.presentInSource != src.presentInCache)
            {
                relink = true;
            }
        }
        if(!relink && !exists(path(outputDirectory) / actualOutputName))
        {
            relink = true;
        }
        BuildNode buildNode;
        buildNode.target = this;
        buildNode.targetCache = bTargetCache;
        localBuildTree.emplace_back(buildNode);
    }
    // buildNode.target above is pointing to this. this is either a ParsedTarget in Builder::Builder() ParsedTargets or
    // a vector element of ParsedTarget::libraryDependenciesPTargets of one of these. Because these vector do not resize
    // after construction, it is safe to use. Otherwise, a vector<shared_ptr<ParsedTarget>>> can be used instead
    // for libraryDependeciesPTargets

    for (auto &i : libraryDependenciesBTargets)
    {
        i.popularizeBuildTree(localBuildTree);
    }
}

string ParsedTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c ";
    }
}

string ParsedTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string command;
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(path(sourceNode.node->filePath).generic_string(), ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags();
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? "/Fo" : "";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(path(buildCacheFilesDirPath).generic_string() + compileFileName + ".o",
                                  ccpSettings.objectFile) +
                   " ";
    }
    return command;
}

PostCompile ParsedTarget::Compile(SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = compileCommand + addQuotes(path(sourceNode.node->filePath).generic_string()) + " ";

    finalCompileCommand += getInfrastructureFlags();
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            "/Fo" + addQuotes(path(buildCacheFilesDirPath).generic_string() + compileFileName + ".o") + " ";
    }
    else
    {
    }

    PostCompile postCompile{*this,
                            finalCompileCommand,
                            compileCommandFirstHalf + getCompileCommandPrintSecondPart(sourceNode),
                            buildCacheFilesDirPath,
                            compileFileName,
                            settings.ccpSettings.outputAndErrorFiles};
    return postCompile;
}

PostLinkOrArchive ParsedTarget::Archive()
{
    ArchiveCommandPrintSettings acpSettings = settings.acpSettings;
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

    string archiveCommand = addQuotes(archiver.bTPath.make_preferred().string()) + " ";

    string archivePrintCommand;
    if (acpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand += getReducedPath(archiver.bTPath.make_preferred().string(), acpSettings.tool) + " ";
    }

    archiveCommand += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
    if (acpSettings.infrastructureFlags)
    {
        archivePrintCommand += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
    }

    archiveCommand += addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o") + " ";
    if (acpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand +=
            getReducedPath(path(buildCacheFilesDirPath / path("")).string() + "*.o", acpSettings.objectFiles) + " ";
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
    archiveCommand += getArchiveOutputFlag();
    if (acpSettings.infrastructureFlags)
    {
        archivePrintCommand += getArchiveOutputFlag();
    }

    archiveCommand += addQuotes(getLibraryPath()) + " ";
    if (acpSettings.archive.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand += getReducedPath(getLibraryPath(), acpSettings.archive) + " ";
    }

    PostLinkOrArchive postLinkOrArchive(archiveCommand, archivePrintCommand, buildCacheFilesDirPath, targetName,
                                        acpSettings.outputAndErrorFiles, true);
    return postLinkOrArchive;
}

PostLinkOrArchive ParsedTarget::Link()
{
    LinkCommandPrintSettings lcpSettings = settings.lcpSettings;
    string linkCommand = addQuotes(linker.bTPath.make_preferred().string()) + " ";

    string linkPrintCommand;
    if (lcpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath(linker.bTPath.make_preferred().string(), lcpSettings.tool) + " ";
    }

    linkCommand += linker.bTFamily == BTFamily::MSVC ? "/NOLOGO " : "";
    if (lcpSettings.infrastructureFlags)
    {
        linkPrintCommand += linker.bTFamily == BTFamily::MSVC ? "/NOLOGO " : "";
    }

    linkCommand += linkerFlags + " ";
    if (lcpSettings.linkerFlags)
    {
        linkPrintCommand += linkerFlags + " ";
    }

    linkCommand += linkerTransitiveFlags + " ";
    if (lcpSettings.linkerTransitiveFlags)
    {
        linkPrintCommand += linkerTransitiveFlags + " ";
    }

    linkCommand += addQuotes(buildCacheFilesDirPath + "*.o") + " ";
    if (lcpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath(buildCacheFilesDirPath + "*.o", lcpSettings.objectFiles) + " ";
    }

    auto getLinkFlag = [this](const std::string &libraryPath, const std::string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes(libraryPath + libraryName + ".lib") + " ";
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

    for (auto &i : libraryDependenciesBTargets)
    {
        linkCommand.append(getLinkFlag(i.outputDirectory, i.outputName));
    }

    auto getLinkFlagPrint = [this](const std::string &libraryPath, const std::string &libraryName,
                                   const PathPrint &pathPrint) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return getReducedPath(libraryPath + libraryName + ".lib", pathPrint) + " ";
        }
        else
        {
            return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) + " ";
        }
    };

    for (auto &i : libraryDependenciesBTargets)
    {
        linkPrintCommand.append(getLinkFlagPrint(i.outputDirectory, i.outputName, lcpSettings.libraryDependencies));
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
        linkCommand.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
        if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
        {
            linkPrintCommand.append(getLibraryDirectoryFlag() + getReducedPath(i, lcpSettings.libraryDirectories) +
                                    " ");
        }
    }

    for (const auto &i : environment.libraryDirectories)
    {
        linkCommand.append(getLibraryDirectoryFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
        if (lcpSettings.environmentLibraryDirectories.printLevel != PathPrintLevel::NO)
        {
            linkPrintCommand.append(
                getLibraryDirectoryFlag() +
                getReducedPath(i.directoryPath.generic_string(), lcpSettings.environmentLibraryDirectories) + " ");
        }
    }

    linkCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
    if (lcpSettings.infrastructureFlags)
    {
        linkPrintCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
    }

    linkCommand += addQuotes((path(outputDirectory) / actualOutputName).string());
    if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath((path(outputDirectory) / actualOutputName).string(), lcpSettings.binary);
    }

    /* for (const auto &i : libraryDependencies)
     {
         if (!i.preBuilt)
         {
             BTarget buildTarget(i.path);
             libraryDependenciesFlags.append(getLinkFlag(buildTarget.outputDirectory, buildTarget.outputName));
         }
         else
         {
             string dir = path(i.path).parent_path().string();
             string libName = path(i.path).filename().string();
             libName.erase(0, 3);
             libName.erase(libName.find('.'), 2);
             libraryDependenciesFlags.append(getLinkFlag(dir, libName));

             if (!i.imported)
             {
                 BPTarget(i.hmakeFilePath, i.path);
             }
         }
     }*/
    // string totalLinkerFlags = environment.linkerFlags + " " + linkerFlags + " " + linkerTransitiveFlags;

    PostLinkOrArchive postLinkOrArchive(linkCommand, linkPrintCommand, buildCacheFilesDirPath, targetName,
                                        lcpSettings.outputAndErrorFiles, true);
    return postLinkOrArchive;
}

BTargetType ParsedTarget::getTargetType()
{
    return targetType;
}

void ParsedTarget::pruneAndSaveBuildCache(BTargetCache &bTargetCache)
{
    // Prune the src_deps which were read from the older cache but are no longer the src_deps.
    const auto count = std::erase_if(bTargetCache.sourceFileDependencies, [](const auto &item) {
        // auto const& [key, value] = item;
        return !item.presentInSource;
    });
    Json cacheFileJson = bTargetCache;
    ofstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) << cacheFileJson.dump(4);
}

bool ParsedTarget::needsRelink() const
{
    if (relink)
    {
        return true;
    }
    for (const auto &i : libraryDependenciesBTargets)
    {
        if (i.needsRelink())
        {
            return true;
        }
    }
    return false;
}

using std::this_thread::get_id;
void Builder::actuallyBuild()
{
    oneAndOnlyMutex.lock();
    if (linkIterator->targetCache.areAllFilesCompiled() && !linkIterator->isLinking)
    {
        linkIterator->isLinking = true;
        oneAndOnlyMutex.unlock();
        unique_ptr<PostLinkOrArchive> postLinkOrArchive;
        if (linkIterator->target->getTargetType() == BTargetType::STATIC)
        {
            PostLinkOrArchive postLinkOrArchive1 = linkIterator->target->Archive();
            postLinkOrArchive = make_unique<PostLinkOrArchive>(postLinkOrArchive1);
        }
        else if (linkIterator->target->getTargetType() == BTargetType::EXECUTABLE)
        {
            PostLinkOrArchive postLinkOrArchive1 = linkIterator->target->Link();
            postLinkOrArchive = make_unique<PostLinkOrArchive>(postLinkOrArchive1);
        }
        linkIterator->target->pruneAndSaveBuildCache(linkIterator->targetCache);
        oneAndOnlyMutex.lock();
        if (linkIterator->target->getTargetType() == BTargetType::STATIC)
        {
            postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor);
        }
        else if (linkIterator->target->getTargetType() == BTargetType::EXECUTABLE)
        {
            postLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor);
        }

        ++linkIterator;
        if (linkIterator == buildTree.rend())
        {
            oneAndOnlyMutex.unlock();
            return;
        }
        oneAndOnlyMutex.unlock();
    }
    else
    {
        if (compileIterator == buildTree.rend())
        {
            oneAndOnlyMutex.unlock();
            return;
        }
        if (compileIterator->targetCache.areFilesLeftToCompile())
        {
            SourceNode &tempSourceNode = compileIterator->targetCache.getNextNodeForCompilation();
            tempSourceNode.compilationStatus = FileStatus::COMPILING;
            ParsedTarget *target = compileIterator->target;
            oneAndOnlyMutex.unlock();
            PostCompile postCompile = target->Compile(tempSourceNode);
            postCompile.executePostCompileRoutineWithoutMutex(tempSourceNode);
            oneAndOnlyMutex.lock();
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor);
            tempSourceNode.compilationStatus = FileStatus::UPDATED;
            oneAndOnlyMutex.unlock();
        }
        else
        {
            ++compileIterator;
            oneAndOnlyMutex.unlock();
        }
    }
    actuallyBuild();
}

Builder::Builder(const vector<string> &targetFilePaths, mutex &oneAndOnlyMutex) : oneAndOnlyMutex(oneAndOnlyMutex)
{
    vector<ParsedTarget> ParsedTargets;
    for (auto &t : targetFilePaths)
    {
        ParsedTargets.emplace_back(t);
    }
    for (auto &t : ParsedTargets)
    {
        t.executePreBuildCommands();
    }

    for (auto &t : ParsedTargets)
    {
        t.popularizeBuildTree(buildTree);
    }
    removeRedundantNodes(buildTree);

    compileIterator = buildTree.rbegin();
    linkIterator = buildTree.rbegin();

    std::size_t actionsNeeded = 0;
    for (const auto &buildNode : buildTree)
    {
        actionsNeeded += buildNode.targetCache.maximumThreadsNeeded() + 1;
    }
    size_t numberOfBuildThreads;
    actionsNeeded > buildThreadsAllowed ? numberOfBuildThreads = buildThreadsAllowed
                                        : numberOfBuildThreads = actionsNeeded;
    vector<thread *> buildThreads;

    // numberOfBuildThreads = 12;
    while (buildThreads.size() != numberOfBuildThreads)
    {
        buildThreads.emplace_back(new thread{&Builder::actuallyBuild, this});
    }

    for (auto t : buildThreads)
    {
        t->join();
        delete t;
    }
    // Build has been finished. Now we can copy stuff.

    // This is the actual build part. I am not doing it here.
    /*
        for (const auto &i : sourceFiles)
        {
            compileCommand += compiler.bTFamily == BTFamily::MSVC ? " /c " : " -c ";
            compileCommand += i;
            compileCommand += compiler.bTFamily == BTFamily::MSVC ? " /Fo" : " -o ";
            compileCommand += addQuotes((path(buildCacheFilesDirPath) / path(i).filename()).string()) + ".o" + "\"";

            cout << compileCommand << endl;
            int code = system(compileCommand.c_str());
            if (code != EXIT_SUCCESS)
            {
                exit(code);
            }
        }

        if (targetType == BTargetType::STATIC)
        {
            cout << "Archiving" << endl;
            string archiveCommand = "\"";
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                archiveCommand += archiver.bTPath.make_preferred().string() +
                                  " /OUT:" + addQuotes((path(outputDirectory) / actualOutputName).string()) + " " +
                                  addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ");
            }
            else if (archiver.bTFamily == BTFamily::GCC)
            {
                archiveCommand = archiver.bTPath.make_preferred().string() + " rcs " +
                                 addQuotes((path(outputDirectory) / actualOutputName).string()) + " " +
                                 addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ");
            }

            archiveCommand += "\"";
            cout << archiveCommand << endl;
            int code = system(archiveCommand.c_str());
            if (code != EXIT_SUCCESS)
            {
                exit(code);
            }
        }
        else
        {
            cout << "Linking" << endl;
            string linkerCommand;
            if (targetType == BTargetType::EXECUTABLE)
            {
                linkerCommand = "\"" + addQuotes(linker.bTPath.make_preferred().string()) + " " + totalLinkerFlags +
       " "
       + addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ") + " " + libraryDependenciesFlags +
       totalLibraryDirectoriesFlags; linkerCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
                linkerCommand += addQuotes((path(outputDirectory) / actualOutputName).string()) + "\"";
            }
            else
            {
            }

            cout << linkerCommand << endl;
            int code = system(linkerCommand.c_str());
            if (code != EXIT_SUCCESS)
            {
                exit(code);
            }

        }*/

    // cout << "Build Complete" << endl;

    /* if (copyPackage)
     {
         cout << "Copying Started" << endl;
         path copyFrom;
         path copyTo = packageTargetPath / "";
         if (targetType != BTargetType::SHARED)
         {
             copyFrom = path(outputDirectory) / actualOutputName;
         }
         else
         {
             // Shared Libraries Not Supported Yet.
         }
         copyFrom = copyFrom.lexically_normal();
         copyTo = copyTo.lexically_normal();
         cout << "Copying Target" << endl;
         cout << "Copying Target From " << copyFrom.string() << endl;
         cout << "Copying Target To " << copyTo.string() << endl;
         create_directories(copyTo);
         copy(copyFrom, copyTo, copy_options::update_existing);
         cout << "Target Copying Done" << endl;
         cout << "Copying Include Directories" << endl;
         for (auto &i : includeDirectories)
         {
             if (i.copy)
             {
                 path includeDirectoryCopyFrom = i.path;
                 path includeDirectoryCopyTo = packageTargetPath / "include";
                 includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
                 includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
                 cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << endl;
                 cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << endl;
                 create_directories(includeDirectoryCopyTo);
                 copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
                      copy_options::update_existing | copy_options::recursive);
                 cout << "IncludeDirectory Copying Done" << endl;
             }
         }
         string consumerTargetFileName;
         path consumerTargetFile = packageTargetPath / (targetName + ".hmake");
         ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
         cout << "Copying Completed" << endl;
     }

     if (!postBuildCustomCommands.empty())
     {
         cout << "Executing POST_BUILD_CUSTOM_COMMANDS" << endl;
     }
     for (auto &i : postBuildCustomCommands)
     {
         cout << i << endl;
         if (int a = system(i.c_str()); a != EXIT_SUCCESS)
         {
             exit(a);
         }
     }*/
}

vector<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at("TARGETS").get<vector<string>>();
}

vector<string> Builder::getTargetFilePathsFromProjectFile(const string &fileName)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec = projectFileJson.at("VARIANTS").get<vector<string>>();
    vector<string> targetFilePaths;
    for (auto &i : vec)
    {
        vector<string> variantTargets = getTargetFilePathsFromVariantFile(i + "/projectVariant.hmake");
        targetFilePaths.insert(targetFilePaths.end(), variantTargets.begin(), variantTargets.end());
    }
    return targetFilePaths;
}

void Builder::removeRedundantNodes(vector<BuildNode> &buildTree)
{
    auto it = buildTree.begin();

    while (it != buildTree.end())
    {

        if (!it->target->needsRelink())
        {

            it = buildTree.erase(it);
        }
        else
            ++it;
    }
}

BPTarget::BPTarget(const string &targetFilePath, const path &copyFrom)
{
    string targetName;
    string compilerFlags;
    vector<BLibraryDependency> libraryDependencies;
    vector<BIDD> includeDirectories;
    vector<BCompileDefinition> compileDefinitions;
    string targetFileName;
    Json consumerDependenciesJson;
    path packageTargetPath;
    bool packageMode;
    bool copyPackage;
    string packageName;
    string packageCopyPath;
    int packageVariantIndex;

    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

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
            packageTargetPath = packageCopyPath / path(packageName) / path(to_string(packageVariantIndex)) / targetName;
        }
    }
    else
    {
        copyPackage = false;
    }

    libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<vector<BLibraryDependency>>();
    if (packageMode)
    {
        includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<BIDD>>();
    }

    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
    }

    for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            //  BTarget buildTarget(i.path);
        }
        else
        {
            if (!i.imported)
            {
                BPTarget(i.hmakeFilePath, copyFrom);
            }
        }
    }
    if (copyPackage)
    {
        cout << "Copying Started" << endl;
        path copyTo = packageTargetPath / "";
        copyTo = copyTo.lexically_normal();
        cout << "Copying Target" << endl;
        cout << "Copying Target From " << copyFrom.string() << endl;
        cout << "Copying Target To " << copyTo.string() << endl;
        create_directories(copyTo);
        copy(copyFrom, copyTo, copy_options::update_existing);
        cout << "Target Copying Done" << endl;
        cout << "Copying Include Directories" << endl;
        for (auto &i : includeDirectories)
        {
            if (i.copy)
            {
                path includeDirectoryCopyFrom = i.path;
                path includeDirectoryCopyTo = packageTargetPath / "include";
                includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
                includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
                cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << endl;
                cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << endl;
                create_directories(includeDirectoryCopyTo);
                copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
                     copy_options::update_existing | copy_options::recursive);
                cout << "IncludeDirectory Copying Done" << endl;
            }
        }
        string consumerTargetFileName;
        path consumerTargetFile = packageTargetPath / (targetName + ".hmake");
        ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
        cout << "Copying Completed" << endl;
    }
}

BPackage::BPackage(const path &packageFilePath)
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
    for (auto &variant : variants)
    {
        string integerIndex = variant.at("INDEX").get<string>();
        path packageVariantFilePath = current_path() / integerIndex / "packageVariant.hmake";
        if (!is_regular_file(packageVariantFilePath))
        {
            cerr << packageVariantFilePath.string() << " is not a regular file";
        }
        // BVariant{packageVariantFilePath};
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

    auto nthOccurrence = [](const std::string &str, const std::string &findMe, size_t nth) -> size_t {
        size_t pos = 0;
        int cnt = 0;

        while (cnt != nth)
        {
            pos += 1;
            pos = str.find(findMe, pos);
            if (pos == std::string::npos)
                return -1;
            cnt++;
        }
        return pos;
    };

    auto countSubstring = [](const std::string &str, const std::string &sub) -> int {
        if (sub.length() == 0)
            return 0;
        int count = 0;
        for (size_t offset = str.find(sub); offset != std::string::npos; offset = str.find(sub, offset + sub.length()))
        {
            ++count;
        }
        return count;
    };

    string str = subjectPath;
    int finalDepth = pathPrint.depth;
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
    return str.substr(index + 1, str.size() - 1);
}
