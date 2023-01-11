#include "Target.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>

using std::filesystem::create_directory, std::filesystem::file_time_type, std::filesystem::recursive_directory_iterator,
    std::ifstream, std::ofstream, std::regex, std::regex_error;

SourceDirectory::SourceDirectory(string sourceDirectory_, string regex_)
    : sourceDirectory{Node::getNodeFromString(sourceDirectory_, false)}, regex{std::move(regex_)}
{
}

void SourceDirectory::populateSourceOrSMFiles(class Target &target, bool sourceFiles)
{
}

void to_json(Json &j, const SourceDirectory &sourceDirectory)
{
    j[JConsts::path] = sourceDirectory.sourceDirectory;
    j[JConsts::regexString] = sourceDirectory.regex;
}

bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs)
{
    return lhs.sourceDirectory < rhs.sourceDirectory || lhs.regex < rhs.regex;
}

SourceNode &Target::addNodeInSourceFileDependencies(const string &str)
{
    auto [pos, ok] = sourceFileDependencies.emplace(str, this, ResultType::SOURCENODE);
    auto &sourceNode = const_cast<SourceNode &>(*pos);
    sourceNode.presentInSource = true;
    return sourceNode;
}

Target::Target(string targetName_, const bool initializeFromCache)
    : targetName(std::move(targetName_)), CTarget{targetName_}, BTarget(this, ResultType::LINK)
{
    if (initializeFromCache)
    {
        initializeFromCacheFunc();
    }
}

Target::Target(string targetName_, Variant &variant)
    : targetName(std::move(targetName_)), outputName(targetName), CTarget{targetName_, variant},
      BTarget(this, ResultType::LINK)
{
    static_cast<Features &>(*this) = static_cast<const Features &>(variant);
    isTarget = true;
}

