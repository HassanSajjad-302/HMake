#include "Toolchains.hpp"

#if __has_include("BuiltinToolchain.hpp")
#include "BuiltinToolchain.hpp"
#endif

#if !defined(HMAKE_DEFAULT_TOOLCHAIN_NAME) || !defined(HMAKE_DEFAULT_COMPILER) || !defined(HMAKE_DEFAULT_LINKER) ||    \
    !defined(HMAKE_DEFAULT_ARCHIVER) || !defined(HMAKE_DEFAULT_TOOLCHAIN_FAMILY) ||                                    \
    !defined(HMAKE_DEFAULT_TOOLCHAIN_STYLE) || !defined(HMAKE_DEFAULT_TOOLCHAIN_VERSION) ||                            \
    !defined(HMAKE_DEFAULT_TARGET) || !defined(HMAKE_DEFAULT_INCLUDE_DIRS) || !defined(HMAKE_DEFAULT_LIBRARY_DIRS)
#error "The HMake built-in toolchain requires all HMAKE_DEFAULT_* definitions."
#endif

#include "BuildSystemFunctions.hpp"
#include "RunCommand.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <utility>

#ifdef __linux__
#include <unistd.h>
#endif

namespace
{
constexpr string_view extendsField = "extends";
constexpr string_view compilerField = "compiler";
constexpr string_view linkerField = "linker";
constexpr string_view archiverField = "archiver";
constexpr string_view familyField = "family";
constexpr string_view styleField = "style";
constexpr string_view versionField = "version";
constexpr string_view targetField = "target";
constexpr string_view includeDirsField = "include-dirs";
constexpr string_view libraryDirsField = "library-dirs";
constexpr string_view bootstrapArgumentsField = "bootstrap-arguments";

constexpr uint16_t toolchainFieldBit(const string_view field)
{
    if (field == extendsField)
    {
        return 1U << 0;
    }
    if (field == compilerField)
    {
        return 1U << 1;
    }
    if (field == linkerField)
    {
        return 1U << 2;
    }
    if (field == archiverField)
    {
        return 1U << 3;
    }
    if (field == familyField)
    {
        return 1U << 4;
    }
    if (field == styleField)
    {
        return 1U << 5;
    }
    if (field == versionField)
    {
        return 1U << 6;
    }
    if (field == targetField)
    {
        return 1U << 7;
    }
    if (field == includeDirsField)
    {
        return 1U << 8;
    }
    if (field == libraryDirsField)
    {
        return 1U << 9;
    }
    if (field == bootstrapArgumentsField)
    {
        return 1U << 10;
    }
    return 0;
}

[[noreturn]] void toolchainError(const path &filePath, const string_view toolchainName, const string_view message)
{
    printErrorMessage(FORMAT("Invalid toolchain registry.\nFile: {}\nToolchain: {}\n{}", filePath.string(),
                             toolchainName.empty() ? "<top-level>" : toolchainName, message));
}

string readRequiredString(const rapidjson::Value &value, const path &filePath, const string_view toolchainName,
                          const string_view field)
{
    if (!value.IsString())
    {
        toolchainError(filePath, toolchainName, FORMAT("Field '{}' must be a string.", field));
    }
    string result(value.GetString(), value.GetStringLength());
    if (result.find('\0') != string::npos)
    {
        toolchainError(filePath, toolchainName, FORMAT("Field '{}' must not contain a null byte.", field));
    }
    return result;
}

std::vector<string> readStringArray(const rapidjson::Value &value, const path &filePath,
                                    const string_view toolchainName, const string_view field)
{
    if (!value.IsArray())
    {
        toolchainError(filePath, toolchainName, FORMAT("Field '{}' must be an array of strings.", field));
    }
    std::vector<string> result;
    result.reserve(value.Size());
    for (const rapidjson::Value &entry : value.GetArray())
    {
        if (!entry.IsString())
        {
            toolchainError(filePath, toolchainName, FORMAT("Every '{}' entry must be a string.", field));
        }
        string item(entry.GetString(), entry.GetStringLength());
        if (item.find('\0') != string::npos)
        {
            toolchainError(filePath, toolchainName, FORMAT("Field '{}' contains a string with a null byte.", field));
        }
        result.emplace_back(std::move(item));
    }
    return result;
}

string lowercase(string value)
{
    std::ranges::transform(value, value.begin(),
                           [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

Version parseVersion(const string &text, const path &sourceFile, const string_view toolchainName)
{
    Version result;
    unsigned *parts[] = {&result.majorVersion, &result.minorVersion, &result.patchVersion};
    uint64_t offset = 0;
    uint64_t part = 0;
    while (offset < text.size())
    {
        uint64_t value = 0;
        const uint64_t start = offset;
        while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset])))
        {
            value = value * 10 + static_cast<unsigned>(text[offset] - '0');
            if (value > std::numeric_limits<unsigned>::max())
            {
                toolchainError(sourceFile, toolchainName, FORMAT("Version component is too large in '{}'.", text));
            }
            ++offset;
        }
        if (offset == start)
        {
            toolchainError(sourceFile, toolchainName,
                           FORMAT("Version '{}' must contain dot-separated decimal components.", text));
        }
        if (part < 3)
        {
            *parts[part] = static_cast<unsigned>(value);
        }
        ++part;
        if (offset == text.size())
        {
            break;
        }
        if (text[offset++] != '.' || offset == text.size())
        {
            toolchainError(sourceFile, toolchainName,
                           FORMAT("Version '{}' must contain dot-separated decimal components.", text));
        }
    }
    return result;
}

