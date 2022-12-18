#if defined(__clang__)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-static-cast-downcast"
#endif

#include "Configure.hpp"

#include <utility>

#include "algorithm"
#include "fstream"
#include "iostream"
#include "regex"
#include "set"

using std::to_string, std::filesystem::directory_entry, std::filesystem::file_type, std::logic_error, std::ifstream,
    std::ofstream, std::move, std::filesystem::current_path, std::cout, std::endl, std::cerr, std::stringstream,
    std::make_tuple, std::filesystem::directory_iterator, std::filesystem::recursive_directory_iterator,
    std::filesystem::remove, std::filesystem::create_directories;

std::string writePath(const path &writePath)
{
    string str = writePath.string();
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}

string addQuotes(const string &pathString)
{
    return "\"" + pathString + "\"";
}

string addEscapedQuotes(const string &pathString)
{
    const string str = R"(\")";
    return str + pathString + str;
}

void demo_status(const fs::path &p, fs::file_status s)
{
    std::cout << p;
    switch (s.type())
    {
    case fs::file_type::none:
        std::cout << " has `not-evaluated-yet` type";
        break;
    case fs::file_type::not_found:
        std::cout << " does not exist";
        break;
    case fs::file_type::regular:
        std::cout << " is a regular file";
        break;
    case fs::file_type::directory:
        std::cout << " is a directory";
        break;
    case fs::file_type::symlink:
        std::cout << " is a symlink";
        break;
    case fs::file_type::block:
        std::cout << " is a block device";
        break;
    case fs::file_type::character:
        std::cout << " is a character device";
        break;
    case fs::file_type::fifo:
        std::cout << " is a named IPC pipe";
        break;
    case fs::file_type::socket:
        std::cout << " is a named IPC socket";
        break;
    case fs::file_type::unknown:
        std::cout << " has `unknown` type";
        break;
    default:
        std::cout << " has `implementation-defined` type";
        break;
    }
    std::cout << '\n';
}

File::File(const path &filePath_)
{
    if (filePath_.is_relative())
    {
        filePath = Cache::sourceDirectory.directoryPath / filePath_;
    }
    else
    {
        filePath = filePath_;
    }
    if (directory_entry(filePath).status().type() == file_type::regular)
    {
        filePath = filePath.lexically_normal();
    }
    else
    {
        cerr << filePath_ << " Is Not a regular file";
        exit(EXIT_FAILURE);
    }
}

void to_json(Json &json, const File &file)
{
    json = file.filePath.generic_string();
}

void from_json(const Json &json, File &file)
{
    file.filePath = path(json.get<string>());
}

bool operator<(const File &lhs, const File &rhs)
{
    return lhs.filePath < rhs.filePath;
}

Directory::Directory(const path &directoryPath_)
{
    if (directoryPath_.is_relative())
    {
        directoryPath = Cache::sourceDirectory.directoryPath / directoryPath_;
    }
    else
    {
        directoryPath = directoryPath_;
    }
    auto type = directory_entry(directoryPath).status().type();
    if (type == file_type::directory)
    {
        directoryPath = directoryPath.lexically_normal() / "";
    }
    else
    {
        cerr << directoryPath_ << " Is Not a directory file. File type is ";
        demo_status(directoryPath, fs::status(directoryPath));
        exit(EXIT_FAILURE);
    }
}

void to_json(Json &json, const Directory &directory)
{
    json = directory.directoryPath.generic_string();
}

void from_json(const Json &json, Directory &directory)
{
    directory = Directory(path(json.get<string>()));
}

bool operator<(const Directory &lhs, const Directory &rhs)
{
    return lhs.directoryPath.generic_string() < rhs.directoryPath.generic_string();
}

Define::Define(const string &name_, const string &value_) : name{name_}, value{value_}
{
}

void to_json(Json &j, const Define &cd)
{
    j[JConsts::name] = cd.name;
    j[JConsts::value] = cd.value;
}

void from_json(const Json &j, Define &cd)
{
    cd.name = j.at(JConsts::name).get<string>();
    cd.value = j.at(JConsts::value).get<string>();
}

void to_json(Json &json, const OSFamily &bTFamily)
{
    if (bTFamily == OSFamily::WINDOWS)
    {
        json = JConsts::windows;
    }
    else if (bTFamily == OSFamily::LINUX_UNIX)
    {
        json = JConsts::linuxUnix;
    }
}

void from_json(const Json &json, OSFamily &bTFamily)
{
    if (json == JConsts::windows)
    {
        bTFamily = OSFamily::WINDOWS;
    }
    else if (json == JConsts::linuxUnix)
    {
        bTFamily = OSFamily::LINUX_UNIX;
    }
}

void to_json(Json &json, const BTFamily &bTFamily)
{
    if (bTFamily == BTFamily::GCC)
    {
        json = JConsts::gcc;
    }
    else if (bTFamily == BTFamily::MSVC)
    {
        json = JConsts::msvc;
    }
    else
    {
        json = JConsts::clang;
    }
}

void from_json(const Json &json, BTFamily &bTFamily)
{
    if (json == JConsts::gcc)
    {
        bTFamily = BTFamily::GCC;
    }
    else if (json == JConsts::msvc)
    {
        bTFamily = BTFamily::MSVC;
    }
    else if (json == JConsts::clang)
    {
        bTFamily = BTFamily::CLANG;
    }
}

void to_json(Json &json, const BuildTool &buildTool)
{
    json[JConsts::family] = buildTool.bTFamily;
    json[JConsts::path] = buildTool.bTPath.generic_string();
    json[JConsts::version] = buildTool.bTVersion;
}

void from_json(const Json &json, BuildTool &buildTool)
{
    buildTool.bTPath = json.at(JConsts::path).get<string>();
    buildTool.bTFamily = json.at(JConsts::family).get<BTFamily>();
    buildTool.bTVersion = json.at(JConsts::version).get<Version>();
}

void to_json(Json &json, const LibraryType &libraryType)
{
    if (libraryType == LibraryType::STATIC)
    {
        json = JConsts::static_;
    }
    else
    {
        json = JConsts::shared;
    }
}

void from_json(const Json &json, LibraryType &libraryType)
{
    if (json == JConsts::static_)
    {
        libraryType = LibraryType::STATIC;
    }
    else if (json == JConsts::shared)
    {
        libraryType = LibraryType::SHARED;
    }
}

void to_json(Json &json, const ConfigType &configType)
{
    if (configType == ConfigType::DEBUG)
    {
        json = JConsts::debug;
    }
    else
    {
        json = JConsts::release;
    }
}

void from_json(const Json &json, ConfigType &configType)
{
    if (json == JConsts::debug)
    {
        configType = ConfigType::DEBUG;
    }
    else if (json == JConsts::release)
    {
        configType = ConfigType::RELEASE;
    }
}

Target::Target(string targetName_) : targetName(move(targetName_))
{
}

Target::Target(string targetName_, const Variant &variant) : targetName(move(targetName_)), outputName(targetName)
{
    assignConfigurationFromVariant(variant);
    isTarget = true;
}

Target::Target(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies)
    : targetName(move(targetName_)), outputName(targetName)
{
    assignConfigurationFromVariant(variantFrom);
    for (Library *library : getAllDependencies())
    {
        library->assignConfigurationFromVariant(variantFrom);
    }
}

string Target::getTargetFilePath(unsigned long variantCount) const
{
    return Cache::configureDirectory.directoryPath.generic_string() + to_string(variantCount) + "/" + targetName + "/" +
           targetName + ".hmake";
}

string Target::getTargetFilePathPackage(unsigned long variantCount) const
{
    return Cache::configureDirectory.directoryPath.generic_string() + "/Package/" + to_string(variantCount) + "/" +
           targetName + "/" + targetName + ".hmake";
}