void Target::setPropertiesFlags() const
{
    // TODO:
    //-Wl and --start-group options are needed because gcc command-line is used. But the HMake approach has been to
    // differentiate in compiler and linker
    //  TODO: setTargetForAndOr is not being called as this target is marked const
    //  Notes
    //   For Windows NT, we use a different JAMSHELL
    //   Currently, I ain't caring for the propagated properties. These properties are only being
    //   assigned to the parent.
    //   TODO s
    //   262 Be caustious of this rc-type. It has something to do with Windows resource compiler
    //   which I don't know
    //   TODO: flavor is being assigned based on the -dumpmachine argument to the gcc command. But this
    //    is not yet catered here.
    //

    // Calls to toolset.flags are being copied.
    // OPTIONS variable is being updated for rule gcc.compile and gcc.link action.
    string compilerFlags;
    string linkerFlags;
    string compilerAndLinkerFlags;
    // FINDLIBS-SA variable is being updated gcc.link rule.
    string findLibsSA;
    // OPTIONS variable is being updated for gcc.compile.c++ rule actions.
    string cppCompilerFlags;
    // DEFINES variable is being updated for gcc.compile.c++ rule actions.
    string cppDefineFlags;
    // OPTIONS variable is being updated for rule gcc.compile.c++ and gcc.link action.
    string cppCompilerAndLinkerFlags;
    // .IMPLIB-COMMAND variable is being updated for rule gcc.link.DLL action.
    string implibCommand;

    auto isTargetBSD = [&]() { return OR(TargetOS::BSD, TargetOS::FREEBSD, TargetOS::NETBSD, TargetOS::NETBSD); };
    {
        // -fPIC
        if (cTargetType == TargetType::LIBRARY_STATIC || cTargetType == TargetType::LIBRARY_SHARED)
        {
            if (targetOs != TargetOS::WINDOWS || targetOs != TargetOS::CYGWIN)
            {
            }
        }
    }
    {
        // Handle address-model
        if (targetOs == TargetOS::AIX && addModel == AddressModel::A_32)
        {
            compilerAndLinkerFlags += "-maix32";
        }
        if (targetOs == TargetOS::AIX && addModel == AddressModel::A_64)
        {
            compilerAndLinkerFlags += "-maix64";
        }
        if (targetOs == TargetOS::HPUX && addModel == AddressModel::A_32)
        {
            compilerAndLinkerFlags += "-milp32";
        }
        if (targetOs == TargetOS::HPUX && addModel == AddressModel::A_64)
        {
            compilerAndLinkerFlags += "-milp64";
        }
    }
    {
        // Handle threading
        if (EVALUATE(Threading::MULTI))
        {
            if (OR(TargetOS::WINDOWS, TargetOS::CYGWIN, TargetOS::SOLARIS))
            {
                compilerAndLinkerFlags += "-mthreads";
            }
            else if (targetOs == TargetOS::QNX || isTargetBSD())
            {
                compilerAndLinkerFlags += "-pthread";
            }
            else if (targetOs == TargetOS::SOLARIS)
            {
                compilerAndLinkerFlags += "-pthreads";
                findLibsSA += "rt";
            }
            else if (!OR(TargetOS::ANDROID, TargetOS::HAIKU, TargetOS::SGI, TargetOS::DARWIN, TargetOS::VXWORKS,
                         TargetOS::IPHONE, TargetOS::APPLETV))
            {
                compilerAndLinkerFlags += "-pthread";
                findLibsSA += "rt";
            }
        }
    }
    {
        CxxSTDDialect dCxxStdDialects = CxxSTDDialect::MS;
        auto setCppStdAndDialectCompilerAndLinkerFlags = [&](CxxSTD cxxStdLocal) {
            cppCompilerAndLinkerFlags += cxxStdDialect == CxxSTDDialect::GNU ? "-std=gnu++" : "-std=c++";
            CxxSTD temp = cxxStd;
            const_cast<CxxSTD &>(cxxStd) = cxxStdLocal;
            cppCompilerAndLinkerFlags += GET_FLAG_EVALUATE(
                CxxSTD::V_98, "98", CxxSTD::V_03, "03", CxxSTD::V_0x, "0x", CxxSTD::V_11, "11", CxxSTD::V_1y, "1y",
                CxxSTD::V_14, "14", CxxSTD::V_1z, "1z", CxxSTD::V_17, "17", CxxSTD::V_2a, "2a", CxxSTD::V_20, "20",
                CxxSTD::V_2b, "2b", CxxSTD::V_23, "23", CxxSTD::V_2c, "2c", CxxSTD::V_26, "26");
            const_cast<CxxSTD &>(cxxStd) = temp;
        };

        if (EVALUATE(CxxSTD::V_LATEST))
        {
            // Rule at Line 429
            /*            if (toolSet.version >= Version{10})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_20);
                        }
                        else if (toolSet.version >= Version{8})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_2a);
                        }
                        else if (toolSet.version >= Version{6})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_17);
                        }
                        else if (toolSet.version >= Version{5})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1z);
                        }
                        else if (toolSet.version >= Version{4, 9})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_14);
                        }
                        else if (toolSet.version >= Version{4, 8})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1y);
                        }
                        else if (toolSet.version >= Version{4, 7})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_11);
                        }
                        else if (toolSet.version >= Version{3, 3})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_98);
                        }*/
        }
        else
        {
            setCppStdAndDialectCompilerAndLinkerFlags(cxxStd);
        }
    }
    // From line 423 to line 625 as no library is using pch or obj

    // General options, link optimizations

    compilerFlags += GET_FLAG_EVALUATE(Optimization::OFF, "-O0", Optimization::SPEED, "-O3", Optimization::SPACE, "-Os",
                                       Optimization::MINIMAL, "-O1", Optimization::DEBUG, "-Og");

    compilerFlags += GET_FLAG_EVALUATE(Inlining::OFF, "-fno-inline", Inlining::ON, "-Wno-inline", Inlining::FULL,
                                       "-finline-functions -Wno-inline");

    compilerFlags += GET_FLAG_EVALUATE(Warnings::OFF, "-w", Warnings::ON, "-Wall", Warnings::ALL, "-Wall",
                                       Warnings::EXTRA, "-Wall -Wextra", Warnings::PEDANTIC, "-Wall -Wextra -pedantic");
    compilerFlags += GET_FLAG_EVALUATE(WarningsAsErrors::ON, "-Werror");

    compilerFlags += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g");
    compilerFlags += GET_FLAG_EVALUATE(Profiling::ON, "-pg");

    compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden");
    cppCompilerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility-inlines-hidden");
    if (!EVALUATE(TargetOS::DARWIN))
    {
        compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected");
    }
    compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::GLOBAL, "-fvisibility=default");

    cppCompilerFlags += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "-fno-exceptions");
    cppCompilerFlags += GET_FLAG_EVALUATE(RTTI::OFF, "-fno-rtti");

    // Sanitizers
    string sanitizerFlags;
    sanitizerFlags += GET_FLAG_EVALUATE(AddressSanitizer::ON, "-fsanitize=address -fno-omit-frame-pointer",
                                        AddressSanitizer::NORECOVER,
                                        "-fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer");
    sanitizerFlags +=
        GET_FLAG_EVALUATE(LeakSanitizer::ON, "-fsanitize=leak -fno-omit-frame-pointer", LeakSanitizer::NORECOVER,
                          "-fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer");
    sanitizerFlags +=
        GET_FLAG_EVALUATE(ThreadSanitizer::ON, "-fsanitize=thread -fno-omit-frame-pointer", ThreadSanitizer::NORECOVER,
                          "-fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer");
    sanitizerFlags += GET_FLAG_EVALUATE(UndefinedSanitizer::ON, "-fsanitize=undefined -fno-omit-frame-pointer",
                                        UndefinedSanitizer::NORECOVER,
                                        "-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer");
    sanitizerFlags += GET_FLAG_EVALUATE(Coverage::ON, "--coverage");

    cppCompilerFlags += sanitizerFlags;
    // I don't understand the following comment at line 667:
    // # configure Dinkum STL to match compiler options
    if (EVALUATE(TargetOS::VXWORKS))
    {
        cppDefineFlags += GET_FLAG_EVALUATE(RTTI::OFF, "_NO_RTTI");
        cppDefineFlags += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "_NO_EX=1");
    }

    // LTO
    // compilerAndLinkerFlags += GET_FLAG_EVALUATE()
    if (EVALUATE(LTO::ON))
    {
        compilerAndLinkerFlags += GET_FLAG_EVALUATE(LTOMode::FULL, "-flto");
        compilerFlags += GET_FLAG_EVALUATE(LTOMode::FAT, "-flto -ffat-lto-objects");
        linkerFlags += GET_FLAG_EVALUATE(LTOMode::FAT, "-flto");
    }

    // ABI selection
    cppDefineFlags +=
        GET_FLAG_EVALUATE(StdLib::GNU, "_GLIBCXX_USE_CXX11_ABI=0", StdLib::GNU11, "_GLIBCXX_USE_CXX11_ABI=1");

    {
        bool noStaticLink = true;
        if (OR(TargetOS::WINDOWS, TargetOS::VMS))
        {
            noStaticLink = false;
        }
        if (noStaticLink && EVALUATE(RuntimeLink::STATIC))
        {
            if (EVALUATE(Link::SHARED))
            {
                print("WARNING: On gcc, DLLs can not be built with <runtime-link>static\n");
            }
            else
            {
                // TODO:  Line 718
                //  Implement the remaining rule
            }
        }
    }

    // Linker Flags

    linkerFlags += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g");
    linkerFlags += GET_FLAG_EVALUATE(Profiling::ON, "-pg");

    linkerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden -fvisibility-inlines-hidden",
                                     LocalVisibility::GLOBAL, "-fvisibility=default");
    if (!EVALUATE(TargetOS::DARWIN))
    {
        linkerFlags += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected");
    }

    // Sanitizers
    // Though sanitizer flags for compiler are assigned at different location, and are for c++, but are exact same
    linkerFlags += sanitizerFlags;
    linkerFlags += GET_FLAG_EVALUATE(Coverage::ON, "--coverage");

    // Target Specific Flags
    // AIX
    /* On AIX we *have* to use the native linker.

   The -bnoipath strips the prepending (relative) path of libraries from
   the loader section in the target library or executable. Hence, during
   load-time LIBPATH (identical to LD_LIBRARY_PATH) or a hard-coded
   -blibpath (*similar* to -lrpath/-lrpath-link) is searched. Without
   this option, the prepending (relative) path + library name is
   hard-coded in the loader section, causing *only* this path to be
   searched during load-time. Note that the AIX linker does not have an
   -soname equivalent, this is as close as it gets.

   The -bbigtoc option instrcuts the linker to create a TOC bigger than 64k.
   This is necessary for some submodules such as math, but it does make running
   the tests a tad slower.

   The above options are definitely for AIX 5.x, and most likely also for
   AIX 4.x and AIX 6.x. For details about the AIX linker see:
   http://download.boulder.ibm.com/ibmdl/pub/software/dw/aix/es-aix_ll.pdf
   */
    linkerFlags += GET_FLAG_EVALUATE(TargetOS::AIX, "-Wl -bnoipath -Wl -bbigtoc");
    linkerFlags += AND(TargetOS::AIX, RuntimeLink::STATIC) ? "-static" : "";
    //  On Darwin, the -s option to ld does not work unless we pass -static,
    // and passing -static unconditionally is a bad idea. So, do not pass -s
    // at all and darwin.jam will use a separate 'strip' invocation.

    // 866
}