void parseTargetTriple(Toolchain &toolchain, const path &sourceFile)
{
    const string triple = lowercase(toolchain.target);
    const uint64_t separator = triple.find('-');
    const string_view architecture = string_view(triple).substr(0, separator);

    if (architecture == "x86_64" || architecture == "amd64")
    {
        toolchain.targetArch = Arch::X86;
        toolchain.targetAddressModel = AddressModel::A_64;
    }
    else if (architecture == "x86" || architecture == "i386" || architecture == "i486" || architecture == "i586" ||
             architecture == "i686")
    {
        toolchain.targetArch = Arch::X86;
        toolchain.targetAddressModel = AddressModel::A_32;
    }
    else if (architecture == "aarch64" || architecture == "arm64")
    {
        toolchain.targetArch = Arch::ARM;
        toolchain.targetAddressModel = AddressModel::A_64;
    }
    else if (architecture.starts_with("arm"))
    {
        toolchain.targetArch = Arch::ARM;
        toolchain.targetAddressModel = AddressModel::A_32;
    }
    else if (architecture == "s390x")
    {
        toolchain.targetArch = Arch::S390X;
        toolchain.targetAddressModel = AddressModel::A_64;
    }
    else if (architecture == "powerpc64" || architecture == "powerpc64le" || architecture == "ppc64" ||
             architecture == "ppc64le")
    {
        toolchain.targetArch = Arch::POWER;
        toolchain.targetAddressModel = AddressModel::A_64;
    }
    else if (architecture == "powerpc" || architecture == "ppc")
    {
        toolchain.targetArch = Arch::POWER;
        toolchain.targetAddressModel = AddressModel::A_32;
    }
    else if (architecture == "loongarch64")
    {
        toolchain.targetArch = Arch::LOONGARCH;
        toolchain.targetAddressModel = AddressModel::A_64;
    }
    else
    {
        toolchainError(sourceFile, toolchain.name,
                       FORMAT("Unsupported architecture in target triple '{}'.", toolchain.target));
    }

    if (triple.contains("android"))
    {
        toolchain.targetOs = TargetOS::ANDROID;
    }
    else if (triple.contains("linux"))
    {
        toolchain.targetOs = TargetOS::LINUX_;
    }
    else if (triple.contains("windows") || triple.contains("win32") || triple.contains("mingw") ||
             triple.ends_with("-msvc"))
    {
        toolchain.targetOs = TargetOS::WINDOWS;
    }
    else if (triple.contains("darwin") || triple.contains("macos") || triple.contains("apple"))
    {
        toolchain.targetOs = TargetOS::DARWIN;
    }
    else if (triple.contains("freebsd"))
    {
        toolchain.targetOs = TargetOS::FREEBSD;
    }
    else if (triple.contains("openbsd"))
    {
        toolchain.targetOs = TargetOS::OPENBSD;
    }
    else if (triple.contains("qnx"))
    {
        toolchain.targetOs = TargetOS::QNX;
    }
    else if (triple.contains("cygwin"))
    {
        toolchain.targetOs = TargetOS::CYGWIN;
    }
    else
    {
        toolchainError(sourceFile, toolchain.name,
                       FORMAT("Unsupported operating system in target triple '{}'.", toolchain.target));
    }
}