void Target::setPropertiesFlags() const
{
    // TODO: setTargetForAndOr is not being called as this target is marked const
    // Notes
    //  For Windows NT, we use a different JAMSHELL
    //  Currently, I ain't caring for the propagated properties. These properties are only being
    //  assigned to the parent.
    //  TODO s
    //  262 Be caustious of this rc-type. It has something to do with Windows resource compiler
    //  which I don't know
    //  TODO: flavor is being assigned based on the -dumpmachine argument to the gcc command. But this
    //   is not yet catered here.
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
        if (targetType == TargetType::STATIC || targetType == TargetType::SHARED)
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
                cout << "WARNING: On gcc, DLLs can not be built with <runtime-link>static" << endl;
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

void Target::assignConfigurationFromVariant(const Variant &variant)
{
    static_cast<CommonProperties &>(*this) = static_cast<const CommonProperties &>(variant);
    environment = variant.environment;
}

// TODO Not Copying the dependencies of dependencies
set<Library *> Target::getAllDependencies()
{
    set<Library *> libraries;

    for (Library *library : publicLibs)
    {
        libraries.emplace(library);
    }

    for (Library *library : privateLibs)
    {
        libraries.emplace(library);
    }

    return libraries;
}

vector<string> Target::convertCustomTargetsToJson(const vector<CustomTarget> &customTargets, VariantMode mode)
{
    vector<string> commands;
    for (auto &i : customTargets)
    {
        if (i.mode == mode)
        {
            commands.emplace_back(i.command);
        }
    }
    return commands;
}

// I think I can use std::move for Target members here as this is the last function called.
Json Target::convertToJson(unsigned long variantIndex) const
{
    Json targetFileJson;

    vector<Directory> includeDirectories;
    string compilerFlags;
    string linkerFlags;
    vector<Json> compileDefinitionsArray;
    vector<Json> dependenciesArray;
    SourceAggregate finalModuleAggregate;

    for (const auto &e : publicIncludes)
    {
        includeDirectories.emplace_back(e);
    }
    for (const auto &e : privateIncludes)
    {
        includeDirectories.emplace_back(e);
    }
    for (const auto &e : publicCompilerFlags)
    {
        compilerFlags.append(" " + e + " ");
    }
    for (const auto &e : privateCompilerFlags)
    {
        compilerFlags.append(" " + e + " ");
    }
    for (const auto &e : publicLinkerFlags)
    {
        linkerFlags.append(" " + e + " ");
    }
    for (const auto &e : privateLinkerFlags)
    {
        linkerFlags.append(" " + e + " ");
    }
    for (const auto &e : publicCompileDefinitions)
    {
        Json compileDefinitionObject = e;
        compileDefinitionsArray.emplace_back(compileDefinitionObject);
    }
    for (const auto &e : privateCompileDefinitions)
    {
        Json compileDefinitionObject = e;
        compileDefinitionsArray.emplace_back(compileDefinitionObject);
    }
    finalModuleAggregate = moduleAggregate;

    auto iterateOverLibraries = [&dependenciesArray, &variantIndex](const set<Library *> &libraries) {
        for (Library *library : libraries)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = false;
            libDepObject[JConsts::path] = (Cache::configureDirectory.directoryPath / to_string(variantIndex) /
                                           library->targetName / path(library->targetName + ".hmake"))
                                              .generic_string();

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

    auto iterateOverPackagedLibs = [&dependenciesArray](const set<PPLibrary *> &packagedLibs) {
        for (PPLibrary *ppLibrary : packagedLibs)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = true;
            libDepObject[JConsts::path] = ppLibrary->libraryPath.generic_string();
            dependenciesArray.emplace_back(libDepObject);
        }
    };

    iterateOverLibraries(publicLibs);
    iterateOverLibraries(privateLibs);
    iterateOverPrebuiltLibs(publicPrebuilts);
    iterateOverPrebuiltLibs(privatePrebuilts);
    iterateOverPackagedLibs(publicPackagedLibs);
    iterateOverPackagedLibs(privatePackagedLibs);

    targetFileJson[JConsts::targetType] = targetType;
    targetFileJson[JConsts::outputName] = outputName;
    targetFileJson[JConsts::outputDirectory] = outputDirectory;
    targetFileJson[JConsts::configuration] = configurationType;
    targetFileJson[JConsts::compiler] = compiler;
    targetFileJson[JConsts::linker] = linker;
    targetFileJson[JConsts::archiver] = archiver;
    targetFileJson[JConsts::environment] = environment;
    targetFileJson[JConsts::compilerFlags] = "";
    targetFileJson[JConsts::linkerFlags] = "";
    sourceAggregate.convertToJson(targetFileJson);
    moduleAggregate.convertToJson(targetFileJson);
    targetFileJson[JConsts::libraryDependencies] = dependenciesArray;
    targetFileJson[JConsts::includeDirectories] = includeDirectories;
    targetFileJson[JConsts::compilerTransitiveFlags] = compilerFlags;
    targetFileJson[JConsts::linkerTransitiveFlags] = linkerFlags;
    targetFileJson[JConsts::compileDefinitions] = compileDefinitionsArray;
    targetFileJson[JConsts::preBuildCustomCommands] = convertCustomTargetsToJson(preBuild, VariantMode::PROJECT);
    targetFileJson[JConsts::postBuildCustomCommands] = convertCustomTargetsToJson(postBuild, VariantMode::PROJECT);
    targetFileJson[JConsts::variant] = JConsts::project;
    return targetFileJson;
}

Target &Target::PUBLIC_COMPILER_FLAGS(const string &compilerFlags)
{
    publicCompilerFlags.emplace_back(compilerFlags);
    return *this;
}

Target &Target::PRIVATE_COMPILER_FLAGS(const string &compilerFlags)
{
    privateCompilerFlags.emplace_back(compilerFlags);
    return *this;
}

Target &Target::PUBLIC_LINKER_FLAGS(const string &linkerFlags)
{
    publicLinkerFlags.emplace_back(linkerFlags);
    return *this;
}

Target &Target::PRIVATE_LINKER_FLAGS(const string &linkerFlags)
{
    privateLinkerFlags.emplace_back(linkerFlags);
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
    sourceAggregate.directories.emplace(Directory{sourceDirectory}, regex);
    return *this;
}

Target &Target::MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex)
{
    moduleAggregate.directories.emplace(Directory{moduleDirectory}, regex);
    return *this;
}

PackageVariant::PackageVariant(struct Package &package)
{
    package.packageVariants.emplace_back(this);
}

SourceDirectory::SourceDirectory(const Directory &sourceDirectory_, const string &regex_)
    : sourceDirectory{sourceDirectory_}, regex{regex_}
{
}

void to_json(Json &j, const SourceDirectory &sourceDirectory)
{
    j[JConsts::path] = sourceDirectory.sourceDirectory;
    j[JConsts::regexString] = sourceDirectory.regex;
}

void from_json(const Json &j, SourceDirectory &sourceDirectory)
{
    sourceDirectory.sourceDirectory = Directory(j.at(JConsts::path).get<string>());
    sourceDirectory.regex = j.at(JConsts::regexString).get<string>();
}

bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs)
{
    return lhs.sourceDirectory.directoryPath < rhs.sourceDirectory.directoryPath || lhs.regex < rhs.regex;
}

void SourceAggregate::convertToJson(Json &j) const
{
    j[Identifier + sourceFilesString] = files;
    j[Identifier + sourceDirectoriesString] = directories;
}

