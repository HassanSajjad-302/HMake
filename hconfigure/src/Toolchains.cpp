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

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <utility>

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

void Toolchains::loadFile(const path &filePath)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(filePath, error);
    if (error == std::errc::no_such_file_or_directory)
    {
        return;
    }
    if (error)
    {
        printErrorMessage(FORMAT("Could not inspect a toolchain registry.\nPath: {}\nSystem error: {}",
                                 filePath.string(), error.message()));
    }
    if (status.type() == std::filesystem::file_type::not_found)
    {
        return;
    }
    if (status.type() != std::filesystem::file_type::regular)
    {
        printErrorMessage(FORMAT("A toolchain-registry path is not a regular file.\nPath: {}", filePath.string()));
    }

    string content = fileToString(filePath.string());
    rapidjson::Document document;
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
    loadFile(sourceDirectory / "toolchains.json");
}

string Toolchains::toJson() const
{
    rapidjson::Document document(rapidjson::kObjectType);
    auto &allocator = document.GetAllocator();
    for (const Toolchain *const toolchain : registryOrder)
    {
        rapidjson::Value jsonToolchain(rapidjson::kObjectType);
        addJsonString(jsonToolchain, compilerField, toolchain->compiler.bTPath, allocator);
        addJsonString(jsonToolchain, linkerField, toolchain->linker.bTPath, allocator);
        addJsonString(jsonToolchain, archiverField, toolchain->archiver.bTPath, allocator);
        addJsonString(jsonToolchain, familyField, toolchain->family, allocator);
        addJsonString(jsonToolchain, styleField, toolchain->style, allocator);
        addJsonString(jsonToolchain, versionField, toolchain->version, allocator);
        addJsonString(jsonToolchain, targetField, toolchain->target, allocator);

        rapidjson::Value includeDirectories = makeJsonStringArray(toolchain->includeDirs, allocator);
        rapidjson::Value includeDirectoriesName(includeDirsField.data(),
                                                static_cast<rapidjson::SizeType>(includeDirsField.size()), allocator);
        jsonToolchain.AddMember(includeDirectoriesName, includeDirectories, allocator);

        rapidjson::Value libraryDirectories = makeJsonStringArray(toolchain->libraryDirs, allocator);
        rapidjson::Value libraryDirectoriesName(libraryDirsField.data(),
                                                static_cast<rapidjson::SizeType>(libraryDirsField.size()), allocator);
        jsonToolchain.AddMember(libraryDirectoriesName, libraryDirectories, allocator);

        rapidjson::Value bootstrapArguments = makeJsonStringArray(toolchain->bootstrapArguments, allocator);
        rapidjson::Value bootstrapArgumentsName(bootstrapArgumentsField.data(),
                                                static_cast<rapidjson::SizeType>(bootstrapArgumentsField.size()),
                                                allocator);
        jsonToolchain.AddMember(bootstrapArgumentsName, bootstrapArguments, allocator);

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