void initializeBuildTools(Toolchain &toolchain, const path &sourceFile)
{
    const string family = lowercase(toolchain.family);
    const string style = lowercase(toolchain.style);
    if (family != "clang" && family != "gcc" && family != "msvc")
    {
        toolchainError(sourceFile, toolchain.name,
                       FORMAT("Unsupported family '{}'. Expected clang, gcc, or msvc.", toolchain.family));
    }
    if (style != "gnu" && style != "msvc")
    {
        toolchainError(sourceFile, toolchain.name,
                       FORMAT("Unsupported style '{}'. Expected gnu or msvc.", toolchain.style));
    }
    if ((family == "gcc" && style != "gnu") || (family == "msvc" && style != "msvc"))
    {
        toolchainError(sourceFile, toolchain.name,
                       FORMAT("Family '{}' is incompatible with style '{}'.", toolchain.family, toolchain.style));
    }

    toolchain.family = family;
    toolchain.style = style;

    const BTFamily buildToolFamily = style == "msvc" ? BTFamily::MSVC : BTFamily::GCC;
    const BTSubFamily subFamily = family == "clang" ? BTSubFamily::CLANG : BTSubFamily::NONE;
    const Version version = parseVersion(toolchain.version, sourceFile, toolchain.name);
    toolchain.compiler = Compiler(buildToolFamily, subFamily, version, toolchain.compiler.bTPath);
    toolchain.linker = Linker(buildToolFamily, subFamily, version, toolchain.linker.bTPath);
    toolchain.archiver = Archiver(buildToolFamily, subFamily, version, toolchain.archiver.bTPath);
    parseTargetTriple(toolchain, sourceFile);
}

void errorOnMissingField(const bool missing, const path &sourceFile, const string &name, const string_view fieldName)
{
    if (missing)
    {
        toolchainError(sourceFile, name, FORMAT("Resolved entry is missing required field '{}'.", fieldName));
    }
}

void addJsonString(rapidjson::Value &object, const string_view name, const string &value,
                   rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value jsonName(name.data(), static_cast<rapidjson::SizeType>(name.size()), allocator);
    rapidjson::Value jsonValue(value.data(), static_cast<rapidjson::SizeType>(value.size()), allocator);
    object.AddMember(jsonName, jsonValue, allocator);
}

rapidjson::Value makeJsonStringArray(const std::vector<string> &values, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value result(rapidjson::kArrayType);
    result.Reserve(static_cast<rapidjson::SizeType>(values.size()), allocator);
    for (const string &value : values)
    {
        rapidjson::Value jsonValue(value.data(), static_cast<rapidjson::SizeType>(value.size()), allocator);
        result.PushBack(jsonValue, allocator);
    }
    return result;
}

rapidjson::Value toolchainToJson(const Toolchain &toolchain, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value result(rapidjson::kObjectType);
    addJsonString(result, compilerField, toolchain.compiler.bTPath, allocator);
    addJsonString(result, linkerField, toolchain.linker.bTPath, allocator);
    addJsonString(result, archiverField, toolchain.archiver.bTPath, allocator);
    addJsonString(result, familyField, toolchain.family, allocator);
    addJsonString(result, styleField, toolchain.style, allocator);
    addJsonString(result, versionField, toolchain.version, allocator);
    addJsonString(result, targetField, toolchain.target, allocator);
    const auto addArray = [&](const string_view name, const std::vector<string> &values) {
        rapidjson::Value jsonName(name.data(), static_cast<rapidjson::SizeType>(name.size()), allocator);
        rapidjson::Value array = makeJsonStringArray(values, allocator);
        result.AddMember(jsonName, array, allocator);
    };
    addArray(includeDirsField, toolchain.includeDirs);
    addArray(libraryDirsField, toolchain.libraryDirs);
    addArray(bootstrapArgumentsField, toolchain.bootstrapArguments);
    return result;
}
} // namespace