void Target::initializeForBuild()
{

    if (outputDirectory.empty())
    {
        outputDirectory = targetFileDir;
    }
    targetFilePath = (path(targetFileDir) / name).generic_string();
    string fileName = path(targetFilePath).filename().string();
    targetName = name;
    bTargetType = cTargetType;
    if (bTargetType != TargetType::EXECUTABLE && bTargetType != TargetType::LIBRARY_STATIC &&
        bTargetType != TargetType::LIBRARY_SHARED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), "Unsupported BTarget {}.",
              getTargetPointer());
        exit(EXIT_FAILURE);
    }

    auto parseRegexSourceDirs = [target = this](bool assignToSourceNodes) {
        for (const SourceDirectory &srcDir : assignToSourceNodes ? target->regexSourceDirs : target->regexModuleDirs)
        {
            for (const auto &k : recursive_directory_iterator(path(srcDir.sourceDirectory->filePath)))
            {
                try
                {
                    if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(srcDir.regex)))
                    {
                        if (assignToSourceNodes)
                        {
                            target->sourceFileDependencies.emplace(k.path().generic_string(), target,
                                                                   ResultType::SOURCENODE);
                        }
                        else
                        {
                            target->moduleSourceFileDependencies.emplace(k.path().generic_string(), target);
                        }
                    }
                }
                catch (const std::regex_error &e)
                {
                    print(stderr, "regex_error : {}\nError happened while parsing regex {} of target{}\n", e.what(),
                          srcDir.regex, target->getTargetPointer());
                    exit(EXIT_FAILURE);
                }
            }
        }
    };
    parseRegexSourceDirs(true);
    parseRegexSourceDirs(false);
    buildCacheFilesDirPath = (path(targetFilePath).parent_path() / ("Cache_Build_Files/")).generic_string();
    // Parsing finished

    actualOutputName = getActualNameFromTargetName(bTargetType, os, targetName);
    auto [pos, Ok] = variantFilePaths.emplace(path(targetFilePath).parent_path().parent_path().string() + "/" +
                                              "projectVariant.hmake");
    variantFilePath = &(*pos);
    // TODO
    /*    if (!sourceFiles.empty() && !modulesSourceFiles.empty())
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "A Target can not have both module-source and regular-source. You "
                  "can make regular-source dependency "
                  "of module source.\nTarget: {}.\n",
                  targetFilePath);
            exit(EXIT_FAILURE);
        }*/
}

