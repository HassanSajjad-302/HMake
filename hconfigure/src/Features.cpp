#include "Features.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppTarget.hpp"

namespace
{
string_view getCxxStdVersionString(const CxxSTD standard, const BuildTool &tool)
{
    if (standard == CxxSTD::V_LATEST)
    {
        const Version compilerVersion = tool.bTVersion;
        if (tool.btSubFamily == BTSubFamily::CLANG)
        {
            if (compilerVersion >= Version{19})
            {
                return "2c";
            }
            if (compilerVersion >= Version{17})
            {
                return "23";
            }
            if (compilerVersion >= Version{10})
            {
                return "20";
            }
            if (compilerVersion >= Version{5})
            {
                return "17";
            }
            return "14";
        }

        if (compilerVersion >= Version{14})
        {
            return "2c";
        }
        if (compilerVersion >= Version{11})
        {
            return "23";
        }
        if (compilerVersion >= Version{8})
        {
            return "20";
        }
        if (compilerVersion >= Version{6})
        {
            return "17";
        }
        return "14";
    }

    switch (standard)
    {
    case CxxSTD::V_98:
        return "98";
    case CxxSTD::V_03:
        return "03";
    case CxxSTD::V_0x:
        return "0x";
    case CxxSTD::V_11:
        return "11";
    case CxxSTD::V_1y:
        return "1y";
    case CxxSTD::V_14:
        return "14";
    case CxxSTD::V_1z:
        return "1z";
    case CxxSTD::V_17:
        return "17";
    case CxxSTD::V_2a:
        return "2a";
    case CxxSTD::V_20:
        return "20";
    case CxxSTD::V_2b:
        return "2b";
    case CxxSTD::V_23:
        return "23";
    case CxxSTD::V_2c:
        return "2c";
    case CxxSTD::V_26:
        return "26";
    case CxxSTD::V_LATEST:
        break;
    }
    return {};
}
} // namespace

TemplateDepth::TemplateDepth(const unsigned long long templateDepth_) : templateDepth(templateDepth_)
{
}

Define::Define(string name_, string value_) : name{std::move(name_)}, value{std::move(value_)}
{
}

void IspcCompilerFeatures::setConfigType(const ConfigType configType_)
{
    configType = configType_;
    if (configType == ConfigType::DEBUG)
    {
        optimization = Optimization::OFF;
        debugSymbols = DebugSymbols::ON;
    }
    else if (configType == ConfigType::RELEASE)
    {
        optimization = Optimization::SPEED;
        debugSymbols = DebugSymbols::OFF;
    }
    else if (configType == ConfigType::PROFILE)
    {
        optimization = Optimization::SPEED;
        debugSymbols = DebugSymbols::ON;
    }
}

void IspcCompilerFeatures::initialize(const CppCompilerFeatures &cppFeatures)
{
    if (targetOs == TargetOS::NONE)
    {
        targetOs = cppFeatures.targetOs;
        if (targetOs == TargetOS::NONE)
        {
            targetOs = os == OS::NT ? TargetOS::WINDOWS : TargetOS::LINUX_;
        }
    }
    if (arch == Arch::NONE)
    {
        arch = cppFeatures.arch == Arch::NONE ? Arch::X86 : cppFeatures.arch;
    }
    if (addressModel == AddressModel::NONE)
    {
        addressModel = cppFeatures.addModel == AddressModel::NONE ? AddressModel::A_64 : cppFeatures.addModel;
    }
    if (configType == ConfigType::NONE)
    {
        setConfigType(cppFeatures.configType == ConfigType::NONE ? ConfigType::RELEASE : cppFeatures.configType);
    }
    if (targets.empty())
    {
        if (arch == Arch::X86)
        {
            targets = {"avx512skx-i32x8", "avx2", "avx", "sse4"};
        }
        else if (arch == Arch::ARM)
        {
            targets = {"neon"};
        }
    }
}