Toolchains::Toolchains()
{
    if constexpr (os == OS::NT)
    {
        if (const char *localAppData = std::getenv("LOCALAPPDATA"))
        {
            userToolchainsFilePath = path(localAppData) / "HMake" / "toolchains.json";
        }
    }
    else
    {
        if (const char *homeDirectory = std::getenv("HOME"))
        {
            userToolchainsFilePath = path(homeDirectory) / ".hmake" / "toolchains.json";
        }
    }
    Toolchain builtIn;
    builtIn.family = HMAKE_DEFAULT_TOOLCHAIN_FAMILY;
    builtIn.style = HMAKE_DEFAULT_TOOLCHAIN_STYLE;
    builtIn.version = HMAKE_DEFAULT_TOOLCHAIN_VERSION;
    builtIn.target = HMAKE_DEFAULT_TARGET;
    builtIn.compiler.bTPath = HMAKE_DEFAULT_COMPILER;
    builtIn.linker.bTPath = HMAKE_DEFAULT_LINKER;
    builtIn.archiver.bTPath = HMAKE_DEFAULT_ARCHIVER;
    builtIn.includeDirs = HMAKE_DEFAULT_INCLUDE_DIRS;
    builtIn.libraryDirs = HMAKE_DEFAULT_LIBRARY_DIRS;

    const string name = HMAKE_DEFAULT_TOOLCHAIN_NAME;
    const path sourceFile = "<builtin>";
    assert(!name.empty());
    assert(!entries.contains(name));
    builtIn.name = name;
    assert(!builtIn.compiler.bTPath.empty() && !builtIn.linker.bTPath.empty() && !builtIn.archiver.bTPath.empty() &&
           !builtIn.family.empty() && !builtIn.style.empty() && !builtIn.version.empty() && !builtIn.target.empty());
    initializeBuildTools(builtIn, sourceFile);
    registryOrder.emplace_back(&entries.emplace(name, std::move(builtIn)).first->second);
}

// Keep the original definitions for registry edits; serializing resolved Toolchains would discard inheritance.
static bool readRegistry(const path &filePath, string &content, rapidjson::Document &document)
{
    document.SetObject();
    if (filePath.empty())
    {
        return false;
    }
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(filePath, error);
    if (error == std::errc::no_such_file_or_directory)
    {
        return false;
    }
    if (error)
    {
        printErrorMessage(FORMAT("Could not inspect a toolchain registry.\nPath: {}\nSystem error: {}",
                                 filePath.string(), error.message()));
    }
    if (status.type() == std::filesystem::file_type::not_found)
    {
        return false;
    }
    if (status.type() != std::filesystem::file_type::regular)
    {
        printErrorMessage(FORMAT("A toolchain-registry path is not a regular file.\nPath: {}", filePath.string()));
    }

    content = fileToString(filePath.string());
    document.ParseInsitu(content.data());
    if (document.HasParseError())
    {
        printErrorMessage(FORMAT("Could not parse the toolchain registry.\nFile: {}\nParser error: {}\nByte offset: {}",
                                 filePath.string(), rapidjson::GetParseError_En(document.GetParseError()),
                                 document.GetErrorOffset()));
    }
    if (!document.IsObject())
    {
        toolchainError(filePath, {}, "The top level must be an object keyed by toolchain name.");
    }
    return true;
}

