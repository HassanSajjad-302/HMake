#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP

#include "BasicTargets.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Features.hpp"
#include "JConsts.hpp"
#include "LinkOrArchiveTarget.hpp"
#include "SMFile.hpp"
#include "ToolsCache.hpp"
#include "Variant.hpp"
#include <concepts>
#include <set>
#include <string>

using std::set, std::string, std::same_as;

enum class Dependency
{
    PUBLIC,
    PRIVATE,
    INTERFACE
};

class Executable;
struct Library;

struct SourceDirectory
{
    const Node *sourceDirectory;
    string regex;
    SourceDirectory() = default;
    SourceDirectory(string sourceDirectory_, string regex_);
    void populateSourceOrSMFiles(class CppSourceTarget &target, bool sourceFiles);
};
void to_json(Json &j, const SourceDirectory &sourceDirectory);
bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs);

class PLibrary;

inline CppSourceTarget *targetForAndOr;

class CppSourceTarget : public Features, public CTarget, public BTarget
{
  public:
    class LinkOrArchiveTarget *linkOrArchiveTarget = nullptr;
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    string targetFilePath;
    const string *variantFilePath;
    string compilerTransitiveFlags;
    string buildCacheFilesDirPath;

    // Some State Variables
    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;

    set<SourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile> moduleSourceFileDependencies;
    void addNodeInSourceFileDependencies(const string &str);
    void addNodeInModuleSourceFileDependencies(const std::string &str, bool angle);
    void setCpuType();
    bool isCpuTypeG7();

    inline static set<string> variantFilePaths;
    vector<SMFile *> smFiles;

    string &getCompileCommand();
    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    //  Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    //  that it is written to the cache on disk for faster compilation next time
    void setSourceNodeFileStatus(SourceNode &sourceNode, bool angle) const;

    void checkForPreBuiltAndCacheDir() override;
    void checkForPreBuiltAndCacheDir(const Json &cacheJson);
    void populateSetTarjanNodesSourceNodes(Builder &builder) override;
    void parseModuleSourceFiles(Builder &builder) override;
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile &smFile);
    PostCompile Compile(SourceNode &sourceNode);
    /* void populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                        const PathPrint &objectFilesPathPrint);*/

    PostBasic GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError);
    TargetType getTargetType() const;
    void pruneAndSaveBuildCache(bool successful);
    Json getBuildCache(bool successful);

    CppSourceTarget *moduleScope = nullptr;
    CppSourceTarget *configurationScope = nullptr;
    vector<const Node *> publicIncludes;
    // In module scope, two different targets should not have a directory in hu-public-includes
    vector<const Node *> publicHUIncludes;
    string publicCompilerFlags;
    vector<Define> publicCompileDefinitions;
    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;

    void initializeForBuild() override;
    void setJson() override;
    void writeJsonFile() override;
    BTarget *getBTarget() override;
    void checkForHeaderUnitsCache() override;
    void createHeaderUnits() override;
    void setPropertiesFlagsMSVC();
    void setPropertiesFlagsGCC();

  private:
    // returns on first positive condition. space is added.
    template <typename T, typename... Argument>
    string GET_FLAG_EVALUATE(T condition, const string &flags, Argument... arguments) const;

    // returns cumulated string for trued conditions. spaces are added
    template <typename T, typename... Argument>
    string GET_CUMULATED_FLAG_EVALUATE(T condition, const string &flags, Argument... arguments) const;

    // TODO
  public:
    CppSourceTarget() = default;
    CppSourceTarget(string name_, bool initializeFromCache = true);
    CppSourceTarget(string name_, LinkOrArchiveTarget &linkOrArchiveTarget, bool initializeFromCache = true);
    CppSourceTarget(string name_, Variant &variant);

  public:
    void updateBTarget() override;
    void printMutexLockRoutine() override;
    //

    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_HU_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_HU_INCLUDES(U... includeDirectoryString);
    /*    template <same_as<Library *>... U> CppSourceTarget &PUBLIC_LIBRARIES(const U... libraries);
        template <same_as<Library *>... U> CppSourceTarget &PRIVATE_LIBRARIES(const U... libraries);*/
    CppSourceTarget &PUBLIC_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PRIVATE_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PUBLIC_LINKER_FLAGS(const string &linkerFlags);
    CppSourceTarget &PRIVATE_LINKER_FLAGS(const string &linkerFlags);
    CppSourceTarget &PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    CppSourceTarget &PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    template <same_as<char const *>... U> CppSourceTarget &SOURCE_FILES(U... sourceFileString);
    template <same_as<char const *>... U> CppSourceTarget &MODULE_FILES(U... moduleFileString);

    CppSourceTarget &SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex);
    CppSourceTarget &MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex);

    inline CppSourceTarget &setTargetForAndOr()
    {
        targetForAndOr = this;
        return *this;
    }
    // All properties are assigned.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    CppSourceTarget &ASSIGN(T property, Condition... properties);

    // Properties are assigned if assign bool is true. To Be Used With functions ADD and OR.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    CppSourceTarget &ASSIGN(bool assign, T property, Condition... conditions);