void Target::setJsonDerived()
{
    vector<Json> dependenciesArray;

    // TODO
    auto iterateOverLibraries = [&dependenciesArray](const set<Library *> &libraries) {
        for (Library *library : libraries)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = false;
            libDepObject[JConsts::path] = library->getTargetPointer();

            dependenciesArray.emplace_back(libDepObject);
        }
    };

    auto iterateOverPrebuiltLibs = [&dependenciesArray](const set<PLibrary *> &prebuiltLibs) {
        for (PLibrary *pLibrary : prebuiltLibs)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = true;
            libDepObject[JConsts::path] = pLibrary->libraryPath.generic_string();
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
    json[JConsts::configuration] = configurationType;
    json[JConsts::compiler] = compiler;
    json[JConsts::linker] = linker;
    json[JConsts::archiver] = archiver;
    json[JConsts::environment] = environment;
    json[JConsts::compilerFlags] = publicCompilerFlags;
    json[JConsts::linkerFlags] = publicLinkerFlags;
    string str = "SOURCE_";
    json[str + JConsts::files] = sourceFileDependencies;
    json[str + JConsts::directories] = regexSourceDirs;
    str = "MODULE_";
    json[str + JConsts::files] = moduleSourceFileDependencies;
    json[str + JConsts::directories] = regexModuleDirs;
    json[JConsts::libraryDependencies] = dependenciesArray;
    json[JConsts::includeDirectories] = publicIncludes;
    json[JConsts::huIncludeDirectories] = publicHUIncludes;
    // TODO
    json[JConsts::compilerTransitiveFlags] = publicCompilerFlags;
    json[JConsts::linkerTransitiveFlags] = publicLinkerFlags;
    json[JConsts::compileDefinitions] = publicCompileDefinitions;
    json[JConsts::variant] = JConsts::project;
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

Target &Target::PUBLIC_COMPILER_FLAGS(const string &compilerFlags)
{
    publicCompilerFlags += compilerFlags;
    return *this;
}

Target &Target::PRIVATE_COMPILER_FLAGS(const string &compilerFlags)
{
    privateCompilerFlags += compilerFlags;
    return *this;
}

Target &Target::PUBLIC_LINKER_FLAGS(const string &linkerFlags)
{
    publicLinkerFlags += linkerFlags;
    return *this;
}

Target &Target::PRIVATE_LINKER_FLAGS(const string &linkerFlags)
{
    privateLinkerFlags += linkerFlags;
    return *this;
}

Target &Target::PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    publicCompileDefinitions.emplace_back(cddName, cddValue);
    return *this;
}