string IspcCompilerFeatures::getCompileCommand() const
{
    if (compiler == nullptr)
    {
        return {};
    }

    string_view targetOsName;
    switch (targetOs)
    {
    case TargetOS::WINDOWS:
        targetOsName = "windows";
        break;
    case TargetOS::LINUX_:
        targetOsName = "linux";
        break;
    case TargetOS::ANDROID:
        targetOsName = "android";
        break;
    case TargetOS::DARWIN:
        targetOsName = "macos";
        break;
    case TargetOS::IPHONE:
    case TargetOS::APPLETV:
        targetOsName = "ios";
        break;
    default:
        printErrorMessage("The selected target OS is not supported by the ISPC integration.");
    }

    string_view architecture;
    if (arch == Arch::X86)
    {
        architecture = addressModel == AddressModel::A_32 ? "x86" : "x86-64";
    }
    else if (arch == Arch::ARM)
    {
        architecture = addressModel == AddressModel::A_32 ? "arm" : "aarch64";
    }
    else
    {
        printErrorMessage("The selected architecture is not supported by the ISPC integration.");
    }
    if (targets.empty())
    {
        printErrorMessage("The ISPC target list must not be empty.");
    }

    string command;
    command.reserve(compiler->filePath.size() + compileDefinitions.size() * 24 + includeDirectories.size() * 64 + 128);
    command.push_back('"');
    command += compiler->filePath;
    command += "\" --target-os=";
    command += targetOsName;
    command += " --arch=";
    command += architecture;
    command += " --target=";
    for (auto target = targets.begin(); target != targets.end(); ++target)
    {
        if (target != targets.begin())
        {
            command.push_back(',');
        }
        command += *target;
    }
    command += " --emit-obj ";
    for (const Node *include : includeDirectories)
    {
        command += "-I\"";
        command += include->filePath;
        command += "\" ";
    }
    for (const string &definition : compileDefinitions)
    {
        if (!definition.contains("\\\\U") && !definition.contains("\\\\u"))
        {
            command += "-D";
            command += definition;
            command.push_back(' ');
        }
    }
    return command;
}

string IspcCompilerFeatures::getObjectFlags() const
{
    string flags;
    if (debugSymbols == DebugSymbols::ON)
    {
        flags += "-g ";
    }
    if (optimization == Optimization::SPEED)
    {
        flags += "-O3 ";
    }
    else if (optimization == Optimization::SPACE || optimization == Optimization::MINIMAL)
    {
        flags += "-O1 ";
    }
    else
    {
        flags += "-O0 ";
    }
    return flags;
}

string_view IspcCompilerFeatures::getObjectSuffix() const
{
    return targetOs == TargetOS::WINDOWS ? ".obj" : ".o";
}

string getActualNameFromTargetName(const TargetType bTargetType, const OS osLocal, const string &targetName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return targetName + (osLocal == OS::NT ? ".exe" : "");
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".lib" : ".a";
        return actualName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".dll" : ".so";
        return actualName;
    }
    printErrorMessage("Unsupported target platform.\nOnly the currently configured host target is supported.");
}

string getTargetNameFromActualName(const TargetType bTargetType, const OS osLocal, const string &actualName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return osLocal == OS::NT ? actualName + ".exe" : actualName;
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        const unsigned short eraseCount = osLocal == OS::NT ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string libName = actualName;
        // Removes lib from libName.so
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .so from libName.so or .dll from Name.dll
        const unsigned short eraseCount = osLocal == OS::NT ? 4 : 3;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    printErrorMessage("Unsupported target platform.\nOnly the currently configured host target is supported.");
}

string getSlashedExecutableName(const string &name)
{
    return os == OS::NT ? name + ".exe" : "./" + name;
}

LinkerFeatures::LinkerFeatures()
{
    // TODO
    // Not Detecting
    addModel = AddressModel::A_64;
    arch = Arch::X86;
    if constexpr (os == OS::NT)
    {
        targetOs = TargetOS::WINDOWS;
    }
    else if constexpr (os == OS::LINUX)
    {
        targetOs = TargetOS::LINUX_;
    }
    configurationType = ConfigType::RELEASE;
    setConfigType(configurationType);
    if (cache.isLinkerInToolsArray)
    {
        linker = toolsCache.vsTools[cache.selectedLinkerArrayIndex].linker;
    }
    else
    {
        linker = toolsCache.linkers[cache.selectedLinkerArrayIndex];
    }
    if (cache.isArchiverInToolsArray)
    {
        archiver = toolsCache.vsTools[cache.selectedArchiverArrayIndex].archiver;
    }
    else
    {
        archiver = toolsCache.archivers[cache.selectedArchiverArrayIndex];
    }
}