#define ASSIGN_I ASSIGN<Dependency::INTERFACE>

    // Evaluates a property. NEGATE is meant to be used where ! would have been used.
    template <typename T> bool EVALUATE(T property) const;
    template <typename T> T NEGATE(T propterty) const;

    // Eight functions are being used to manipulate properties. The two are AND and OR in global namespace.
    // AND and OR functions also have an overload that takes a target pointer and sets the global variable.
    // Remaining six include the last 4 and the 2 ASSIGN functions.

  private:
    // var left = right;
    // Only last property will be assigned. Others aren't considered.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    void RIGHT_MOST(T condition, Condition... conditions);

  public:
    // Multiple properties on left which are anded and after that right's assignment occurs.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    CppSourceTarget &M_LEFT_AND(T condition, Condition... conditions);
#define M_LEFT_AND_I M_LEFT_AND<Dependency::INTERFACE>

    // Single Condition and Property. M_LEFT_AND or M_LEFT_OR could be used too, but, that's more clear
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename P>
    CppSourceTarget &SINGLE(T condition, P property);
#define SINGLE_I SINGLE<Dependency::INTERFACE>

    // Multiple properties on left which are orred and after that right's assignment occurs.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    CppSourceTarget &M_LEFT_OR(T condition, Condition... conditions);
#define M_LEFT_OR_I M_LEFT_OR<Dependency::INTERFACE>

    // Incomplete
    // If first left succeeds then, Multiple properties of the right are assigned to the target.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    CppSourceTarget &M_RIGHT(T condition, Condition... conditions);
#define M_RIGHT_I M_RIGHT<Dependency::INTERFACE>
}; // class Target

template <typename T, typename... Argument>
string CppSourceTarget::GET_FLAG_EVALUATE(T condition, const string &flags, Argument... arguments) const
{
    if (EVALUATE(condition))
    {
        return flags;
    }
    if constexpr (sizeof...(arguments))
    {
        return GET_FLAG_EVALUATE(arguments...);
    }
    return "";
}

