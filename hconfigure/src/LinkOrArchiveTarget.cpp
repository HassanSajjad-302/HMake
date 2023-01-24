#include "LinkOrArchiveTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <utility>

using std::ofstream, std::filesystem::create_directory, std::ifstream;

LinkOrArchiveTarget::LinkOrArchiveTarget(string name, TargetType targetType)
    : BTarget(ResultType::LINK), CTarget(std::move(name))
{
    cTargetType = targetType;
    // TODO
    if (cache.isLinkerInVSToolsArray)
    {
        linker = toolsCache.vsTools[cache.selectedLinkerArrayIndex].linker;
        libraryDirectoriesStandard = toolsCache.vsTools[cache.selectedLinkerArrayIndex].libraryDirectories;
    }
    else
    {
        linker = toolsCache.linkers[cache.selectedLinkerArrayIndex];
    }
    if (cache.isArchiverInVSToolsArray)
    {
        archiver = toolsCache.vsTools[cache.selectedArchiverArrayIndex].archiver;
    }
    else
    {
        archiver = toolsCache.archivers[cache.selectedArchiverArrayIndex];
    }
}

void LinkOrArchiveTarget::initializeForBuild()
{
    if (outputName.empty())
    {
        outputName = name;
    }
    if (outputDirectory.empty())
    {
        outputDirectory = targetFileDir;
    }
    /* targetFilePath = (path(targetFileDir) / name).generic_string();
     string fileName = path(targetFilePath).filename().string();*/
    bTargetType = cTargetType;
    buildCacheFilesDirPath = (path(targetFileDir) / ("Cache_Build_Files/")).generic_string();
    // Parsing finished

    actualOutputName = getActualNameFromTargetName(bTargetType, os, name);

    for (const LinkOrArchiveTarget *linkOrArchiveTarget : publicLibs)
    {
        addDependency(const_cast<LinkOrArchiveTarget &>(*linkOrArchiveTarget));
    }
}

string &LinkOrArchiveTarget::getLinkOrArchiveCommand()
{
    if (linkOrArchiveCommand.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }
    return linkOrArchiveCommand;
}

string &LinkOrArchiveTarget::getLinkOrArchiveCommandPrintFirstHalf()
{
    if (linkOrArchiveCommandPrintFirstHalf.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }

    return linkOrArchiveCommandPrintFirstHalf;
}

void LinkOrArchiveTarget::updateBTarget()
{
    if (bTargetType == TargetType::LIBRARY_STATIC)
    {
        postBasicLinkOrArchive = std::make_shared<PostBasic>(Archive());
    }
    else if (bTargetType == TargetType::EXECUTABLE || bTargetType == TargetType::LIBRARY_SHARED)
    {
        postBasicLinkOrArchive = std::make_shared<PostBasic>(Link());
    }
    if (postBasicLinkOrArchive->successfullyCompleted)
    {
        pruneAndSaveBuildCache(postBasicLinkOrArchive->successfullyCompleted);
    }
}

void LinkOrArchiveTarget::printMutexLockRoutine()
{
    if (bTargetType == TargetType::LIBRARY_STATIC)
    {
        postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, false);
    }
    else if (bTargetType == TargetType::EXECUTABLE)
    {
        postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, false);
    }
}