void Toolchains::loadFile(const path &filePath)
{
    string content;
    rapidjson::Document document;
    if (!readRegistry(filePath, content, document))
    {
        return;
    }

    for (auto member = document.MemberBegin(); member != document.MemberEnd(); ++member)
    {
        const string name(member->name.GetString(), member->name.GetStringLength());
        if (name.empty() || name.find('\0') != string::npos)
        {
            toolchainError(filePath, {}, "Toolchain names must not be empty or contain null bytes.");
        }
        if (!member->value.IsObject())
        {
            toolchainError(filePath, name, "A toolchain definition must be an object.");
        }
        if (entries.contains(name))
        {
            toolchainError(filePath, name, "Duplicate toolchain name.");
        }

        uint16_t fields = 0;
        string baseName;
        bool hasBase = false;
        for (auto field = member->value.MemberBegin(); field != member->value.MemberEnd(); ++field)
        {
            const string_view fieldName(field->name.GetString(), field->name.GetStringLength());
            const uint16_t fieldBit = toolchainFieldBit(fieldName);
            if (fieldBit == 0)
            {
                toolchainError(filePath, name, FORMAT("Unknown field '{}'.", fieldName));
            }
            if ((fields & fieldBit) != 0)
            {
                toolchainError(filePath, name, FORMAT("Duplicate field '{}'.", fieldName));
            }
            fields |= fieldBit;
            if (fieldName == extendsField)
            {
                baseName = readRequiredString(field->value, filePath, name, fieldName);
                hasBase = true;
            }
        }

        Toolchain toolchain;
        if (hasBase)
        {
            const auto base = entries.find(baseName);
            if (base == entries.end())
            {
                toolchainError(filePath, name,
                               FORMAT("Base toolchain '{}' must be built-in or declared before this entry.", baseName));
            }
            toolchain = base->second;
        }
        else
        {
            errorOnMissingField((fields & toolchainFieldBit(compilerField)) == 0, filePath, name, compilerField);
            errorOnMissingField((fields & toolchainFieldBit(linkerField)) == 0, filePath, name, linkerField);
            errorOnMissingField((fields & toolchainFieldBit(archiverField)) == 0, filePath, name, archiverField);
            errorOnMissingField((fields & toolchainFieldBit(familyField)) == 0, filePath, name, familyField);
            errorOnMissingField((fields & toolchainFieldBit(styleField)) == 0, filePath, name, styleField);
            errorOnMissingField((fields & toolchainFieldBit(versionField)) == 0, filePath, name, versionField);
            errorOnMissingField((fields & toolchainFieldBit(targetField)) == 0, filePath, name, targetField);
            errorOnMissingField((fields & toolchainFieldBit(includeDirsField)) == 0, filePath, name, includeDirsField);
            errorOnMissingField((fields & toolchainFieldBit(libraryDirsField)) == 0, filePath, name, libraryDirsField);
            errorOnMissingField((fields & toolchainFieldBit(bootstrapArgumentsField)) == 0, filePath, name,
                                bootstrapArgumentsField);
        }

        toolchain.name = name;
        for (auto field = member->value.MemberBegin(); field != member->value.MemberEnd(); ++field)
        {
            const string_view fieldName(field->name.GetString(), field->name.GetStringLength());
            if (fieldName == extendsField)
            {
                continue;
            }
            if (fieldName == compilerField)
            {
                toolchain.compiler.bTPath = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == linkerField)
            {
                toolchain.linker.bTPath = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == archiverField)
            {
                toolchain.archiver.bTPath = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == familyField)
            {
                toolchain.family = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == styleField)
            {
                toolchain.style = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == versionField)
            {
                toolchain.version = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == targetField)
            {
                toolchain.target = readRequiredString(field->value, filePath, name, fieldName);
            }
            else if (fieldName == includeDirsField)
            {
                toolchain.includeDirs = readStringArray(field->value, filePath, name, fieldName);
            }
            else if (fieldName == libraryDirsField)
            {
                toolchain.libraryDirs = readStringArray(field->value, filePath, name, fieldName);
            }
            else if (fieldName == bootstrapArgumentsField)
            {
                toolchain.bootstrapArguments = readStringArray(field->value, filePath, name, fieldName);
            }
        }

        if (toolchain.compiler.bTPath.empty() || toolchain.linker.bTPath.empty() || toolchain.archiver.bTPath.empty() ||
            toolchain.family.empty() || toolchain.style.empty() || toolchain.version.empty() ||
            toolchain.target.empty())
        {
            toolchainError(filePath, name, "Required string fields must not be empty after inheritance.");
        }
        initializeBuildTools(toolchain, filePath);
        registryOrder.emplace_back(&entries.emplace(name, std::move(toolchain)).first->second);
    }
}

void Toolchains::initialize(const path &sourceDirectory)
{
    loadFile(userToolchainsFilePath);
    if (!sourceDirectory.empty())
    {
        projectToolchainsFilePath = sourceDirectory / "toolchains.json";
        loadFile(projectToolchainsFilePath);
    }
}