Target &Target::PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    publicCompileDefinitions.emplace_back(cddName, cddValue);
    return *this;
}

Target &Target::SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex)
{
    regexSourceDirs.emplace(sourceDirectory, regex);
    return *this;
}

Target &Target::MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex)
{
    regexModuleDirs.emplace(moduleDirectory, regex);
    return *this;
}

SMFile &Target::addNodeInModuleSourceFileDependencies(const std::string &str, bool angle)
{
    auto [pos, ok] = moduleSourceFileDependencies.emplace(str, this);
    auto &smFile = const_cast<SMFile &>(*pos);
    smFile.presentInSource = true;
    return smFile;
}

void Target::addPrivatePropertiesToPublicProperties()
{
    for (Library *library : publicLibs)
    {
        publicLibs.insert(library->publicLibs.begin(), library->publicLibs.end());
        publicPrebuilts.insert(library->publicPrebuilts.begin(), library->publicPrebuilts.end());
    }
    publicLibs.insert(privateLibs.begin(), privateLibs.end());
    publicCompilerFlags += privateCompilerFlags;

    // TODO: Following is added to compile the examples. However, the code should also add other public
    // properties from public dependencies
    // Modify PLibrary to be dependent of Dependency, thus having public properties.
    for (Library *lib : publicLibs)
    {
        for (const Node *idd : lib->publicIncludes)
        {
            publicIncludes.emplace_back(idd);
        }
        publicCompilerFlags += lib->publicCompilerFlags;
    }
    for (Library *lib : publicLibs)
    {
        for (const Node *idd : lib->publicHUIncludes)
        {
            publicIncludes.emplace_back(idd);
        }
    }
}

string &Target::getCompileCommand()
{
    if (compileCommand.empty())
    {
        setCompileCommand();
    }
    return compileCommand;
}

void Target::setCompileCommand()
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
    compileCommand += publicCompilerFlags + " ";
    compileCommand += compilerTransitiveFlags + " ";

    for (const auto &i : publicCompileDefinitions)
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

    for (const Node *idd : publicIncludes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(idd->filePath) + " ");
    }
    for (const Node *idd : publicHUIncludes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(idd->filePath) + " ");
    }

    for (const string &str : environment.includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(str) + " ");
    }
}

void Target::setSourceCompileCommandPrintFirstHalf()
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
        sourceCompileCommandPrintFirstHalf += publicCompilerFlags + " ";
    }
    if (ccpSettings.compilerTransitiveFlags)
    {
        sourceCompileCommandPrintFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : publicCompileDefinitions)
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

    for (const Node *idd : publicIncludes)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(idd->filePath, ccpSettings.projectIncludeDirectories) + " ";
        }
    }
    for (const Node *idd : publicHUIncludes)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(idd->filePath, ccpSettings.projectIncludeDirectories) + " ";
        }
    }

    for (const string &str : environment.includeDirectories)
    {
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(str, ccpSettings.environmentIncludeDirectories) + " ";
        }
    }
}