void LinkOrArchiveTarget::setJson()
{
    vector<Json> dependenciesArray;

    // TODO
    auto iterateOverLibraries = [&dependenciesArray](const set<LinkOrArchiveTarget *> &libraries) {
        for (LinkOrArchiveTarget *library : libraries)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = false;
            libDepObject[JConsts::path] = library->getTargetPointer();

            dependenciesArray.emplace_back(libDepObject);
        }
    };

    auto iterateOverPrebuiltLibs = [&dependenciesArray](const set<LinkOrArchiveTarget *> &prebuiltLibs) {
        for (LinkOrArchiveTarget *pLibrary : prebuiltLibs)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = true;
            // TODO
            libDepObject[JConsts::path] = pLibrary->getTargetPointer();
            dependenciesArray.emplace_back(libDepObject);
        }
    };

    iterateOverLibraries(publicLibs);
    iterateOverLibraries(privateLibs);
    iterateOverPrebuiltLibs(publicPrebuilts);
    iterateOverPrebuiltLibs(privatePrebuilts);

    json[JConsts::targetType] = cTargetType;
    json[JConsts::outputName] = outputName;
    json[JConsts::outputDirectory] = outputDirectory;
    json[JConsts::linker] = linker;
    json[JConsts::archiver] = archiver;
    json[JConsts::linkerFlags] = publicLinkerFlags;
    json[JConsts::cppSourceTargets] = cppSourceTarget->json;
    json[JConsts::libraryDependencies] = dependenciesArray;
    // TODO
    json[JConsts::linkerTransitiveFlags] = publicLinkerFlags;
    /*    if (moduleScope)
            {
                targetFileJson[JConsts::moduleScope] = moduleScope->getTargetFilePath(configurationName);
            }
            else
            {
                print(stderr, "Module Scope not assigned\n");
            }
            if (configurationScope)
            {
                targetFileJson[JConsts::configurationScope] = configurationScope->getTargetFilePath(configurationName);
            }
            else
            {
                print(stderr, "Configuration Scope not assigned\n");
            }*/
}

BTarget *LinkOrArchiveTarget::getBTarget()
{
    return this;
}

PostBasic LinkOrArchiveTarget::Archive()
{
    return PostBasic(archiver, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(),
                     buildCacheFilesDirPath, name, settings.acpSettings.outputAndErrorFiles, true);
}

PostBasic LinkOrArchiveTarget::Link()
{
    return PostBasic(linker, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(), buildCacheFilesDirPath,
                     name, settings.lcpSettings.outputAndErrorFiles, true);
}

void LinkOrArchiveTarget::populateCTargetDependencies()
{
    addCTargetDependency(publicLibs);
    addCTargetDependency(privateLibs);
}

void LinkOrArchiveTarget::addPrivatePropertiesToPublicProperties()
{
    publicLibs.insert(privateLibs.begin(), privateLibs.end());
    for (LinkOrArchiveTarget *library : publicLibs)
    {
        publicLibs.insert(library->publicLibs.begin(), library->publicLibs.end());
        publicPrebuilts.insert(library->publicPrebuilts.begin(), library->publicPrebuilts.end());
    }
    cppSourceTarget->publicCompilerFlags += cppSourceTarget->privateCompilerFlags;

    // TODO: Following is added to compile the examples. However, the code should also add other public
    // properties from public dependencies
    // Modify PLibrary to be dependent of Dependency, thus having public properties.
    for (LinkOrArchiveTarget *lib : publicLibs)
    {
        for (const Node *idd : lib->cppSourceTarget->publicIncludes)
        {
            cppSourceTarget->publicIncludes.emplace_back(idd);
        }
        for (const Node *idd : lib->cppSourceTarget->publicHUIncludes)
        {
            cppSourceTarget->publicIncludes.emplace_back(idd);
        }
        cppSourceTarget->publicCompilerFlags += lib->cppSourceTarget->publicCompilerFlags;
    }
}

void LinkOrArchiveTarget::pruneAndSaveBuildCache(const bool successful)
{
    Json cacheFileJson;
    cacheFileJson[cppSourceTarget->name] = cppSourceTarget->getBuildCache(successful);
    ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << cacheFileJson.dump(4);
}