string Toolchains::toJson() const
{
    rapidjson::Document document(rapidjson::kObjectType);
    auto &allocator = document.GetAllocator();
    for (const Toolchain *const toolchain : registryOrder)
    {
        rapidjson::Value jsonToolchain = toolchainToJson(*toolchain, allocator);

        rapidjson::Value jsonName(toolchain->name.data(), static_cast<rapidjson::SizeType>(toolchain->name.size()),
                                  allocator);
        document.AddMember(jsonName, jsonToolchain, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 4);
    document.Accept(writer);
    return {buffer.GetString(), buffer.GetSize()};
}

void Toolchains::remove(const string_view name)
{
    if (name == registryOrder.front()->name)
    {
        printErrorMessage("Cannot remove the default toolchain: " + string(name));
    }
    if (!entries.contains(name))
    {
        printErrorMessage("Unknown toolchain: " + string(name));
    }
    const path registries[] = {userToolchainsFilePath, projectToolchainsFilePath};
    string contents[2];
    rapidjson::Document documents[2];
    const rapidjson::Value jsonName(rapidjson::StringRef(name.data(), static_cast<rapidjson::SizeType>(name.size())));
    uint32_t registryIndex = 2;
    for (uint32_t index = 0; index < 2; ++index)
    {
        readRegistry(registries[index], contents[index], documents[index]);
        if (documents[index].HasMember(jsonName))
        {
            if (registryIndex != 2)
            {
                toolchainError(registries[index], name, "Duplicate toolchain name.");
            }
            registryIndex = index;
        }
    }
    if (registryIndex == 2)
    {
        printErrorMessage("Toolchain is no longer defined in either registry: " + string(name));
    }
    const path &registry = registries[registryIndex];
    FileLock lock(registry.string() + ".lock");

    // Re-read after locking so an intervening edit is never overwritten with the earlier snapshot.
    rapidjson::Document &document = documents[registryIndex];
    readRegistry(registry, contents[registryIndex], document);
    const auto selected = document.FindMember(jsonName);
    if (selected == document.MemberEnd())
    {
        printErrorMessage(FORMAT("Toolchain '{}' is not defined in registry: {}", name, registry.string()));
    }
    for (uint32_t index = 0; index < 2; ++index)
    {
        for (auto member = documents[index].MemberBegin(); member != documents[index].MemberEnd(); ++member)
        {
            if (index == registryIndex && member == selected)
            {
                continue;
            }
            const string_view memberName(member->name.GetString(), member->name.GetStringLength());
            if (memberName == name)
            {
                toolchainError(registries[index], name, "Duplicate toolchain name.");
            }
            if (!member->value.IsObject())
            {
                toolchainError(registries[index], memberName, "A toolchain definition must be an object.");
            }
            for (const auto &field : member->value.GetObject())
            {
                if (string_view(field.name.GetString(), field.name.GetStringLength()) == extendsField &&
                    readRequiredString(field.value, registries[index], memberName, extendsField) == name)
                {
                    printErrorMessage(FORMAT("Cannot remove toolchain '{}': '{}' extends it.\nRegistry: {}", name,
                                             memberName, registries[index].string()));
                }
            }
        }
    }
    // EraseMember preserves declaration order, required when later definitions extend earlier ones.
    document.EraseMember(selected);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 4);
    document.Accept(writer);
    writeCacheFile(registry.string(), {buffer.GetString(), buffer.GetSize()});
    printMessage(FORMAT("Removed toolchain '{}'.\nRegistry: {}\n", name, registry.string()));
}