string LinkerFeatures::getLinkerFlags()
{
    string linkerFlags;
    if (linker.bTFamily == BTFamily::MSVC)
    {
        linkerFlags += " /NOLOGO /INCREMENTAL:NO";
        if (evaluate(LTO::ON))
        {
            linkerFlags += " /LTCG ";
        }
        if (evaluate(AddressSanitizer::ON))
        {
            linkerFlags += " -incremental:no ";
        }
        if (evaluate(Arch::X86))
        {
            if (evaluate(AddressModel::A_64))
            {
                linkerFlags += " /MACHINE:X64 ";
            }
            else if (evaluate(AddressModel::A_32))
            {
                linkerFlags += " /MACHINE:X86 ";
            }
        }
        else if (evaluate(Arch::ARM))
        {
            if (evaluate(AddressModel::A_64))
            {
                linkerFlags += " /MACHINE:ARM64 ";
            }
            else if (evaluate(AddressModel::A_32))
            {
                linkerFlags += " /MACHINE:ARM ";
            }
        }
        if (evaluate(DebugSymbols::ON))
        {
            linkerFlags += " /DEBUG ";
            if (evaluate(RuntimeDebugging::OFF))
            {
                linkerFlags += " /OPT:REF,ICF  ";
            }
        }
        if (evaluate(UserInterface::CONSOLE))
        {
            linkerFlags += " /subsystem:console ";
        }
        else if (evaluate(UserInterface::GUI))
        {
            linkerFlags += " /subsystem:windows ";
        }
        else if (evaluate(UserInterface::WINCE))
        {
            linkerFlags += " /subsystem:windowsce ";
        }
        else if (evaluate(UserInterface::NATIVE))
        {
            linkerFlags += " /subsystem:native ";
        }
        else if (evaluate(UserInterface::AUTO))
        {
            linkerFlags += " /subsystem:posix ";
        }
    }
    else if (linker.bTFamily == BTFamily::GCC)
    {
        if (evaluate(Threading::MULTI))
        {
            if (evaluate(TargetOS::WINDOWS) || evaluate(TargetOS::CYGWIN))
            {
                linkerFlags += " -mthreads ";
            }
            else if (evaluate(TargetOS::QNX) || evaluate(TargetOS::FREEBSD) || evaluate(TargetOS::OPENBSD))
            {
                linkerFlags += " -pthread ";
            }
            else if (!evaluate(TargetOS::ANDROID) && !evaluate(TargetOS::DARWIN) && !evaluate(TargetOS::IPHONE) &&
                     !evaluate(TargetOS::APPLETV))
            {
                linkerFlags += " -pthread ";
            }
        }

        linkerFlags += (cxxStdDialect == CxxSTDDialect::GNU ? " -std=gnu++" : " -std=c++");
        if (cxxStd == CxxSTD::V_LATEST)
        {
            if (linker.bTVersion >= Version{10})
            {
                linkerFlags += "20 ";
            }
            else if (linker.bTVersion >= Version{8})
            {
                linkerFlags += "2a ";
            }
            else if (linker.bTVersion >= Version{6})
            {
                linkerFlags += "17 ";
            }
            else if (linker.bTVersion >= Version{5})
            {
                linkerFlags += "1z ";
            }
            else if (linker.bTVersion >= Version{4, 9})
            {
                linkerFlags += "14 ";
            }
            else if (linker.bTVersion >= Version{4, 8})
            {
                linkerFlags += "1y ";
            }
            else if (linker.bTVersion >= Version{4, 7})
            {
                linkerFlags += "11 ";
            }
            else if (linker.bTVersion >= Version{3, 3})
            {
                linkerFlags += "98 ";
            }
        }
        else
        {
            switch (cxxStd)
            {
            case CxxSTD::V_98:
                linkerFlags += "98 ";
                break;
            case CxxSTD::V_03:
                linkerFlags += "03 ";
                break;
            case CxxSTD::V_0x:
                linkerFlags += "0x ";
                break;
            case CxxSTD::V_11:
                linkerFlags += "11 ";
                break;
            case CxxSTD::V_1y:
                linkerFlags += "1y ";
                break;
            case CxxSTD::V_14:
                linkerFlags += "14 ";
                break;
            case CxxSTD::V_1z:
                linkerFlags += "1z ";
                break;
            case CxxSTD::V_17:
                linkerFlags += "17 ";
                break;
            case CxxSTD::V_2a:
                linkerFlags += "2a ";
                break;
            case CxxSTD::V_20:
                linkerFlags += "20 ";
                break;
            case CxxSTD::V_2b:
                linkerFlags += "2b ";
                break;
            case CxxSTD::V_23:
                linkerFlags += "23 ";
                break;
            case CxxSTD::V_2c:
                linkerFlags += "2c ";
                break;
            case CxxSTD::V_26:
                linkerFlags += "26 ";
                break;
            default:
                break;
            }
        }

        linkerFlags += " -x c++ ";

        if (evaluate(AddressSanitizer::ON))
        {
            linkerFlags += " -fsanitize=address -fno-omit-frame-pointer ";
        }
        else if (evaluate(AddressSanitizer::NORECOVER))
        {
            linkerFlags += " -fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer ";
        }
        if (evaluate(LeakSanitizer::ON))
        {
            linkerFlags += " -fsanitize=leak -fno-omit-frame-pointer ";
        }
        else if (evaluate(LeakSanitizer::NORECOVER))
        {
            linkerFlags += " -fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer ";
        }
        if (evaluate(ThreadSanitizer::ON))
        {
            linkerFlags += " -fsanitize=thread -fno-omit-frame-pointer ";
        }
        else if (evaluate(ThreadSanitizer::NORECOVER))
        {
            linkerFlags += " -fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer ";
        }
        if (evaluate(UndefinedSanitizer::ON))
        {
            linkerFlags += " -fsanitize=undefined -fno-omit-frame-pointer ";
        }
        else if (evaluate(UndefinedSanitizer::NORECOVER))
        {
            linkerFlags += " -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer ";
        }
        if (evaluate(Coverage::ON))
        {
            linkerFlags += " --coverage ";
        }

        if (evaluate(LTO::ON))
        {
            linkerFlags += " -flto ";
        }

        if (evaluate(Strip::ON))
        {
            linkerFlags += " -Wl,--strip-all ";
        }

        if (evaluate(TargetOS::WINDOWS) && evaluate(RuntimeLink::STATIC))
        {
            linkerFlags += " -Wl,-Bstatic ";
        }
        else if (!evaluate(TargetOS::WINDOWS) && evaluate(RuntimeLink::STATIC))
        {
            linkerFlags += " -static ";
        }

        if (evaluate(Arch::X86))
        {
            if (evaluate(InstructionSet::native))
            {
                linkerFlags += " -march=native ";
            }
            else if (evaluate(InstructionSet::x86_64_v1))
            {
                linkerFlags += " -march=x86-64 ";
            }
            else if (evaluate(InstructionSet::x86_64_v2))
            {
                linkerFlags += " -march=x86-64-v2 ";
            }
            else if (evaluate(InstructionSet::x86_64_v3))
            {
                linkerFlags += " -march=x86-64-v3 ";
            }
            else if (evaluate(InstructionSet::x86_64_v4))
            {
                linkerFlags += " -march=x86-64-v4 ";
            }
        }

        if (evaluate(Visibility::HIDDEN))
        {
            linkerFlags += " -fvisibility=hidden -fvisibility-inlines-hidden ";
        }
        else if (evaluate(Visibility::GLOBAL))
        {
            linkerFlags += " -fvisibility=default ";
        }
    }
    return linkerFlags;
}