set<string> SourceAggregate::convertFromJsonAndGetAllSourceFiles(const Json &j, const string &targetFilePath,
                                                                 const string &Identifier)
{
    set<string> sourceFiles = j.at(Identifier + sourceFilesString).get<set<string>>();
    set<SourceDirectory> sourceDirectories = j.at(Identifier + sourceDirectoriesString).get<set<SourceDirectory>>();
    for (const auto &i : sourceDirectories)
    {
        for (const auto &k : recursive_directory_iterator(i.sourceDirectory.directoryPath))
        {
            try
            {
                if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(i.regex)))
                {
                    sourceFiles.emplace(k.path().generic_string());
                }
            }
            catch (const std::regex_error &e)
            {
                cerr << "regex_error caught: " << e.what() << '\n';
                cerr << "Regular Expressions error happened while parsing regex " << i.regex << " of the target "
                     << targetFilePath << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    return sourceFiles;
}

bool SourceAggregate::empty() const
{
    return files.empty() && directories.empty();
}

void to_json(Json &j, const TargetType &targetType)
{
    switch (targetType)
    {

    case TargetType::EXECUTABLE:
        j = JConsts::executable;
        break;
    case TargetType::STATIC:
        j = JConsts::static_;
        break;
    case TargetType::SHARED:
        j = JConsts::shared;
        break;
    case TargetType::COMPILE:
        j = JConsts::compile;
        break;
    case TargetType::PREPROCESS:
        j = JConsts::preprocess;
        break;
    case TargetType::RUN:
        j = JConsts::run;
        break;
    case TargetType::PLIBRARY_STATIC:
        j = JConsts::plibraryStatic;
        break;
    case TargetType::PLIBRARY_SHARED:
        j = JConsts::plibraryShared;
        break;
    }
}

void from_json(const Json &j, TargetType &targetType)
{
    if (j == JConsts::executable)
    {
        targetType = TargetType::EXECUTABLE;
    }
    else if (j == JConsts::static_)
    {
        targetType = TargetType::STATIC;
    }
    else if (j == JConsts::shared)
    {
        targetType = TargetType::SHARED;
    }
    else if (j == JConsts::compile)
    {
        targetType = TargetType::COMPILE;
    }
    else if (j == JConsts::preprocess)
    {
        targetType = TargetType::PREPROCESS;
    }
    else if (j == JConsts::run)
    {
        targetType = TargetType::RUN;
    }
    else if (j == JConsts::plibraryStatic)
    {
        targetType = TargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = TargetType::PLIBRARY_SHARED;
    }
}

string getActualNameFromTargetName(TargetType targetType, const OSFamily &osFamily, const string &targetName)
{
    if (targetType == TargetType::EXECUTABLE)
    {
        return targetName + (osFamily == OSFamily::WINDOWS ? ".exe" : "");
    }
    else if (targetType == TargetType::STATIC || targetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName;
        actualName = osFamily == OSFamily::WINDOWS ? "" : "lib";
        actualName += targetName;
        actualName += osFamily == OSFamily::WINDOWS ? ".lib" : ".a";
        return actualName;
    }
    else
    {
    }
    cerr << "Other Targets Not Supported Yet." << endl;
    exit(EXIT_FAILURE);
}

string getTargetNameFromActualName(TargetType targetType, const OSFamily &osFamily, const string &actualName)
{
    if (targetType == TargetType::STATIC || targetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = osFamily == OSFamily::WINDOWS ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        unsigned short eraseCount = osFamily == OSFamily::WINDOWS ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    cerr << "Other Targets Not Supported Yet." << endl;
    exit(EXIT_FAILURE);
}

void Target::configure(unsigned long variantIndex) const
{
    path targetFileDir = Cache::configureDirectory.directoryPath / to_string(variantIndex) / targetName;
    create_directories(targetFileDir);
    if (outputDirectory.directoryPath.empty())
    {
        const_cast<Directory &>(outputDirectory) = Directory(targetFileDir);
    }

    Json targetFileJson;
    targetFileJson = convertToJson(variantIndex);

    path targetFilePath = targetFileDir / (targetName + ".hmake");
    ofstream(targetFilePath) << targetFileJson.dump(4);
}

Json Target::convertToJson(const Package &package, unsigned variantIndex) const
{
    Json targetFileJson;

    Json includeDirectoriesArray;
    string compilerFlags;
    string linkerFlags;
    vector<Json> compileDefinitionsArray;
    vector<Json> dependenciesArray;

    vector<string> consumerIncludeDirectories;
    string consumerCompilerFlags;
    string consumerLinkerFlags;
    vector<Json> consumerCompileDefinitionsArray;
    vector<Json> consumerDependenciesArray;

    for (const auto &e : publicIncludes)
    {
        Json incDirJson;
        incDirJson[JConsts::path] = e;
        if (package.cacheCommonIncludeDirs && e.isCommon)
        {
            consumerIncludeDirectories.emplace_back("../../Common/" + to_string(e.commonDirectoryNumber) + "/include/");
            incDirJson[JConsts::copy] = false;
        }
        else
        {
            consumerIncludeDirectories.emplace_back("include/");
            incDirJson[JConsts::copy] = true;
        }
        includeDirectoriesArray.emplace_back(incDirJson);
    }
    for (const auto &e : privateIncludes)
    {
        Json incDirJson;
        incDirJson[JConsts::path] = e;
        incDirJson[JConsts::copy] = false;
        includeDirectoriesArray.emplace_back(incDirJson);
    }

    for (const auto &e : publicCompilerFlags)
    {
        compilerFlags.append(" " + e + " ");
        consumerCompilerFlags.append(" " + e + " ");
    }
    for (const auto &e : publicLinkerFlags)
    {
        linkerFlags.append(" " + e + " ");
        consumerLinkerFlags.append(" " + e + " ");
    }
    for (const auto &e : publicCompileDefinitions)
    {
        Json compileDefinitionObject = e;
        compileDefinitionsArray.emplace_back(compileDefinitionObject);
        consumerCompileDefinitionsArray.emplace_back(compileDefinitionObject);
    }

    auto iterateOverLibraries = [&dependenciesArray, &variantIndex,
                                 &consumerDependenciesArray](const set<Library *> &libraries) {
        for (Library *library : libraries)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = false;
            Json consumeLibDepObject;
            consumeLibDepObject[JConsts::importedFromOtherHmakePackageFromOtherHmakePackage] = false;
            consumeLibDepObject[JConsts::name] = library->targetName;
            consumerDependenciesArray.emplace_back(consumeLibDepObject);
            libDepObject[JConsts::path] = library->getTargetFilePathPackage(variantIndex);
            dependenciesArray.emplace_back(libDepObject);
        }
    };

    auto iterateOverPrebuiltLibs = [&dependenciesArray, &variantIndex,
                                    &consumerDependenciesArray](const set<PLibrary *> &prebuiltLibs) {
        for (PLibrary *pLibrary : prebuiltLibs)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = true;
            libDepObject[JConsts::path] = pLibrary->libraryPath.generic_string();
            libDepObject[JConsts::importedFromOtherHmakePackage] = false;
            libDepObject[JConsts::hmakeFilePath] =
                pLibrary->getTargetVariantDirectoryPath(variantIndex) / (pLibrary->libraryName + ".hmake");
            dependenciesArray.emplace_back(libDepObject);

            Json consumeLibDepObject;
            consumeLibDepObject[JConsts::importedFromOtherHmakePackage] = false;
            consumeLibDepObject[JConsts::name] = pLibrary->libraryName;
            consumerDependenciesArray.emplace_back(consumeLibDepObject);
        }
    };

    auto iterateOverPackagedLibs = [&dependenciesArray, &consumerDependenciesArray,
                                    &variantIndex](const set<PPLibrary *> &packagedLibs) {
        for (PPLibrary *ppLibrary : packagedLibs)
        {
            Json libDepObject;
            libDepObject[JConsts::prebuilt] = true;
            libDepObject[JConsts::path] = ppLibrary->libraryPath.generic_string();
            libDepObject[JConsts::importedFromOtherHmakePackage] = ppLibrary->importedFromOtherHMakePackage;
            if (!ppLibrary->importedFromOtherHMakePackage)
            {
                libDepObject[JConsts::hmakeFilePath] =
                    ppLibrary->getTargetVariantDirectoryPath(variantIndex) / (ppLibrary->libraryName + ".hmake");
            }
            dependenciesArray.emplace_back(libDepObject);

            Json consumeLibDepObject;
            consumeLibDepObject[JConsts::importedFromOtherHmakePackage] = ppLibrary->importedFromOtherHMakePackage;
            consumeLibDepObject[JConsts::name] = ppLibrary->libraryName;
            if (ppLibrary->importedFromOtherHMakePackage)
            {
                consumeLibDepObject[JConsts::packageName] = ppLibrary->packageName;
                consumeLibDepObject[JConsts::packageVersion] = ppLibrary->packageVersion;
                consumeLibDepObject[JConsts::packagePath] = ppLibrary->packagePath.generic_string();
                consumeLibDepObject[JConsts::useIndex] = ppLibrary->useIndex;
                consumeLibDepObject[JConsts::index] = ppLibrary->index;
                consumeLibDepObject[JConsts::packageVariantJson] = ppLibrary->packageVariantJson;
            }
            consumerDependenciesArray.emplace_back(consumeLibDepObject);
        }
    };

    iterateOverLibraries(publicLibs);
    iterateOverLibraries(privateLibs);
    iterateOverPrebuiltLibs(publicPrebuilts);
    iterateOverPrebuiltLibs(privatePrebuilts);
    iterateOverPackagedLibs(publicPackagedLibs);
    iterateOverPackagedLibs(privatePackagedLibs);

    targetFileJson[JConsts::targetType] = targetType;
    targetFileJson[JConsts::outputName] = outputName;
    targetFileJson[JConsts::outputDirectory] =
        (path(getTargetFilePathPackage(variantIndex)).parent_path() / "").generic_string();
    targetFileJson[JConsts::configuration] = configurationType;
    targetFileJson[JConsts::compiler] = compiler;
    targetFileJson[JConsts::linker] = linker;
    targetFileJson[JConsts::archiver] = archiver;
    targetFileJson[JConsts::environment] = environment;
    targetFileJson[JConsts::compilerFlags] = "";
    targetFileJson[JConsts::linkerFlags] = "";
    sourceAggregate.convertToJson(targetFileJson);
    moduleAggregate.convertToJson(targetFileJson);
    targetFileJson[JConsts::libraryDependencies] = dependenciesArray;
    targetFileJson[JConsts::includeDirectories] = includeDirectoriesArray;
    targetFileJson[JConsts::compilerTransitiveFlags] = compilerFlags;
    targetFileJson[JConsts::linkerTransitiveFlags] = linkerFlags;
    targetFileJson[JConsts::compileDefinitions] = compileDefinitionsArray;
    targetFileJson[JConsts::preBuildCustomCommands] = convertCustomTargetsToJson(preBuild, VariantMode::PACKAGE);
    targetFileJson[JConsts::postBuildCustomCommands] = convertCustomTargetsToJson(postBuild, VariantMode::PACKAGE);

    Json consumerDependenciesJson;

    consumerDependenciesJson[JConsts::libraryType] = targetType;
    consumerDependenciesJson[JConsts::libraryDependencies] = consumerDependenciesArray;
    consumerDependenciesJson[JConsts::includeDirectories] = consumerIncludeDirectories;
    consumerDependenciesJson[JConsts::compilerTransitiveFlags] = consumerCompilerFlags;
    consumerDependenciesJson[JConsts::linkerTransitiveFlags] = consumerLinkerFlags;
    consumerDependenciesJson[JConsts::compileDefinitions] = consumerCompileDefinitionsArray;

    targetFileJson[JConsts::consumerDependencies] = consumerDependenciesJson;
    targetFileJson[JConsts::variant] = JConsts::package;
    targetFileJson[JConsts::packageCopy] = Cache::copyPackage;
    if (Cache::copyPackage)
    {
        targetFileJson[JConsts::packageName] = package.name;
        targetFileJson[JConsts::packageVersion] = package.version;
        targetFileJson[JConsts::packageCopyPath] = Cache::packageCopyPath;
        targetFileJson[JConsts::packageVariantIndex] = variantIndex;
    }
    return targetFileJson;
}