string &Target::getSourceCompileCommandPrintFirstHalf()
{
    if (sourceCompileCommandPrintFirstHalf.empty())
    {
        setSourceCompileCommandPrintFirstHalf();
    }
    return sourceCompileCommandPrintFirstHalf;
}

void Target::setLinkOrArchiveCommandAndPrint()
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

    // TODO
    //  GCC linker gives error if object files come after the library
    for (BTarget *dependency : allDependencies)
    {
        if (dependency->resultType == ResultType::LINK)
        {
            if (bTargetType != TargetType::LIBRARY_STATIC)
            {
                const auto *targetDependency = static_cast<const Target *>(dependency);
                linkOrArchiveCommand += getLinkFlag(targetDependency->outputDirectory, targetDependency->outputName);
                linkOrArchiveCommandPrintFirstHalf += getLinkFlagPrint(
                    targetDependency->outputDirectory, targetDependency->outputName, lcpSettings.libraryDependencies);
            }
        }
        else
        {
            Target *target;
            SMFile *smFile;
            if (dependency->resultType == ResultType::SOURCENODE)
            {
                target = this;
                if (!sourceFileDependencies.contains(static_cast<SourceNode &>(*dependency)))
                {
                    continue;
                }
            }
            else
            {
                smFile = static_cast<SMFile *>(dependency);
                target = &(smFile->target);
                assert(dependency->resultType == ResultType::CPP_MODULE &&
                       "ResultType must be either ResultType::SourceNode or ResultType::CPP_MODULE");
                if (!moduleSourceFileDependencies.contains(static_cast<SMFile &>(*dependency)))
                {
                    continue;
                }
            }
            const PathPrint *pathPrint;
            if (bTargetType == TargetType::LIBRARY_STATIC)
            {
                pathPrint = &(acpSettings.objectFiles);
            }
            else
            {
                pathPrint = &(lcpSettings.objectFiles);
            }
            string outputFilePath;
            if (dependency->resultType == ResultType::CPP_MODULE && smFile->standardHeaderUnit)
            {
                outputFilePath =
                    addQuotes((path(*(smFile->target.variantFilePath)).parent_path() / "shus/").generic_string() +
                              path(smFile->node->filePath).filename().string() + ".o");
            }
            else
            {
                const auto *sourceNodeDependency = static_cast<const SourceNode *>(dependency);
                outputFilePath = target->buildCacheFilesDirPath +
                                 path(sourceNodeDependency->node->filePath).filename().string() + ".o";
            }
            linkOrArchiveCommand += addQuotes(outputFilePath) + " ";
            if (pathPrint->printLevel != PathPrintLevel::NO)
            {
                linkOrArchiveCommandPrintFirstHalf += getReducedPath(outputFilePath, *pathPrint) + " ";
            }
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
            if (compiler.bTFamily == BTFamily::MSVC)
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

        for (const string &str : environment.libraryDirectories)
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

        linkOrArchiveCommand += addQuotes((path(outputDirectory) / actualOutputName).string());
        if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrintFirstHalf +=
                getReducedPath((path(outputDirectory) / actualOutputName).string(), lcpSettings.binary);
        }
    }
}

string &Target::getLinkOrArchiveCommand()
{
    if (linkOrArchiveCommand.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }
    return linkOrArchiveCommand;
}

string &Target::getLinkOrArchiveCommandPrintFirstHalf()
{
    if (linkOrArchiveCommandPrintFirstHalf.empty())
    {
        setLinkOrArchiveCommandAndPrint();
    }

    return linkOrArchiveCommandPrintFirstHalf;
}

void Target::setSourceNodeFileStatus(SourceNode &sourceNode, bool angle = false) const
{
    sourceNode.fileStatus = FileStatus::UPDATED;

    path objectFilePath = path(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".o");

    if (!std::filesystem::exists(objectFilePath))
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    if (sourceNode.node->getLastUpdateTime() > objectFileLastEditTime)
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {
        for (const Node *headerNode : sourceNode.headerDependencies)
        {
            if (headerNode->getLastUpdateTime() > objectFileLastEditTime)
            {
                sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
                break;
            }
        }
    }
}

void Target::checkForRelinkPrebuiltDependencies()
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

void Target::checkForPreBuiltAndCacheDir()
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
                sourceNode->headerDependencies.emplace(Node::getNodeFromString(headerFile, true));
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