void LinkOrArchiveTarget::setLinkOrArchiveCommandAndPrint()
{
    const ArchiveCommandPrintSettings &acpSettings = settings.acpSettings;
    const LinkCommandPrintSettings &lcpSettings = settings.lcpSettings;

    if (bTargetType == TargetType::LIBRARY_STATIC)
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

        linkOrArchiveCommand += publicLinkerFlags + " ";
        if (lcpSettings.linkerFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += publicLinkerFlags + " ";
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

    vector<SourceNode *> sourceNodes;
    vector<LinkOrArchiveTarget *> targets;

    for (auto it = allDependencies.rbegin(); it != allDependencies.rend(); ++it)
    {
        BTarget *dependency = *it;
        if (auto *smFile = dynamic_cast<SMFile *>(dependency))
        {
            if (cppSourceTarget->moduleSourceFileDependencies.contains(*smFile))
            {
                sourceNodes.emplace_back(smFile);
            }
        }
        else if (auto *sourceNode = dynamic_cast<SourceNode *>(dependency))
        {
            if (cppSourceTarget->sourceFileDependencies.contains(*sourceNode))
            {
                sourceNodes.emplace_back(sourceNode);
            }
        }
        else if (auto *target = dynamic_cast<LinkOrArchiveTarget *>(dependency); target)
        {
            targets.emplace_back(target);
        }
    }

    for (SourceNode *sourceNode : sourceNodes)
    {
        const PathPrint *pathPrint;
        if (bTargetType == TargetType::LIBRARY_STATIC)
        {
            pathPrint = &(acpSettings.objectFiles);
        }
        else
        {
            pathPrint = &(lcpSettings.objectFiles);
        }
        string outputFilePath = sourceNode->getOutputFilePath();
        linkOrArchiveCommand += addQuotes(outputFilePath) + " ";
        if (pathPrint->printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf += getReducedPath(outputFilePath, *pathPrint) + " ";
        }
    }

    for (LinkOrArchiveTarget *target : targets)
    {
        if (bTargetType != TargetType::LIBRARY_STATIC)
        {
            linkOrArchiveCommand += getLinkFlag(target->outputDirectory, target->outputName);
            linkOrArchiveCommandPrintFirstHalf +=
                getLinkFlagPrint(target->outputDirectory, target->outputName, lcpSettings.libraryDependencies);
        }
    }

    // HMake does not link any dependency to static library
    if (bTargetType != TargetType::LIBRARY_STATIC)
    {
        // TODO
        // Not doing prebuilt libraries yet

        /*        for (auto &i : libraryDependencies)
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
                }*/

        auto getLibraryDirectoryFlag = [this]() {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            else
            {
                return "-L";
            }
        };

        for (const Node *includeDir : libraryDirectories)
        {
            linkOrArchiveCommand += getLibraryDirectoryFlag() + addQuotes(includeDir->filePath) + " ";
            if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
            {
                linkOrArchiveCommandPrintFirstHalf +=
                    getLibraryDirectoryFlag() + getReducedPath(includeDir->filePath, lcpSettings.libraryDirectories) +
                    " ";
            }
        }

        for (const string &str : libraryDirectoriesStandard)
        {
            linkOrArchiveCommand += getLibraryDirectoryFlag() + addQuotes(str) + " ";
            if (lcpSettings.environmentLibraryDirectories.printLevel != PathPrintLevel::NO)
            {
                linkOrArchiveCommandPrintFirstHalf +=
                    getLibraryDirectoryFlag() + getReducedPath(str, lcpSettings.environmentLibraryDirectories) + " ";
            }
        }

        linkOrArchiveCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrintFirstHalf += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        }

        linkOrArchiveCommand += addQuotes((path(outputDirectory) / actualOutputName).string()) + " ";
        if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf +=
                getReducedPath((path(outputDirectory) / actualOutputName).string(), lcpSettings.binary) + " ";
        }

        if (bTargetType == TargetType::LIBRARY_SHARED)
        {
            linkOrArchiveCommand += "/DLL";
            if (lcpSettings.infrastructureFlags)
            {
                linkOrArchiveCommandPrintFirstHalf += "/DLL";
            }
        }
    }
}

void LinkOrArchiveTarget::checkForPreBuiltAndCacheDir()
{
    checkForRelinkPrebuiltDependencies();
    if (!std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
    }

    if (std::filesystem::exists(path(buildCacheFilesDirPath) / (name + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (name + ".cache")) >> targetCacheJson;
        cppSourceTarget->checkForPreBuiltAndCacheDir(targetCacheJson[cppSourceTarget->name]);
    }
}

void LinkOrArchiveTarget::checkForRelinkPrebuiltDependencies()
{
    path outputPath = path(outputDirectory) / actualOutputName;
    if (!std::filesystem::exists(outputPath))
    {
        fileStatus = FileStatus::NEEDS_UPDATE;
    }
    // TODO
    // Not doing prebuilt libraries yet

    /*    for (const auto &i : libraryDependencies)
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
        }*/
}