void Target::configure(const Package &package, unsigned count) const
{
    Json targetFileJson;
    targetFileJson = convertToJson(package, count);
    string str = getTargetFilePathPackage(count);
    create_directories(path(str).parent_path());
    ofstream(str) << targetFileJson.dump(4);
}

bool std::less<Target *>::operator()(const Target *lhs, const Target *rhs) const
{
    return lhs->targetName < rhs->targetName;
    cout << "Comparator Called" << endl;
}

Executable::Executable(string targetName_) : Target(move(targetName_))
{
}

Executable::Executable(string targetName_, Variant &variant) : Target(move(targetName_), variant)
{
    targetType = TargetType::EXECUTABLE;
    if (auto [pos, Ok] = variant.executables.emplace(this); !Ok)
    {
        fmt::print(stderr, "Executable {} already added to variant.", targetName_);
        exit(EXIT_FAILURE);
    }
}

Executable::Executable(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies)
    : Target(std::move(targetName_), variantFrom, variantTo, copyDependencies)
{
}

Library::Library(string targetName_) : Target(move(targetName_))
{
}

Library::Library(string targetName_, Variant &variant) : Target(move(targetName_), variant)
{
    setLibraryType(variant.libraryType);
    if (auto [pos, Ok] = variant.libraries.emplace(this); !Ok)
    {
        fmt::print(stderr, "Library {} already added to variant.", targetName_);
        exit(EXIT_FAILURE);
    }
}

Library::Library(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies)
    : Target(std::move(targetName_), variantFrom, variantTo, copyDependencies)
{
}

void Library::setLibraryType(LibraryType libraryType_)
{
    libraryType = libraryType_;
    if (libraryType == LibraryType::STATIC)
    {
        targetType = TargetType::STATIC;
    }
    else
    {
        targetType = TargetType::SHARED;
    }
}

bool operator==(const Version &lhs, const Version &rhs)
{
    return lhs.majorVersion == rhs.majorVersion && lhs.minorVersion == rhs.minorVersion &&
           lhs.patchVersion == rhs.patchVersion;
}

void Cache::initializeCache()
{
    path filePath = current_path() / "cache.hmake";
    Json cacheFileJson;
    ifstream(filePath) >> cacheFileJson;

    path srcDirPath = cacheFileJson.at(JConsts::sourceDirectory).get<string>();
    if (srcDirPath.is_relative())
    {
        srcDirPath = (current_path() / srcDirPath).lexically_normal();
    }

    Cache::sourceDirectory = Directory(srcDirPath);
    Cache::configureDirectory = Directory(current_path());

    srcDir = srcDirPath.generic_string();
    configureDir = configureDirectory.directoryPath.generic_string();

    Cache::copyPackage = cacheFileJson.at(JConsts::packageCopy).get<bool>();
    if (copyPackage)
    {
        packageCopyPath = cacheFileJson.at(JConsts::packageCopyPath).get<string>();
    }

    Cache::projectConfigurationType = cacheFileJson.at(JConsts::configuration).get<ConfigType>();
    Cache::compilerArray = cacheFileJson.at(JConsts::compilerArray).get<vector<Compiler>>();
    Cache::selectedCompilerArrayIndex = cacheFileJson.at(JConsts::compilerSelectedArrayIndex).get<int>();
    Cache::linkerArray = cacheFileJson.at(JConsts::linkerArray).get<vector<Linker>>();
    Cache::selectedLinkerArrayIndex = cacheFileJson.at(JConsts::compilerSelectedArrayIndex).get<int>();
    Cache::archiverArray = cacheFileJson.at(JConsts::archiverArray).get<vector<Archiver>>();
    Cache::selectedArchiverArrayIndex = cacheFileJson.at(JConsts::archiverSelectedArrayIndex).get<int>();
    Cache::libraryType = cacheFileJson.at(JConsts::libraryType).get<LibraryType>();
    Cache::cacheVariables = cacheFileJson.at(JConsts::cacheVariables).get<Json>();
#ifdef _WIN32
    Cache::environment = Environment::initializeEnvironmentFromVSBatchCommand(
        R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64)");
#else

#endif
    if (!std::filesystem::exists(path("settings.hmake")))
    {
        Json settings = Settings{};
        ofstream("settings.hmake") << settings.dump(4);
    }
}

void Cache::registerCacheVariables()
{
    path filePath = current_path() / "cache.hmake";
    Json cacheFileJson;
    ifstream(filePath) >> cacheFileJson;
    cacheFileJson[JConsts::cacheVariables] = Cache::cacheVariables;
    ofstream(filePath) << cacheFileJson.dump(4);
}

void to_json(Json &j, const Arch &arch)
{
}

void from_json(const Json &j, Arch &arch)
{
}

ToolSet::ToolSet(TS ts_) : ts{ts_}
{
    // Initialize Name And Version
}

bool Variant::TargetComparator::operator()(const class Target *lhs, const class Target *rhs) const
{
    return lhs->targetName < rhs->targetName;
}

Variant::Variant()
{
    configurationType = Cache::projectConfigurationType;
    compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
    linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
    archiver = Cache::archiverArray[Cache::selectedArchiverArrayIndex];
    libraryType = Cache::libraryType;
    environment = Cache::environment;
}

// TODO: Will Not be copying prebuilts and packagedLibs for Now.
void Variant::copyAllTargetsFromOtherVariant(Variant &variantFrom)
{
    for (Executable *executable : variantFrom.executables)
    {
        executablesContainer.emplace_back(make_shared<Executable>(*executable));
        executables.emplace(executablesContainer.back().get());
    }
    for (Library *library : variantFrom.libraries)
    {
        librariesContainer.emplace_back(make_shared<Library>(*library));
        libraries.emplace(librariesContainer.back().get());
    }
}