void Target::populateSetTarjanNodesSourceNodes(Builder &builder)
{
    for (const SourceNode &sourceNodeConst : sourceFileDependencies)
    {
        auto &sourceNode = const_cast<SourceNode &>(sourceNodeConst);
        if (sourceNode.presentInSource != sourceNode.presentInCache)
        {
            sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            setSourceNodeFileStatus(sourceNode);
        }
        addDependency(sourceNode);
    }
}

template <typename T, typename U = std::less<T>> pair<T *, bool> insert(set<T *, U> &containerSet, T *element)
{
    auto [pos, Ok] = containerSet.emplace(element);
    return make_pair(const_cast<T *>(*pos), Ok);
}

template <typename T, typename U = std::less<T>, typename... V>
pair<T *, bool> insert(set<T, U> &containerSet, V &&...elements)
{
    auto [pos, Ok] = containerSet.emplace(std::forward<V>(elements)...);
    return make_pair(const_cast<T *>(&(*pos)), Ok);
}

void Target::parseModuleSourceFiles(Builder &builder)
{
    ModuleScope &moduleScope = builder.moduleScopes.emplace(variantFilePath, ModuleScope{}).first->second;
    for (const Node *idd : publicHUIncludes)
    {
        if (const auto &[pos, Ok] = moduleScope.appHUDirTarget.emplace(idd, this); !Ok)
        {
            // TODO:
            //  Improve Message
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "hu-include-directory\n{}\n is being provided by two different targets\n{}\n{}\nThis is not allowed "
                  "because HMake can't determine which Header Unit to attach to which target.",
                  idd->filePath, targetFilePath, pos->second->targetFilePath);
            exit(EXIT_FAILURE);
        }
    }
    for (const SMFile &smFileConst : moduleSourceFileDependencies)
    {
        auto smFile = const_cast<SMFile &>(smFileConst);

        if (auto [pos, Ok] = moduleScope.smFiles.emplace(&smFile); Ok)
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
                  "In Module Scope\n{}\nmodule file\n{}\nis being provided by two targets\n{}\n{}\n", *variantFilePath,
                  smFile.node->filePath, targetFilePath, (**pos).target.targetFilePath);
            exit(EXIT_FAILURE);
        }
    }
}

string Target::getInfrastructureFlags()
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

string Target::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
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
        command += getReducedPath(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".o",
                                  ccpSettings.objectFile) +
                   " ";
    }
    return command;
}

PostCompile Target::CompileSMFile(SMFile &smFile, Builder &builder)
{
    // TODO: Add Compiler error handling for this operation.
    // TODO: A temporary hack to support modules
    string finalCompileCommand = compileCommand;

    for (const BTarget *bTarget : smFile.allDependencies)
    {
        if (bTarget->resultType == ResultType::CPP_MODULE)
        {
            const auto *smFileDependency = static_cast<const SMFile *>(bTarget);
            finalCompileCommand += smFileDependency->getRequireFlag(smFile) + " ";
        }
    }
    finalCompileCommand += getInfrastructureFlags() + addQuotes(smFile.node->filePath) + " ";

    // TODO:
    //  getFlag and getRequireFlags create confusion. Instead of getRequireFlags, only getFlag should be used.
    if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
    {
        if (smFile.standardHeaderUnit)
        {
            string cacheFilePath = (path(*(smFile.target.variantFilePath)).parent_path() / "shus/").generic_string();
            finalCompileCommand += smFile.getFlag(cacheFilePath + path(smFile.node->filePath).filename().string());
        }
        else
        {
            finalCompileCommand += smFile.getFlag(smFile.ahuTarget->buildCacheFilesDirPath +
                                                  path(smFile.node->filePath).filename().string());
        }
    }
    else
    {
        finalCompileCommand += smFile.getFlag(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string());
    }

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile.getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       path(smFile.node->filePath).filename().string(),
                       settings.ccpSettings.outputAndErrorFiles};
}