template <typename T, typename... Argument>
string CppSourceTarget::GET_CUMULATED_FLAG_EVALUATE(T condition, const std::string &flags, Argument... arguments) const
{
    string str{};
    if (EVALUATE(condition))
    {
        str = flags;
    }
    if constexpr (sizeof...(arguments))
    {
        str += GET_FLAG_EVALUATE(arguments...);
    }
    return str;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_INCLUDES(U... includeDirectoryString)
{
    (publicIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_INCLUDES(U... includeDirectoryString)
{
    (privateIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_HU_INCLUDES(U... includeDirectoryString)
{
    (publicHUIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_HU_INCLUDES(U... includeDirectoryString)
{
    (privateHUIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

/*template <same_as<Library *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_LIBRARIES(const U... libraries)
{
    (publicLibs.emplace(libraries...));
    return *this;
}

template <same_as<Library *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_LIBRARIES(const U... libraries)
{
    (privateLibs.emplace(libraries...));
    return *this;
}*/

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::SOURCE_FILES(U... sourceFileString)
{
    (addNodeInSourceFileDependencies(sourceFileString), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::MODULE_FILES(U... moduleFileString)
{
    (addNodeInModuleSourceFileDependencies(moduleFileString), ...);
    return *this;
}

template <Dependency dependency, typename T, typename... Property>
CppSourceTarget &CppSourceTarget::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        targetOs = property;
        // TODO
        //  Ensure that toolset version supports it.
    }
    else if constexpr (std::is_same_v<decltype(property), ToolSet>)
    {
        toolSet = property;
        // TODO
        //  Here toolset is being assigned. It is ensured that the respective ToolSet exists in the
        //  Target's Variant. And the compiler and linker
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        // Beside assigning to warnings, add the respective flags for respective compilers to the
        // cxxflags.
        warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        privateCompilerFlags += property;
    }
    else if constexpr (std::is_same_v<decltype(property), LinkFlags>)
    {
        privateLinkerFlags += property;
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        privateCompileDefinitions.emplace_back(property);
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        threading = property;
    }
    else
    {
        warnings = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

template <Dependency dependency, typename T, typename... Condition>
CppSourceTarget &CppSourceTarget::ASSIGN(bool assign, T property, Condition... conditions)
{
    return assign ? ASSIGN(property, conditions...) : *this;
}

template <typename T> bool CppSourceTarget::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TS>)
    {
        return toolSet.ts == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        return threading == property;
    }
    else if constexpr (std::is_same_v<decltype(property), enum Link>)
    {
        return link == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        return cxxStd == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        return cxxStdDialect == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        return optimization == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        return inlining == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        return warnings == property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        return warningsAsErrors == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        return exceptionHandling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        return asyncExceptions == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        return rtti == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        return externCNoThrow == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        return debugSymbols == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        return profiling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LocalVisibility>)
    {
        return localVisibility == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        return addressSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        return leakSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        return threadSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        return undefinedSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        return coverage == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        return lto == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        return ltoMode == property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        return stdLib == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        return runtimeLink == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        return runtimeDebugging == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        return arch == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        return addModel == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        if (bTargetType != cTargetType)
        {
            // TODO
        }
        return cTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        return debugStore == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        return userInterface == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        toolSet = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

template <typename T> T CppSourceTarget::NEGATE(T property) const
{
    if (EVALUATE(property))
    {
        return static_cast<T>(static_cast<int>(property) + 1);
    }
    else
    {
        return property;
    }
}

template <Dependency dependency, typename T, typename... Condition>
void CppSourceTarget::RIGHT_MOST(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        RIGHT_MOST(conditions...);
    }
    else
    {
        ASSIGN(condition);
    }
}

template <Dependency dependency, typename T, typename... Condition>
CppSourceTarget &CppSourceTarget::M_LEFT_AND(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        return EVALUATE(condition) ? M_LEFT_AND(conditions...) : *this;
    }
    else
    {
        ASSIGN(condition);
        return *this;
    }
}

template <Dependency dependency, typename T, typename P>
CppSourceTarget &CppSourceTarget::SINGLE(T condition, P property)
{
    if (EVALUATE(condition))
    {
        ASSIGN(property);
    }
    return *this;
}

template <Dependency dependency, typename T, typename... Condition>
CppSourceTarget &CppSourceTarget::M_LEFT_OR(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        if (EVALUATE(condition))
        {
            RIGHT_MOST(conditions...);
            return *this;
        }
        return M_LEFT_OR(conditions...);
    }
    else
    {
        return *this;
    }
}

template <Dependency dependency, typename T, typename... Condition>
CppSourceTarget &CppSourceTarget::M_RIGHT(T condition, Condition... conditions)
{
    if (EVALUATE(condition))
    {
        ASSIGN(conditions...);
    }
    return *this;
}

template <typename T, typename... Condition> bool AND(T condition, Condition... conditions)
{
    if (!targetForAndOr->EVALUATE(condition))
    {
        return false;
    }
    if constexpr (sizeof...(conditions))
    {
        return AND(conditions...);
    }
    else
    {
        return true;
    }
}

template <typename T, typename... Condition> bool OR(T condition, Condition... conditions)
{
    if (targetForAndOr->EVALUATE(condition))
    {
        return true;
    }
    if constexpr (sizeof...(conditions))
    {
        return OR(conditions...);
    }
    else
    {
        return false;
    }
}

template <typename T, typename... Condition> bool AND(CppSourceTarget *target, T condition, Condition... conditions)
{
    targetForAndOr = target;
    if (!targetForAndOr->EVALUATE(condition))
    {
        return false;
    }
    if constexpr (sizeof...(conditions))
    {
        return AND(conditions...);
    }
    else
    {
        return true;
    }
}

template <typename T, typename... Condition> bool OR(CppSourceTarget *target, T condition, Condition... conditions)
{
    targetForAndOr = target;
    if (targetForAndOr->EVALUATE(condition))
    {
        return true;
    }
    if constexpr (sizeof...(conditions))
    {
        return OR(conditions...);
    }
    else
    {
        return false;
    }
}

struct Executable : public CppSourceTarget
{
    explicit Executable(string targetName_, bool initializeFromCache = true);
    Executable(string targetName_, Variant &variant);
};

struct Library : public CppSourceTarget
{
    explicit Library(string targetName_, bool initializeFromCache = true);
    Library(string targetName_, Variant &variant);
};
void to_json(Json &j, const Library *library);

// PreBuilt-Library
class PLibrary
{
    PLibrary() = default;
    friend class PPLibrary;

    TargetType libraryType;

  private:
    vector<PLibrary *> prebuilts;

  public:
    string libraryName;
    set<Node *> includes;
    string compilerFlags;
    string linkerFlags;
    vector<Define> compileDefinitions;
    path libraryPath;
    mutable TargetType targetType;
    PLibrary(Variant &variant, const path &libraryPath_, TargetType libraryType_);
};
bool operator<(const PLibrary &lhs, const PLibrary &rhs);
void to_json(Json &j, const PLibrary *pLibrary);
#endif // HMAKE_CPPSOURCETARGET_HPP