void Variant::configure(VariantMode mode, unsigned long variantCount, const class Package &package)
{
    set<TarjanNode<Dependent>> tarjanNodesLibraryPointers;
    {
        // TODO
        // This does not work because prebuilts is not public. change this in a way that it does not effect PPLibrary
        // which is derived from PLibrary
        /*        for (PLibrary *pLibrary : preBuiltLibraries)
                {
                    auto [pos, Ok] = tarjanNodesLibraryPointers.emplace(pLibrary);
                    for(PLibrary *pLibrary1 : pLibrary->prebuilts)
                    {
                        auto [posDep, Ok1] = tarjanNodesLibraryPointers.emplace(pLibrary1);
                        pos->deps.emplace_back(const_cast<TarjanNode<Dependent> *>(&(*posDep)));
                    }
                }*/

        // TODO
        // Packaged Libs currently can't have dependencies. Need to revise Packaging and this decision. However,
        // currently allowing prebuilts to have other prebuilts as dependencies so to help package a project that was
        // not compiled in HMake.
        auto populateTarjanNode = [&tarjanNodesLibraryPointers](const auto &targets) {
            for (Target *target : targets)
            {
                auto [pos, Ok] = tarjanNodesLibraryPointers.emplace(target);
                auto b = pos;

                auto populateDepsOfTarjanNode = [&b, &tarjanNodesLibraryPointers](auto &dependentVector) {
                    for (Dependent *dependent : dependentVector)
                    {
                        auto [posDep, Ok1] = tarjanNodesLibraryPointers.emplace(dependent);
                        b->deps.emplace_back(const_cast<TarjanNode<Dependent> *>(&(*posDep)));
                    }
                };

                populateDepsOfTarjanNode(target->publicLibs);
                populateDepsOfTarjanNode(target->privateLibs);
                populateDepsOfTarjanNode(target->publicPrebuilts);
                populateDepsOfTarjanNode(target->privatePrebuilts);
                populateDepsOfTarjanNode(target->publicPackagedLibs);
                populateDepsOfTarjanNode(target->privatePackagedLibs);
            }
        };

        populateTarjanNode(executables);
        populateTarjanNode(libraries);

        TarjanNode<Dependent>::tarjanNodes = &tarjanNodesLibraryPointers;
        TarjanNode<Dependent>::findSCCS();
        // TODO
        // This gives error which should not be the case.
        TarjanNode<Dependent>::checkForCycle([](Dependent *dependent) -> string {
            // return getStringFromVariant(dependent);
            return "dsd";
        });
        vector<Dependent *> topologicallySorted = TarjanNode<Dependent>::topologicalSort;

        // TODO
        // Not confirm on the order of the topologicallySorted. Assuming that next ones are dependencies of previous
        // ones
        for (auto it = topologicallySorted.begin(); it != topologicallySorted.end(); ++it)
        {
            Dependent *dependent = it.operator*();
            if (dependent->isTarget)
            {
                auto *target = static_cast<Target *>(dependent);
                for (Library *library : target->publicLibs)
                {
                    target->publicLibs.insert(library->publicLibs.begin(), library->publicLibs.end());
                    target->publicPrebuilts.insert(library->publicPrebuilts.begin(), library->publicPrebuilts.end());
                    target->publicPackagedLibs.insert(library->publicPackagedLibs.begin(),
                                                      library->publicPackagedLibs.end());
                }

                // TODO: Following is added to compile the examples. However, the code should also add other public
                // properties from public dependencies
                // Modify PLibrary to be dependent of Dependency, thus having public properties.
                for (Library *lib : target->publicLibs)
                {
                    for (Directory &idd : lib->publicIncludes)
                    {
                        target->publicIncludes.emplace_back(idd);
                    }
                }
            }
        }
    }

    Json variantJson;
    variantJson[JConsts::configuration] = configurationType;
    variantJson[JConsts::compiler] = compiler;
    variantJson[JConsts::linker] = linker;
    variantJson[JConsts::archiver] = archiver;
    variantJson[JConsts::compilerFlags] = "";
    variantJson[JConsts::linkerFlags] = "";
    variantJson[JConsts::libraryType] = libraryType;
    variantJson[JConsts::environment] = environment;

    vector<string> targetArray;

    auto parseExesAndLibs = [&](string (Target::*func)(unsigned long count) const) {
        for (const auto &exe : executables)
        {
            targetArray.emplace_back(((*exe).*func)(variantCount));
        }
        for (const auto &lib : libraries)
        {
            targetArray.emplace_back((*(lib).*func)(variantCount));
        }
    };

    mode == VariantMode::PROJECT ? parseExesAndLibs(&Target::getTargetFilePath)
                                 : parseExesAndLibs(&Target::getTargetFilePathPackage);

    vector<string> targetsWithModulesStringVector;

    for (Executable *exe : executables)
    {
        targetsWithModulesStringVector.emplace_back(exe->getTargetFilePath(variantCount));
    }
    for (Library *lib : libraries)
    {
        targetsWithModulesStringVector.emplace_back(lib->getTargetFilePath(variantCount));
    }

    variantJson[JConsts::targets] = targetArray;
    variantJson[JConsts::targetsWithModules] = targetsWithModulesStringVector;

    path variantFilePath;
    if (mode == VariantMode::PROJECT)
    {
        variantFilePath = Cache::configureDirectory.directoryPath / to_string(variantCount) / "projectVariant.hmake";
    }
    else
    {
        variantFilePath =
            Cache::configureDirectory.directoryPath / "Package" / to_string(variantCount) / "packageVariant.hmake";
    }

    // TODO
    // Currently, not configuring Prebuilt and Paackaged Libs;
    if (mode == VariantMode::PROJECT)
    {
        for (Executable *executable : executables)
        {
            executable->configure(variantCount);
        }
        for (Library *library : libraries)
        {
            library->configure(variantCount);
        }
    }
    else
    {
        for (Executable *executable : executables)
        {
            executable->configure(package, variantCount);
        }
        for (Library *library : libraries)
        {
            library->configure(package, variantCount);
        }
    }

    ofstream(variantFilePath) << variantJson.dump(4);
}

Executable &Variant::findExecutable(const string &name)
{
    Executable executable(name);
    auto it = executables.find(&executable);
    if (it == executables.end())
    {
        fmt::print(stderr, "Could Not find the Executable");
    }
    return **it;
}

Library &Variant::findLibrary(const string &name)
{
    Library library(name);
    auto it = libraries.find(&library);
    if (it == libraries.end())
    {
        fmt::print(stderr, "Could Not find the library");
    }
    return **it;
}

Executable &Variant::addExecutable(const string &exeName)
{
    return *executablesContainer.emplace_back(make_shared<Executable>(exeName, *this)).get();
}

Library &Variant::addLibrary(const string &libName)
{
    return *librariesContainer.emplace_back(make_shared<Library>(libName, *this)).get();
}

ProjectVariant::ProjectVariant(struct Project &project)
{
    project.projectVariants.emplace_back(this);
}

Version::Version(unsigned int majorVersion_, unsigned int minorVersion_, unsigned int patchVersion_)
    : majorVersion{majorVersion_}, minorVersion{minorVersion_}, patchVersion{patchVersion_}
{
}

void to_json(Json &j, const Version &p)
{
    j = to_string(p.majorVersion) + "." + to_string(p.minorVersion) + "." + to_string(p.patchVersion);
}

void from_json(const Json &j, Version &v)
{
    string jString = j;
    char delim = '.';
    stringstream ss(jString);
    string item;
    int count = 0;
    while (getline(ss, item, delim))
    {
        if (count == 0)
        {
            v.majorVersion = stoi(item);
        }
        else if (count == 1)
        {
            v.minorVersion = stoi(item);
        }
        else
        {
            v.patchVersion = stoi(item);
        }
        ++count;
    }
}

bool operator<(const Version &lhs, const Version &rhs)
{
    if (lhs.majorVersion < rhs.majorVersion)
    {
        return true;
    }
    else if (lhs.majorVersion == rhs.majorVersion)
    {
        if (lhs.minorVersion < rhs.minorVersion)
        {
            return true;
        }
        else if (lhs.minorVersion == rhs.minorVersion)
        {
            if (lhs.patchVersion < rhs.patchVersion)
            {
                return true;
            }
        }
    }
    return false;
}

bool operator>(const Version &lhs, const Version &rhs)
{
    return rhs < lhs;
}

bool operator<=(const Version &lhs, const Version &rhs)
{
    return !(lhs > rhs);
}

bool operator>=(const Version &lhs, const Version &rhs)
{
    return !(lhs < rhs);
}

Environment Environment::initializeEnvironmentFromVSBatchCommand(const string &command)
{
    string temporaryIncludeFilename = "temporaryInclude.txt";
    string temporaryLibFilename = "temporaryLib.txt";
    string temporaryBatchFilename = "temporaryBatch.bat";
    ofstream(temporaryBatchFilename) << "call " + command << endl
                                     << "echo %INCLUDE% > " + temporaryIncludeFilename << endl
                                     << "echo %LIB%;%LIBPATH% > " + temporaryLibFilename;

    if (int code = system(temporaryBatchFilename.c_str()); code == EXIT_FAILURE)
    {
        cout << "Error in Executing Batch File" << endl;
        exit(-1);
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths) -> set<Directory> {
        set<Directory> separatedPaths{};
        unsigned long pos = accumulatedPaths.find(';');
        while (pos != std::string::npos)
        {
            std::string token = accumulatedPaths.substr(0, pos);
            if (token.empty())
            {
                break; // Only In Release Configuration in Visual Studio, for some reason the while loop also run with
                // empty string. Not investigating further for now
            }
            separatedPaths.emplace(path(token));
            accumulatedPaths.erase(0, pos + 1);
            pos = accumulatedPaths.find(';');
        }
        return separatedPaths;
    };

    Environment environment;
    string accumulatedPaths = file_to_string(temporaryIncludeFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryIncludeFilename);
    environment.includeDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    accumulatedPaths = file_to_string(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryLibFilename);
    environment.libraryDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    environment.compilerFlags = " /EHsc /MD /nologo";
    environment.linkerFlags = " /SUBSYSTEM:CONSOLE /NOLOGO";
    return environment;
    // return Environment{};
}