PostCompile Target::Compile(SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

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

PostBasic Target::Archive()
{
    return PostBasic(archiver, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(),
                     buildCacheFilesDirPath, targetName, settings.acpSettings.outputAndErrorFiles, true);
}

PostBasic Target::Link()
{
    return PostBasic(linker, getLinkOrArchiveCommand(), getLinkOrArchiveCommandPrintFirstHalf(), buildCacheFilesDirPath,
                     targetName, settings.lcpSettings.outputAndErrorFiles, true);
}

PostBasic Target::GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError)
{
    string finalCompileCommand = getCompileCommand() + addQuotes(sourceNode.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            " /scanDependencies " +
            addQuotes(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".smrules") + " ";
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Modules supported only on MSVC");
    }

    return printOnlyOnError
               ? PostBasic(compiler, finalCompileCommand, "", buildCacheFilesDirPath,
                           path(sourceNode.node->filePath).filename().string(),
                           settings.ccpSettings.outputAndErrorFiles, true)
               : PostBasic(compiler, finalCompileCommand,
                           getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                           buildCacheFilesDirPath, path(sourceNode.node->filePath).filename().string() + ".smrules",
                           settings.ccpSettings.outputAndErrorFiles, true);
}

TargetType Target::getTargetType() const
{
    return bTargetType;
}

void Target::pruneAndSaveBuildCache(const bool successful)
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

void Target::execute(unsigned long fileTargetId)
{
}

void Target::executePrintRoutine(unsigned long fileTargetId)
{
}

/*
void Target::buildTargetsSetFunc()
{
    for (const Target *target_ : buildTargetsSet)
    {
        auto *target = const_cast<Target *>(target_);
        target->checkForPreBuiltAndCacheDir();
        target->parseModuleSourceFiles(*this);
    }
}


void Target::checkForRelinkPrebuiltDependencies()
{
    path outputPath = outputDirectory.directoryPath / actualOutputName;
    if (!std::filesystem::exists(outputPath))
    {
        fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {
        for (PLibrary *pLibrary : publicPrebuilts)
        {
            if (last_write_time(pLibrary->libraryPath) > last_write_time(outputPath))
            {
                fileStatus = FileStatus::NEEDS_UPDATE;
            }
        }
    }
}

void Target::checkForPreBuiltAndCacheDir()
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
                sourceNode->headerDependencies.emplace(&(Node::getNodeFromString(headerFile)));
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
*/

Executable::Executable(string targetName_, const bool initializeFromCache)
    : Target(std::move(targetName_), initializeFromCache)
{
    cTargetType = TargetType::EXECUTABLE;
}

Executable::Executable(string targetName_, Variant &variant) : Target(std::move(targetName_), variant)
{
    cTargetType = TargetType::EXECUTABLE;
}

Library::Library(string targetName_, const bool initializeFromCache)
    : Target(std::move(targetName_), initializeFromCache)
{
    cTargetType = libraryType;
}

Library::Library(string targetName_, Variant &variant) : Target(std::move(targetName_), variant)
{
    cTargetType = libraryType;
}

void to_json(Json &j, const Library *library)
{
    j[JConsts::prebuilt] = false;
    j[JConsts::path] = library->getTargetPointer();
}

bool operator==(const Version &lhs, const Version &rhs)
{
    return lhs.majorVersion == rhs.majorVersion && lhs.minorVersion == rhs.minorVersion &&
           lhs.patchVersion == rhs.patchVersion;
}

PLibrary::PLibrary(Variant &variant, const path &libraryPath_, TargetType libraryType_)
{
    // TODO
    // Should check if library exists or not
    libraryType = libraryType_;
    if (libraryType != TargetType::PLIBRARY_STATIC || libraryType != TargetType::PLIBRARY_SHARED)
    {
        print(stderr, "PLibrary libraryType TargetType is not one of PLIBRARY_STATIC or PLIBRARY_SHARED \n");
        exit(EXIT_FAILURE);
    }
    variant.preBuiltLibraries.emplace(this);
    if (libraryPath_.is_relative())
    {
        libraryPath = path(srcDir) / libraryPath_;
        libraryPath = libraryPath.lexically_normal();
    }
    libraryName = getTargetNameFromActualName(libraryType, os, libraryPath.filename().string());
}

bool operator<(const PLibrary &lhs, const PLibrary &rhs)
{
    // TODO
    // What is this?
    return lhs.libraryName < rhs.libraryName && lhs.libraryPath < rhs.libraryPath && lhs.includes < rhs.includes &&
           lhs.compilerFlags < rhs.compilerFlags && lhs.compilerFlags < rhs.compilerFlags;
}

void to_json(Json &j, const PLibrary *pLibrary)
{
    j[JConsts::prebuilt] = true;
    j[JConsts::path] = pLibrary->libraryPath.generic_string();
}