string LinkerFeatures::getLinkCommand() const
{
    string str;
    str += "\"" + linker.bTPath + "\" ";
    str += linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";

    if (!evaluate(TargetOS::WINDOWS) && !evaluate(TargetOS::CYGWIN))
    {
        str += "-fPIC ";
    }

    str += linker.bTFamily == BTFamily::MSVC ? " /OUT:\"" : " -o \"";

    return str;
}

string LinkerFeatures::getArchiveCommand() const
{
    string str;
    str += "\"" + archiver.bTPath + "\" ";
    str += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
    if (archiver.bTFamily == BTFamily::MSVC)
    {
        str += "/OUT:\"";
    }
    else if (archiver.bTFamily == BTFamily::GCC)
    {

        str += " rcs \"";
    }

    return str;
}

void LinkerFeatures::setConfigType(const ConfigType configType)
{
    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        debugSymbols = DebugSymbols::ON;
        runtimeDebugging = RuntimeDebugging::ON;
    }
    else if (configType == ConfigType::RELEASE)
    {
        runtimeDebugging = RuntimeDebugging::OFF;
        debugSymbols = DebugSymbols::OFF;
    }
    else if (configType == ConfigType::PROFILE)
    {
        debugSymbols = DebugSymbols::ON;
        profiling = Profiling::ON;
    }
}

void CppCompilerFeatures::initialize()
{
    // TODO
    // Not Detecting
    addModel = AddressModel::A_64;
    arch = Arch::X86;
    if (targetOs == TargetOS::NONE)
    {
        if constexpr (os == OS::NT)
        {
            targetOs = TargetOS::WINDOWS;
        }
        else if constexpr (os == OS::LINUX)
        {
            targetOs = TargetOS::LINUX_;
        }
    }
    if (configType == ConfigType::NONE)
    {
        setConfigType(ConfigType::RELEASE);
    }
    if (compiler.bTPath == "")
    {
        if (cache.isCompilerInToolsArray)
        {
            if constexpr (os == OS::NT)
            {
                compiler = toolsCache.vsTools[cache.selectedCompilerArrayIndex].compiler;
            }
            else
            {
                compiler = toolsCache.linuxTools[cache.selectedCompilerArrayIndex].compiler;
            }
        }
        else
        {
            compiler = toolsCache.compilers[cache.selectedCompilerArrayIndex];
        }
    }

    if constexpr (os == OS::NT)
    {
        compiler.btSubFamily = BTSubFamily::CLANG;
        compiler.bTPath = R"(c:\projects\llvm-project\llvm\cmake-build-release\bin\clang-cl.exe)";
    }
}

void CppCompilerFeatures::setConfigType(const ConfigType configType_)
{
    configType = configType_;
    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        optimization = Optimization::OFF;
        inlining = Inlining::OFF;
    }
    else if (configType == ConfigType::RELEASE)
    {
        optimization = Optimization::SPEED;
        inlining = Inlining::FULL;
    }

    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        debugSymbols = DebugSymbols::ON;
        runtimeDebugging = RuntimeDebugging::ON;
    }
    else if (configType == ConfigType::RELEASE)
    {
        runtimeDebugging = RuntimeDebugging::OFF;
        debugSymbols = DebugSymbols::OFF;
    }
    else if (configType == ConfigType::PROFILE)
    {
        debugSymbols = DebugSymbols::ON;
        profiling = Profiling::ON;
    }
}

// For some features the resultant object-file is same these are termed incidental. Change these does not result in
// recompilation. Skip these in compiler-command that is cached.
string CppCompilerFeatures::getCompilerFlags() const
{
    string compilerFlags;
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compilerFlags += " /nologo /FC /EHsc /c";
        if (evaluate(Warnings::ALL))
        {
            compilerFlags += " /W3";
        }
        else if (evaluate(Warnings::EXTRA))
        {
            compilerFlags += " /W4";
        }
        else if (evaluate(Warnings::OFF))
        {
            compilerFlags += " /w";
        }

        if (evaluate(Optimization::SPEED))
        {
            compilerFlags += " /O2";
        }
        else if (evaluate(Optimization::SPACE))
        {
            compilerFlags += " /O1";
        }
        else if (evaluate(Optimization::OFF))
        {
            compilerFlags += " /Od";
        }

        if (evaluate(DebugSymbols::ON))
        {
            if (evaluate(DebugStore::DATABASE))
            {
                compilerFlags += " /Zi";
            }
            else if (evaluate(DebugStore::OBJECT))
            {
                compilerFlags += " /Z7";
            }
        }

        if (evaluate(RTTI::ON))
        {
            compilerFlags += " /GR";
        }
        else if (evaluate(RTTI::OFF))
        {
            compilerFlags += " /GR-";
        }

        if (evaluate(WarningsAsErrors::ON))
        {
            compilerFlags += " /WX";
        }

        if (evaluate(RuntimeLink::SHARED))
        {
            if (evaluate(RuntimeDebugging::ON))
            {
                compilerFlags += " /MDd";
            }
            else if (evaluate(RuntimeDebugging::OFF))
            {
                compilerFlags += " /MD";
            }
        }
        else if (evaluate(RuntimeLink::STATIC))
        {
            if (evaluate(RuntimeDebugging::ON))
            {
                compilerFlags += " /MTd";
            }
            else if (evaluate(RuntimeDebugging::OFF))
            {
                compilerFlags += " /MT";
            }
        }

        if (evaluate(LTO::ON))
        {
            compilerFlags += " /GL";
        }

        if (cxxStd == CxxSTD::V_14)
        {
            compilerFlags += " /std:c++14";
        }
        else if (cxxStd == CxxSTD::V_17)
        {
            compilerFlags += " /std:c++17";
        }
        else if (cxxStd == CxxSTD::V_20)
        {
            compilerFlags += " /std:c++20";
        }
        else if (cxxStd == CxxSTD::V_23 || cxxStd == CxxSTD::V_2b || cxxStd == CxxSTD::V_LATEST)
        {
            compilerFlags += " /std:c++latest";
        }

        if (evaluate(Threading::MULTI))
        {
            compilerFlags += " /D_MT";
        }
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        if (evaluate(Threading::MULTI))
        {
            if (evaluate(TargetOS::WINDOWS) || evaluate(TargetOS::CYGWIN))
            {
                compilerFlags += " -mthreads";
            }
            else if (evaluate(TargetOS::QNX) || evaluate(TargetOS::FREEBSD) || evaluate(TargetOS::OPENBSD))
            {
                compilerFlags += " -pthread";
            }
            else if (!evaluate(TargetOS::ANDROID) && !evaluate(TargetOS::DARWIN) && !evaluate(TargetOS::IPHONE) &&
                     !evaluate(TargetOS::APPLETV))
            {
                compilerFlags += " -pthread";
            }
        }

        compilerFlags += (cxxStdDialect == CxxSTDDialect::GNU ? " -std=gnu++" : " -std=c++");
        if (cxxStd == CxxSTD::V_LATEST)
        {
            if (compiler.bTVersion >= Version{10})
            {
                compilerFlags += "20";
            }
            else if (compiler.bTVersion >= Version{8})
            {
                compilerFlags += "2a";
            }
            else if (compiler.bTVersion >= Version{6})
            {
                compilerFlags += "17";
            }
            else if (compiler.bTVersion >= Version{5})
            {
                compilerFlags += "1z";
            }
            else if (compiler.bTVersion >= Version{4, 9})
            {
                compilerFlags += "14";
            }
            else if (compiler.bTVersion >= Version{4, 8})
            {
                compilerFlags += "1y";
            }
            else if (compiler.bTVersion >= Version{4, 7})
            {
                compilerFlags += "11";
            }
            else if (compiler.bTVersion >= Version{3, 3})
            {
                compilerFlags += "98";
            }
        }
        else
        {
            switch (cxxStd)
            {
            case CxxSTD::V_98:
                compilerFlags += "98";
                break;
            case CxxSTD::V_03:
                compilerFlags += "03";
                break;
            case CxxSTD::V_0x:
                compilerFlags += "0x";
                break;
            case CxxSTD::V_11:
                compilerFlags += "11";
                break;
            case CxxSTD::V_1y:
                compilerFlags += "1y";
                break;
            case CxxSTD::V_14:
                compilerFlags += "14";
                break;
            case CxxSTD::V_1z:
                compilerFlags += "1z";
                break;
            case CxxSTD::V_17:
                compilerFlags += "17";
                break;
            case CxxSTD::V_2a:
                compilerFlags += "2a";
                break;
            case CxxSTD::V_20:
                compilerFlags += "20";
                break;
            case CxxSTD::V_2b:
                compilerFlags += "2b";
                break;
            case CxxSTD::V_23:
                compilerFlags += "23";
                break;
            case CxxSTD::V_2c:
                compilerFlags += "2c";
                break;
            case CxxSTD::V_26:
                compilerFlags += "26";
                break;
            default:
                break;
            }
        }

        compilerFlags += " -x c++";

        if (evaluate(AddressSanitizer::ON))
        {
            compilerFlags += " -fsanitize=address -fno-omit-frame-pointer";
        }
        else if (evaluate(AddressSanitizer::NORECOVER))
        {
            compilerFlags += " -fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer";
        }
        if (evaluate(LeakSanitizer::ON))
        {
            compilerFlags += " -fsanitize=leak -fno-omit-frame-pointer";
        }
        else if (evaluate(LeakSanitizer::NORECOVER))
        {
            compilerFlags += " -fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer";
        }
        if (evaluate(ThreadSanitizer::ON))
        {
            compilerFlags += " -fsanitize=thread -fno-omit-frame-pointer";
        }
        else if (evaluate(ThreadSanitizer::NORECOVER))
        {
            compilerFlags += " -fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer";
        }
        if (evaluate(UndefinedSanitizer::ON))
        {
            compilerFlags += " -fsanitize=undefined -fno-omit-frame-pointer";
        }
        else if (evaluate(UndefinedSanitizer::NORECOVER))
        {
            compilerFlags += " -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer";
        }
        if (evaluate(Coverage::ON))
        {
            compilerFlags += " --coverage";
        }

        if (evaluate(LTO::ON))
        {
            compilerFlags += " -flto";
        }

        if (evaluate(Warnings::ALL))
        {
            compilerFlags += " -Wall";
        }
        else if (evaluate(Warnings::EXTRA))
        {
            compilerFlags += " -Wextra";
        }
        else if (evaluate(Warnings::OFF))
        {
            compilerFlags += " -w";
        }

        if (evaluate(WarningsAsErrors::ON))
        {
            compilerFlags += " -Werror";
        }

        if (evaluate(Optimization::SPEED))
        {
            compilerFlags += " -O3";
        }
        else if (evaluate(Optimization::SPACE))
        {
            compilerFlags += " -Os";
        }
        else if (evaluate(Optimization::MINIMAL))
        {
            compilerFlags += " -O1";
        }
        else if (evaluate(Optimization::DEBUG))
        {
            compilerFlags += " -Og";
        }
        else if (evaluate(Optimization::OFF))
        {
            compilerFlags += " -O0";
        }

        if (evaluate(Inlining::OFF))
        {
            compilerFlags += " -fno-inline";
        }
        else if (evaluate(Inlining::FULL))
        {
            compilerFlags += " -finline-functions -Wno-inline";
        }

        if (evaluate(DebugSymbols::ON))
        {
            compilerFlags += " -g";
        }

        if (evaluate(Profiling::ON))
        {
            compilerFlags += " -pg";
        }

        if (evaluate(ExceptionHandling::OFF))
        {
            compilerFlags += " -fno-exceptions";
        }
        if (evaluate(RTTI::OFF))
        {
            compilerFlags += " -fno-rtti";
        }

        if (evaluate(Vectorize::ON))
        {
            compilerFlags += " -ftree-vectorize";
        }

        if (evaluate(Arch::X86))
        {
            if (evaluate(InstructionSet::native))
            {
                compilerFlags += " -march=native";
            }
            else if (evaluate(InstructionSet::x86_64_v1))
            {
                compilerFlags += " -march=x86-64";
            }
            else if (evaluate(InstructionSet::x86_64_v2))
            {
                compilerFlags += " -march=x86-64-v2";
            }
            else if (evaluate(InstructionSet::x86_64_v3))
            {
                compilerFlags += " -march=x86-64-v3";
            }
            else if (evaluate(InstructionSet::x86_64_v4))
            {
                compilerFlags += " -march=x86-64-v4";
            }
        }

        if (evaluate(Visibility::HIDDEN))
        {
            compilerFlags += " -fvisibility=hidden -fvisibility-inlines-hidden";
        }
        else if (evaluate(Visibility::GLOBAL))
        {
            compilerFlags += " -fvisibility=default";
        }
    }

    compilerFlags += " ";
    return compilerFlags;
}

string CppCompilerFeatures::getCompileCommand()
{
    initialize();
    string compileCommand;
    compileCommand += '\"' + compiler.bTPath + "\" ";
    compileCommand += getCompilerFlags();
    return compileCommand;
}

void CppCompilerFeatures::setCpuType()
{
    if (evaluate(Arch::X86))
    {
        cpuType = CpuType::AMD64;
    }
    else if (evaluate(Arch::ARM))
    {
        cpuType = CpuType::ARM;
    }
    else
    {
        cpuType = CpuType::NONE;
    }
}

bool CppCompilerFeatures::isCpuTypeG7()
{
    return false;
}