Environment Environment::initializeEnvironmentOnLinux()
{
    // Maybe run cpp -v and then parse the output.
    // https://stackoverflow.com/questions/28688484/actual-default-linker-script-and-settings-gcc-uses
    return {};
}

void to_json(Json &j, const Environment &environment)
{
    j[JConsts::includeDirectories] = environment.includeDirectories;
    j[JConsts::libraryDirectories] = environment.libraryDirectories;
    j[JConsts::compilerFlags] = environment.compilerFlags;
    j[JConsts::linkerFlags] = environment.linkerFlags;
}

void from_json(const Json &j, Environment &environment)
{
    environment.includeDirectories = j.at(JConsts::includeDirectories).get<set<Directory>>();
    environment.libraryDirectories = j.at(JConsts::libraryDirectories).get<set<Directory>>();
    environment.compilerFlags = j.at(JConsts::compilerFlags).get<string>();
    environment.linkerFlags = j.at(JConsts::linkerFlags).get<string>();
}

void Project::configure()
{
    for (unsigned long i = 0; i < projectVariants.size(); ++i)
    {
        projectVariants[i]->configure(VariantMode::PROJECT, i, Package(""));
    }
    Json projectFileJson;
    vector<string> projectVariantsInt;
    for (unsigned long i = 0; i < projectVariants.size(); ++i)
    {
        projectVariantsInt.emplace_back(to_string(i));
    }
    projectFileJson[JConsts::variants] = projectVariantsInt;
    ofstream(Cache::configureDirectory.directoryPath / "project.hmake") << projectFileJson.dump(4);
}

Package::Package(string name_) : name(move(name_)), version{0, 0, 0}
{
}