void Toolchains::calibrate(const string_view compiler, const string_view name, const bool project)
{
#ifndef __linux__
    printErrorMessage("Compiler calibration currently supports Linux Clang and GCC only.");
#else
    if (name.empty() || name.front() == '#' || name.front() == ' ' || name.front() == '\t' ||
        name.find_first_of("\r\n") != string_view::npos || name.find('\0') != string_view::npos)
    {
        printErrorMessage(
            "The toolchain name must be a nonempty cache.txt value without a leading comment or whitespace.");
    }

    std::error_code error;
    const path currentDirectory = std::filesystem::current_path(error);
    if (error)
    {
        printErrorMessage("Could not determine the calibration directory.\nSystem error: " + error.message());
    }
    const auto absolute = [&](const path &value) {
        return (value.is_absolute() ? value : currentDirectory / value).lexically_normal();
    };
    const auto findExecutable = [&](const string_view executable) -> string {
        const auto resolve = [&](const path &value) -> string {
            const path candidate = absolute(value);
            if (std::filesystem::is_regular_file(candidate, error) && access(candidate.c_str(), X_OK) == 0)
            {
                // Keep the driver basename: resolving a clang++ symlink to clang changes its default link behavior.
                return candidate.string();
            }
            return {};
        };
        if (executable.find('/') != string_view::npos)
        {
            return resolve(path(executable));
        }
        if (const char *searchPath = std::getenv("PATH"))
        {
            for (const string_view directory : split(searchPath, ':'))
            {
                if (string result = resolve(path(directory) / executable); !result.empty())
                {
                    return result;
                }
            }
        }
        return {};
    };
    Toolchain calibrated;
    calibrated.name = name;
    calibrated.style = "gnu";
    calibrated.compiler.bTPath = findExecutable(compiler);
    if (calibrated.compiler.bTPath.empty())
    {
        printErrorMessage("Could not find an executable compiler: " + string(compiler));
    }
    calibrated.linker.bTPath = calibrated.compiler.bTPath;

    const path registry = project ? projectToolchainsFilePath : userToolchainsFilePath;
    if (registry.empty())
    {
        printErrorMessage(project ? "Could not locate the project toolchain registry."
                                  : "Could not locate the user toolchain registry.");
    }
    std::filesystem::create_directories(registry.parent_path(), error);
    if (error)
    {
        printErrorMessage("Could not create the toolchain registry directory.\nSystem error: " + error.message());
    }
    FileLock lock(registry.string() + ".lock");
    string registryContents;
    rapidjson::Document document;
    readRegistry(registry, registryContents, document);
    const rapidjson::Value nameView(rapidjson::StringRef(name.data(), static_cast<rapidjson::SizeType>(name.size())));
    if (entries.contains(name) || document.HasMember(nameView))
    {
        printErrorMessage("Toolchain already exists; calibration never replaces a named entry: " + string(name));
    }
    const auto quote = [](const string_view value) {
        string result = "'";
        for (const char character : value)
        {
            if (character == '\'')
            {
                result += "'\\''";
            }
            else
            {
                result += character;
            }
        }
        result += '\'';
        return result;
    };
    const auto trim = [](const string_view value) {
        const uint64_t begin = value.find_first_not_of(" \t\r\n");
        return begin == string_view::npos ? string_view{}
                                          : value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
    };
    string temporaryDirectory;
    const auto fail = [&](const string &diagnostic) {
        if (!temporaryDirectory.empty())
        {
            std::error_code cleanupError;
            std::filesystem::remove_all(temporaryDirectory, cleanupError);
        }
        printErrorMessage(diagnostic);
    };
    const string env = findExecutable("env");
    if (env.empty())
    {
        fail("Could not find env for isolated compiler calibration.");
    }
    // Probe and validate the same default profile without accidentally capturing environment-injected search paths.
    const string environment = quote(env) + " -u CPATH -u CPLUS_INCLUDE_PATH -u C_INCLUDE_PATH -u OBJC_INCLUDE_PATH"
                                            " -u LIBRARY_PATH -u COMPILER_PATH -u GCC_EXEC_PREFIX LC_ALL=C ";
    const auto run = [&](const string &command) {
        RunCommand::OutputAndStatus result = RunCommand::runProcess(environment + command);
        if (result.exitStatus != EXIT_SUCCESS)
        {
            fail(FORMAT("Compiler calibration failed.\nCommand: {}\nExit code: {}\nOutput:\n{}", command,
                        result.exitStatus, result.output));
        }
        return std::move(result.output);
    };
    const string driver = quote(calibrated.compiler.bTPath);
    printMessage("Calibrating " + calibrated.compiler.bTPath + "\n");
    const string macros = run(driver + " -dM -E -x c++ /dev/null");
    const auto macro = [&](const string_view key) {
        const string prefix = "#define " + string(key) + ' ';
        const uint64_t begin = macros.find(prefix);
        if (begin == string::npos)
        {
            fail("Compiler calibration could not read the predefined macro " + string(key));
        }
        const uint64_t valueBegin = begin + prefix.size();
        return string(trim(string_view(macros).substr(valueBegin, macros.find('\n', valueBegin) - valueBegin)));
    };
    if (macros.find("#define __clang__ ") != string::npos)
    {
        calibrated.family = "clang";
        calibrated.version =
            macro("__clang_major__") + '.' + macro("__clang_minor__") + '.' + macro("__clang_patchlevel__");
    }
    else if (macros.find("#define __GNUC__ ") != string::npos)
    {
        calibrated.family = "gcc";
        calibrated.version = macro("__GNUC__") + '.' + macro("__GNUC_MINOR__") + '.' + macro("__GNUC_PATCHLEVEL__");
    }
    else
    {
        fail("Compiler calibration requires a Clang or GCC C++ driver.");
    }
    calibrated.target = trim(run(driver + " -dumpmachine"));

    const auto addDirectory = [&](std::vector<string> &directories, const string_view value) {
        if (value.empty())
        {
            return;
        }
        const path directory = absolute(path(value));
        if (std::filesystem::is_directory(directory, error))
        {
            string normalized = directory.string();
            if (normalized == "/")
            {
                fail("A calibrated include/library directory cannot be the filesystem root.");
            }
            if (normalized.back() == '/')
            {
                normalized.pop_back();
            }
            if (std::ranges::find(directories, normalized) == directories.end())
            {
                directories.emplace_back(std::move(normalized));
            }
        }
    };
    const string includes = run(driver + " -E -x c++ -v /dev/null -o /dev/null");
    constexpr string_view includeStart = "#include <...> search starts here:";
    const uint64_t begin = includes.find(includeStart);
    const uint64_t end = includes.find("End of search list.", begin);
    if (begin == string::npos || end == string::npos)
    {
        fail("Could not read the compiler's include search list.\nOutput:\n" + includes);
    }
    for (const string_view line :
         split(string_view(includes).substr(begin + includeStart.size(), end - begin - includeStart.size()), '\n'))
    {
        addDirectory(calibrated.includeDirs, trim(line));
    }
    if (calibrated.includeDirs.empty())
    {
        fail("The compiler reported no usable standard include directories.");
    }
    const string searches = run(driver + " -print-search-dirs");
    const uint64_t librariesBegin = searches.find("libraries: =");
    if (librariesBegin == string::npos)
    {
        fail("Could not read the compiler's library search list.\nOutput:\n" + searches);
    }
    const uint64_t valuesBegin = librariesBegin + string_view("libraries: =").size();
    for (const string_view directory :
         split(trim(string_view(searches).substr(valuesBegin, searches.find('\n', valuesBegin) - valuesBegin)), ':'))
    {
        addDirectory(calibrated.libraryDirs, directory);
    }
    const string archiverName = calibrated.family == "clang" ? "llvm-ar" : "ar";
    if (calibrated.family == "clang")
    {
        calibrated.archiver.bTPath =
            findExecutable((path(calibrated.compiler.bTPath).parent_path() / archiverName).string());
    }
    if (calibrated.archiver.bTPath.empty())
    {
        calibrated.archiver.bTPath = findExecutable(trim(run(driver + " -print-prog-name=" + archiverName)));
    }
    if (calibrated.archiver.bTPath.empty() && calibrated.family == "clang")
    {
        calibrated.archiver.bTPath = findExecutable(trim(run(driver + " -print-prog-name=ar")));
    }
    if (calibrated.archiver.bTPath.empty())
    {
        fail("Could not find the compiler's archiver.");
    }
    initializeBuildTools(calibrated, registry);

    const path temporaryRoot = std::filesystem::temp_directory_path(error);
    if (error)
    {
        fail("Could not locate the calibration temporary directory.\nSystem error: " + error.message());
    }
    string directoryTemplate = (temporaryRoot / "hmake-calibrate-XXXXXX").string();
    if (mkdtemp(directoryTemplate.data()) == nullptr)
    {
        fail("Could not create the calibration temporary directory.\nSystem error: " + string(std::strerror(errno)));
    }
    temporaryDirectory = directoryTemplate;
    const string source = temporaryDirectory + "/probe.cpp";
    const string object = temporaryDirectory + "/probe.o";
    const string archive = temporaryDirectory + "/probe.a";
    const string executable = temporaryDirectory + "/probe";
    {
        std::ofstream output(source, std::ios::binary);
        output
            << "#include <stddef.h>\n#include <stdint.h>\n#include <iostream>\n#include <string>\n#include <vector>\n"
               "int main(int argc, char**) { std::vector<std::string> words(argc, \"HMake\"); "
               "std::cout << words.front() << sizeof(uint64_t); return 0; }\n";
        output.close();
        if (!output)
        {
            fail("Could not write the compiler calibration source: " + source);
        }
    }
    string command = driver + " -nostdinc -nostdinc++";
    for (const string &directory : calibrated.includeDirs)
    {
        command += " -isystem " + quote(directory);
    }
    run(command + " -c " + quote(source) + " -o " + quote(object));
    run(quote(calibrated.archiver.bTPath) + " rcs " + quote(archive) + ' ' + quote(object));
    command = driver;
    for (const string &directory : calibrated.libraryDirs)
    {
        command += " -L " + quote(directory);
    }
    run(command + ' ' + quote(archive) + " -o " + quote(executable));
    if (!std::filesystem::is_regular_file(executable, error))
    {
        fail("The compiler reported success without producing the calibration executable: " + executable);
    }
    // No target executable is run. Cleanup precedes registry writing, whose fatal errors do not unwind C++ objects.
    std::filesystem::remove_all(temporaryDirectory, error);
    if (error)
    {
        fail("Could not remove the calibration temporary directory.\nSystem error: " + error.message());
    }
    temporaryDirectory.clear();

    auto &allocator = document.GetAllocator();
    rapidjson::Value jsonName(name.data(), static_cast<rapidjson::SizeType>(name.size()), allocator);
    rapidjson::Value entry = toolchainToJson(calibrated, allocator);
    document.AddMember(jsonName, entry, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 4);
    document.Accept(writer);
    writeCacheFile(registry.string(), {buffer.GetString(), buffer.GetSize()});
    printMessage(FORMAT("Calibrated {}: {} {}, target {}\nRegistry: {}\n", name, calibrated.family, calibrated.version,
                        calibrated.target, registry.string()));
#endif
}