void Package::configureCommonAmongVariants()
{
    struct NonCommonIncludeDir
    {
        const Directory *directory;
        bool isCommon = false;
        int variantIndex;
        unsigned long indexInPackageCommonTargetsVector;
    };
    struct CommonIncludeDir
    {
        vector<unsigned long> variantsIndices;
        vector<Directory *> directories;
    };
    vector<NonCommonIncludeDir> packageNonCommonIncludeDirs;
    vector<CommonIncludeDir> packageCommonIncludeDirs;

    auto isDirectoryCommonOrNonCommon = [&](Directory *includeDirectory, unsigned long index) {
        bool matched = false;
        for (auto &i : packageNonCommonIncludeDirs)
        {
            if (i.directory->directoryPath == includeDirectory->directoryPath)
            {
                if (i.isCommon)
                {
                    packageCommonIncludeDirs[i.indexInPackageCommonTargetsVector].variantsIndices.emplace_back(index);
                }
                else
                {
                    CommonIncludeDir includeDir;
                    includeDir.directories.emplace_back(const_cast<Directory *>(i.directory));
                    includeDir.directories.emplace_back(includeDirectory);
                    includeDir.variantsIndices.emplace_back(i.variantIndex);
                    includeDir.variantsIndices.emplace_back(index);
                    packageCommonIncludeDirs.emplace_back(includeDir);
                    i.isCommon = true;
                    i.indexInPackageCommonTargetsVector = packageCommonIncludeDirs.size() - 1;
                }
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            NonCommonIncludeDir includeDir;
            includeDir.directory = includeDirectory;
            includeDir.variantIndex = index;
            packageNonCommonIncludeDirs.emplace_back(includeDir);
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirsForSingleTarget = [&](Target *target, unsigned long index) {
        for (auto &idd : target->publicIncludes)
        {
            isDirectoryCommonOrNonCommon(&(idd), index);
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary = [&](PLibrary *pLibrary, unsigned long index) {
        for (auto &id : pLibrary->includes)
        {
            isDirectoryCommonOrNonCommon(&id, index);
        }
    };

    // TODO
    // Don't understand what is happening. Packages need to be more generice
    /*    auto assignPackageCommonAndNonCommonIncludeDirs = [&](const Target *target, unsigned long index) {
            assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(const_cast<Target *>(target), index);
            auto dependencies = LibraryDependency::getDependencies(*target);
            for (const auto &libraryDependency : dependencies)
            {
                if (libraryDependency->ldlt == LDLT::LIBRARY)
                {
                    auto &lib = const_cast<Library &>(libraryDependency->library);
                    assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(&lib, index);
                }
                else
                {
                    auto &pLib = const_cast<PLibrary &>(libraryDependency->pLibrary);
                    assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary(&pLib, index);
                }
            }
        };

        for (unsigned long i = 0; i < packageVariants.size(); ++i)
        {
            for (const auto &exe : packageVariants[i].executables)
            {
                assignPackageCommonAndNonCommonIncludeDirs(*exe, i);
            }
            for (const auto &lib : packageVariants[i].libraries)
            {
                assignPackageCommonAndNonCommonIncludeDirs(*lib, i);
            }
        }*/

    Json commonDirsJson;
    for (unsigned long i = 0; i < packageCommonIncludeDirs.size(); ++i)
    {
        for (auto j : packageCommonIncludeDirs[i].directories)
        {
            j->isCommon = true;
            j->commonDirectoryNumber = i;
        }

        Json commonDirJsonObject;
        commonDirJsonObject[JConsts::index] = i;
        commonDirJsonObject[JConsts::path] = *packageCommonIncludeDirs[i].directories[0];
        commonDirJsonObject[JConsts::variantsIndices] = packageCommonIncludeDirs[i].variantsIndices;
        commonDirsJson.emplace_back(commonDirJsonObject);
    }
    create_directory(Cache::configureDirectory.directoryPath / "Package");
    ofstream(Cache::configureDirectory.directoryPath / "Package" / path("Common.hmake")) << commonDirsJson.dump(4);
}

void Package::configure()
{
    checkForSimilarJsonsInPackageVariants();

    Json packageFileJson;
    Json packageVariantJson;
    int count = 0;
    for (auto &i : packageVariants)
    {
        if (i->uniqueJson.contains(JConsts::index))
        {
            cerr << "Package Variant Json can not have COUNT in it's Json." << endl;
            exit(EXIT_FAILURE);
        }
        i->uniqueJson[JConsts::index] = to_string(count);
        packageVariantJson.emplace_back(i->uniqueJson);
        ++count;
    }
    packageFileJson[JConsts::name] = name;
    packageFileJson[JConsts::version] = version;
    packageFileJson[JConsts::cacheIncludes] = cacheCommonIncludeDirs;
    packageFileJson[JConsts::packageCopy] = Cache::copyPackage;
    packageFileJson[JConsts::packageCopyPath] = Cache::packageCopyPath;
    packageFileJson[JConsts::variants] = packageVariantJson;
    path packageDirectorypath = Cache::configureDirectory.directoryPath / "Package";
    create_directory(packageDirectorypath);
    Directory packageDirectory(packageDirectorypath);
    path file = packageDirectory.directoryPath / "package.hmake";
    ofstream(file) << packageFileJson.dump(4);

    if (Cache::copyPackage && cacheCommonIncludeDirs)
    {
        configureCommonAmongVariants();
    }
    count = 0;
    for (auto &variant : packageVariants)
    {
        variant->configure(VariantMode::PACKAGE, count, *this);
        ++count;
    }
}

void Package::checkForSimilarJsonsInPackageVariants()
{
    // Check that no two JSON'S of packageVariants of package are same
    set<nlohmann::json> checkSet;
    for (auto &i : packageVariants)
    {
        // TODO: Implement this check correctly.
        // This associatedJson is ordered_json, thus two different json's equality test will be false if the order is
        // different even if they have same elements. Thus we first convert it into json normal which is unordered one.
        /* nlohmann::json j = i.json;
        if (auto [pos, ok] = checkSet.emplace(j); !ok) {
          throw logic_error("No two json's of packagevariants can be same. Different order with same values does
        not make two Json's different");
        }*/
    }
}

PLibrary::PLibrary(Variant &variant, const path &libraryPath_, LibraryType libraryType_)
{
    variant.preBuiltLibraries.emplace(this);
    setLibraryType(libraryType_);
    if (libraryPath_.is_relative())
    {
        libraryPath = Cache::sourceDirectory.directoryPath / libraryPath_;
        libraryPath = libraryPath.lexically_normal();
    }
    libraryName = getTargetNameFromActualName(libraryType == LibraryType::STATIC ? TargetType::PLIBRARY_STATIC
                                                                                 : TargetType::SHARED,
                                              Cache::osFamily, libraryPath.filename().string());
}

path PLibrary::getTargetVariantDirectoryPath(int variantCount) const
{
    return Cache::configureDirectory.directoryPath / "Package" / to_string(variantCount) / libraryName;
}

void PLibrary::setLibraryType(LibraryType libraryType_)
{
    libraryType = libraryType_;
    if (libraryType == LibraryType::STATIC)
    {
        targetType = TargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = TargetType::PLIBRARY_SHARED;
    }
}

// Only Called in case of Package
Json PLibrary::convertToJson(const Package &package, unsigned count) const
{

    Json targetFileJson;

    Json includeDirectoriesArray;
    vector<Json> dependenciesArray;

    vector<string> consumerIncludeDirectories;
    string consumerCompilerFlags;
    string consumerLinkerFlags;
    vector<Json> consumerCompileDefinitionsArray;
    vector<Json> consumerDependenciesArray;

    for (const Directory &dir : includes)
    {
        Json JIDDObject;
        JIDDObject[JConsts::path] = dir;
        JIDDObject[JConsts::copy] = true;
        if (package.cacheCommonIncludeDirs && dir.isCommon)
        {
            consumerIncludeDirectories.emplace_back("../../Common/" + to_string(dir.commonDirectoryNumber) +
                                                    "/include/");
            JIDDObject[JConsts::copy] = false;
        }
        else
        {
            consumerIncludeDirectories.emplace_back("include/");
            JIDDObject[JConsts::copy] = true;
        }
        includeDirectoriesArray.emplace_back(JIDDObject);
    }

    consumerCompilerFlags.append(" " + compilerFlags + " ");
    consumerLinkerFlags.append(" " + linkerFlags + " ");

    for (const auto &e : compileDefinitions)
    {
        Json compileDefinitionObject = e;
        consumerCompileDefinitionsArray.emplace_back(compileDefinitionObject);
    }

    // TODO
    // We

    /*    vector<const LibraryDependency *> dependencies;
        dependencies = LibraryDependency::getDependencies(*this);
        for (const auto &libraryDependency : dependencies)
        {
            if (libraryDependency->ldlt == LDLT::LIBRARY)
            {
                const Library &library = libraryDependency->library;
                for (const auto &e : library.publicIncludes)
                {
                    Json JIDDObject;
                    JIDDObject[JConsts::path] = e;
                    JIDDObject[JConsts::copy] = false;
                    includeDirectoriesArray.emplace_back(JIDDObject);
                }

                Json libDepObject;
                libDepObject[JConsts::prebuilt] = false;
                Json consumeLibDepOject;
                consumeLibDepOject[JConsts::importedFromOtherHmakePackage] = false;
                consumeLibDepOject[JConsts::name] = library.targetName;
                consumerDependenciesArray.emplace_back(consumeLibDepOject);
                libDepObject[JConsts::path] = library.getTargetFilePathPackage(count);
                dependenciesArray.emplace_back(libDepObject);
            }
            else
            {
                Json libDepObject;
                libDepObject[JConsts::prebuilt] = true;
                Json consumeLibDepOject;
                if (libraryDependency->ldlt == LDLT::PLIBRARY)
                {
                    const auto &pLibrary = libraryDependency->pLibrary;
                    for (const auto &e : pLibrary.includes)
                    {
                        Json JIDDObject;
                        JIDDObject[JConsts::path] = e;
                        JIDDObject[JConsts::copy] = false;

                        includeDirectoriesArray.emplace_back(JIDDObject);
                    }

                    libDepObject[JConsts::path] = pLibrary.libraryPath.generic_string();
                    libDepObject[JConsts::importedFromOtherHmakePackage] = false;
                    libDepObject[JConsts::hmakeFilePath] =
                        pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
                    consumeLibDepOject[JConsts::importedFromOtherHmakePackage] = false;
                    consumeLibDepOject[JConsts::name] = pLibrary.libraryName;
                }
                else
                {
                    const auto &ppLibrary = libraryDependency->ppLibrary;
                    for (const auto &e : ppLibrary.includes)
                    {
                        Json JIDDObject;
                        JIDDObject[JConsts::path] = e;
                        JIDDObject[JConsts::copy] = false;
                        includeDirectoriesArray.emplace_back(JIDDObject);
                    }

                    libDepObject[JConsts::path] = ppLibrary.libraryPath.generic_string();
                    libDepObject[JConsts::importedFromOtherHmakePackage] = ppLibrary.importedFromOtherHMakePackage;
                    if (!ppLibrary.importedFromOtherHMakePackage)
                    {
                        libDepObject[JConsts::hmakeFilePath] =
                            ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
                    }

                    consumeLibDepOject[JConsts::importedFromOtherHmakePackage] =
       ppLibrary.importedFromOtherHMakePackage; consumeLibDepOject[JConsts::name] = ppLibrary.libraryName; if
       (ppLibrary.importedFromOtherHMakePackage)
                    {
                        consumeLibDepOject[JConsts::packageName] = ppLibrary.packageName;
                        consumeLibDepOject[JConsts::packageVersion] = ppLibrary.packageVersion;
                        consumeLibDepOject[JConsts::packagePath] = ppLibrary.packagePath.generic_string();
                        consumeLibDepOject[JConsts::useIndex] = ppLibrary.useIndex;
                        consumeLibDepOject[JConsts::index] = ppLibrary.index;
                        consumeLibDepOject[JConsts::packageVariantJson] = ppLibrary.packageVariantJson;
                    }
                }
                consumerDependenciesArray.emplace_back(consumeLibDepOject);
                dependenciesArray.emplace_back(libDepObject);
            }
        }*/

    targetFileJson[JConsts::targetType] = targetType;
    targetFileJson[JConsts::name] = libraryName;
    targetFileJson[JConsts::path] = libraryPath.generic_string();
    targetFileJson[JConsts::libraryDependencies] = dependenciesArray;
    targetFileJson[JConsts::includeDirectories] = includeDirectoriesArray;

    Json consumerDependenciesJson;
    consumerDependenciesJson[JConsts::libraryType] = libraryType;
    consumerDependenciesJson[JConsts::libraryDependencies] = consumerDependenciesArray;
    consumerDependenciesJson[JConsts::includeDirectories] = consumerIncludeDirectories;
    consumerDependenciesJson[JConsts::compilerTransitiveFlags] = consumerCompilerFlags;
    consumerDependenciesJson[JConsts::linkerTransitiveFlags] = consumerLinkerFlags;
    consumerDependenciesJson[JConsts::compileDefinitions] = consumerCompileDefinitionsArray;

    targetFileJson[JConsts::consumerDependencies] = consumerDependenciesJson;
    targetFileJson[JConsts::variant] = JConsts::package;
    targetFileJson[JConsts::packageCopy] = Cache::copyPackage;
    if (Cache::copyPackage)
    {
        targetFileJson[JConsts::packageName] = package.name;
        targetFileJson[JConsts::packageVersion] = package.version;
        targetFileJson[JConsts::packageCopyPath] = Cache::packageCopyPath;
        targetFileJson[JConsts::packageVariantIndex] = count;
    }
    return targetFileJson;
}

void PLibrary::configure(const Package &package, unsigned count) const
{
    Json targetFileJson;
    targetFileJson = convertToJson(package, count);
    path targetFileBuildDir = Cache::configureDirectory.directoryPath / "Package" / to_string(count) / libraryName;
    path filePath = targetFileBuildDir / (libraryName + ".hmake");
    create_directories(filePath.parent_path());
    ofstream(filePath.string()) << targetFileJson.dump(4);
}

bool operator<(const PLibrary &lhs, const PLibrary &rhs)
{
    /*    LibraryType libraryType;

  public:
    string libraryName;
    vector<Directory> includes;
    string compilerFlags;
    string linkerFlags;
    vector<Define>
        compileDefinitions; path libraryPath;*/

    vector<tuple<string, string>> b;
    b[0] = {"dsd", "dsd"};
    b.emplace_back("dsd", "dsd");
    return lhs.libraryName < rhs.libraryName && lhs.libraryPath < rhs.libraryPath && lhs.includes < rhs.includes &&
           lhs.compilerFlags < rhs.compilerFlags && lhs.compilerFlags < rhs.compilerFlags;
}

PPLibrary::PPLibrary(Variant &variant, string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant)
{
    variant.packagedLibraries.emplace(this);
    libraryName = move(libraryName_);
    path libraryDirectoryPath;
    bool found = false;
    for (auto &i : directory_iterator(cpVariant.variantPath))
    {
        if (i.path().filename() == libraryName && i.is_directory())
        {
            libraryDirectoryPath = i;
            found = true;
            break;
        }
    }
    if (!found)
    {
        cerr << "Library " << libraryName << " Not Present In Package Variant" << endl;
        exit(EXIT_FAILURE);
    }

    path libraryFilePath = libraryDirectoryPath / (libraryName + ".hmake");
    if (!std::filesystem::exists(libraryFilePath))
    {
        cerr << "Library " << libraryName << " Does Not Exists. Searched For File " << endl
             << writePath(libraryFilePath);
        exit(EXIT_FAILURE);
    }

    Json libraryFileJson;
    ifstream(libraryFilePath) >> libraryFileJson;

    if (libraryFileJson.at(JConsts::libraryType).get<string>() == JConsts::static_)
    {
        libraryType = LibraryType::STATIC;
    }
    else
    {
        libraryType = LibraryType::SHARED;
    }

    // TODO
    //  Should also add these packages to the other variant, I guess
    auto addPPLibsToVariant = [](set<PPLibrary> &ppLibs, PPLibrary &ppLibrary) {
        auto [it, Ok] = ppLibs.emplace(ppLibrary);
    };
    Json ppLibs = libraryFileJson.at(JConsts::libraryDependencies).get<Json>();
    for (auto &i : ppLibs)
    {
        bool isImported = i.at(JConsts::importedFromOtherHmakePackage).get<bool>();
        if (!isImported)
        {
            string dependencyLibraryName = i.at(JConsts::name).get<string>();
            PPLibrary ppLibrary(variant, dependencyLibraryName, cPackage, cpVariant);
            addPPLibsToVariant(ppLibraries, ppLibrary);
        }
        else
        {
            CPackage pack(i.at(JConsts::packagePath).get<string>());
            string libName = i.at(JConsts::name).get<string>();
            if (i.at(JConsts::useIndex).get<bool>())
            {
                PPLibrary ppLibrary(variant, libName, pack, pack.getVariant(i.at(JConsts::index).get<int>()));
                addPPLibsToVariant(ppLibraries, ppLibrary);
            }
            else
            {
                PPLibrary ppLibrary(variant, libName, pack,
                                    pack.getVariant(i.at(JConsts::packageVariantJson).get<Json>()));
                addPPLibsToVariant(ppLibraries, ppLibrary);
            }
        }
    }
    vector<string> includeDirs = libraryFileJson.at(JConsts::includeDirectories).get<vector<string>>();
    for (auto &i : includeDirs)
    {
        Directory dir(libraryDirectoryPath / i);
        includes.emplace_back(dir);
    }
    compilerFlags = libraryFileJson.at(JConsts::compilerTransitiveFlags).get<string>();
    linkerFlags = libraryFileJson.at(JConsts::linkerTransitiveFlags).get<string>();
    if (!libraryFileJson.at(JConsts::compileDefinitions).empty())
    {
        compileDefinitions = libraryFileJson.at(JConsts::compileDefinitions).get<vector<Define>>();
    }
    libraryPath = libraryDirectoryPath /
                  path(getActualNameFromTargetName(libraryType == LibraryType::STATIC ? TargetType::PLIBRARY_STATIC
                                                                                      : TargetType::PLIBRARY_SHARED,
                                                   Cache::osFamily, libraryName));
    packageName = cPackage.name;
    packageVersion = cPackage.version;
    packagePath = cPackage.packagePath;
    packageVariantJson = cpVariant.variantJson;
    index = cpVariant.index;
}

CPVariant::CPVariant(path variantPath_, Json variantJson_, unsigned index_)
    : variantPath(move(variantPath_)), variantJson(move(variantJson_)), index(index_)
{
}

CPackage::CPackage(path packagePath_) : packagePath(move(packagePath_))
{
    if (packagePath.is_relative())
    {
        packagePath = Cache::sourceDirectory.directoryPath / packagePath;
    }
    packagePath = packagePath.lexically_normal();
    path packageFilePath = packagePath / "cpackage.hmake";
    if (!std::filesystem::exists(packageFilePath))
    {
        cerr << "Package File Path " << packageFilePath.generic_string() << " Does Not Exists " << endl;
        exit(-1);
    }
    Json packageFileJson;
    ifstream(packageFilePath) >> packageFileJson;
    name = packageFileJson[JConsts::name];
    version = packageFileJson[JConsts::version];
    variantsJson = packageFileJson[JConsts::variants];
}

CPVariant CPackage::getVariant(const Json &variantJson_)
{
    int numberOfMatches = 0;
    int matchIndex;
    for (unsigned long i = 0; i < variantsJson.size(); ++i)
    {
        bool matched = true;
        for (const auto &keyValuePair : variantJson_.items())
        {
            if (!variantsJson[i].contains(keyValuePair.key()) ||
                variantsJson[i][keyValuePair.key()] != keyValuePair.value())
            {
                matched = false;
                break;
            }
        }
        if (matched)
        {
            ++numberOfMatches;
            matchIndex = i;
        }
    }
    if (numberOfMatches == 1)
    {
        string index = variantsJson[matchIndex].at(JConsts::index).get<string>();
        return CPVariant(packagePath / index, variantJson_, stoi(index));
    }
    else if (numberOfMatches == 0)
    {
        cerr << "No Json in package " << writePath(packagePath) << " matches \n" << variantJson_.dump(4) << endl;
    }
    else if (numberOfMatches > 1)
    {
        cerr << "More than 1 Jsons in package " + writePath(packagePath) + " matches \n"
             << endl
             << to_string(variantJson_);
    }
    exit(EXIT_FAILURE);
}

CPVariant CPackage::getVariant(const int index)
{
    int numberOfMatches = 0;
    for (const auto &i : variantsJson)
    {
        if (i.at(JConsts::index).get<string>() == to_string(index))
        {
            return CPVariant(packagePath / to_string(index), i, index);
        }
    }
    cerr << "No Json in package " << writePath(packagePath) << " has index " << to_string(index) << endl;
    exit(EXIT_FAILURE);
}

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
        cerr << "Level should be in range 0-2" << endl;
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
    json[JConsts::compilerTransitiveFlags] = ccpSettings.compilerTransitiveFlags;
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
    ccpSettings.compilerTransitiveFlags = json.at(JConsts::compilerTransitiveFlags).get<bool>();
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
    json[JConsts::environmentLibraryDirectories] = lcpSettings.environmentLibraryDirectories;
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
    lcpSettings.environmentLibraryDirectories = json.at(JConsts::environmentLibraryDirectories).get<PathPrint>();
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
    json[JConsts::copyingPackage] = generalPrintSettings.copyingPackage;
    json[JConsts::copyingTarget] = generalPrintSettings.copyingTarget;
    json[JConsts::threadId] = generalPrintSettings.threadId;
}

void from_json(const Json &json, GeneralPrintSettings &generalPrintSettings)
{
    generalPrintSettings.preBuildCommandsStatement = json.at(JConsts::preBuildCommandsStatement).get<bool>();
    generalPrintSettings.preBuildCommands = json.at(JConsts::preBuildCommands).get<bool>();
    generalPrintSettings.postBuildCommandsStatement = json.at(JConsts::postBuildCommandsStatement).get<bool>();
    generalPrintSettings.postBuildCommands = json.at(JConsts::postBuildCommands).get<bool>();
    generalPrintSettings.copyingPackage = json.at(JConsts::copyingPackage).get<bool>();
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

string file_to_string(const string &file_name)
{
    ifstream file_stream{file_name};

    if (file_stream.fail())
    {
        // Error opening file.
        cerr << "Error opening file in file_to_string function " << file_name << endl;
        exit(-1);
    }

    std::ostringstream str_stream{};
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        cerr << "Error reading file in file_to_string function " << file_name << endl;
        exit(-1);
    }

    return str_stream.str();
}

vector<string> split(string str, const string &token)
{
    vector<string> result;
    while (!str.empty())
    {
        unsigned long index = str.find(token);
        if (index != string::npos)
        {
            result.emplace_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.empty())
            {
                result.emplace_back(str);
            }
        }
        else
        {
            result.emplace_back(str);
            str = "";
        }
    }
    return result;
}

void ADD_ENV_INCLUDES_TO_TARGET_MODULE_SRC(Target &moduleTarget)
{
    for (const Directory &directory : moduleTarget.environment.includeDirectories)
    {
        moduleTarget.MODULE_DIRECTORIES(directory.directoryPath.generic_string(), ".*");
    }
}

void privateFunctions::SEARCH_AND_ADD_FILE_FROM_ENV_INCL_TO_TARGET_MODULE_SRC(Target &moduleTarget,
                                                                              const string &moduleFileName)
{
    bool found = false;
    for (const Directory &directory : moduleTarget.environment.includeDirectories)
    {
        for (auto &f : directory_iterator(directory.directoryPath))
        {
            if (f.is_regular_file() && f.path().filename() == moduleFileName)
            {
                moduleTarget.MODULE_FILES(f.path().generic_string().c_str());
                found = true;
            }
        }
    }
    if (!found)
    {
        cerr << stderr, "{} could not be found in Environment include directories of Target {}\n", moduleFileName,
            moduleTarget.targetName;
        exit(EXIT_FAILURE);
    }
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif