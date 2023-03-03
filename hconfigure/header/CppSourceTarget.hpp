#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP

#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "JConsts.hpp"
#include "SMFile.hpp"
#include "ToolsCache.hpp"
#include <concepts>
#include <set>
#include <string>

using std::set, std::string, std::same_as;

struct SourceDirectory
{
    const Node *sourceDirectory;
    string regex;
    SourceDirectory() = default;
    SourceDirectory(const string &sourceDirectory_, string regex_);
};
void to_json(Json &j, const SourceDirectory &sourceDirectory);
bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs);

struct CompilerFlags
{
    // GCC
    string OPTIONS;
    string OPTIONS_COMPILE_CPP;
    string OPTIONS_COMPILE;
    string DEFINES_COMPILE_CPP;
    string LANG;

    string TRANSLATE_INCLUDE;
    // MSVC
    string DOT_CC_COMPILE;
    string DOT_ASM_COMPILE;
    string DOT_ASM_OUTPUT_COMPILE;
    string DOT_LD_ARCHIVE;
    string PCH_FILE_COMPILE;
    string PCH_SOURCE_COMPILE;
    string PCH_HEADER_COMPILE;
    string PDB_CFLAG;
    string ASMFLAGS_ASM;
    string CPP_FLAGS_COMPILE_CPP;
    string CPP_FLAGS_COMPILE;
};

struct ModuleScopeData
{
    set<CppSourceTarget *> cppTargets;
    set<SMFile *> smFiles;
    set<SMFile> headerUnits;
    // Which application header unit directory come from which target
    map<const Node *, CppSourceTarget *> appHUDirTarget;
    map<string, SMFile *> requirePaths;
};

class CppSourceTarget : public CommonFeatures,
                        public CompilerFeatures,
                        public CTarget,
                        public FeatureConvenienceFunctions<CppSourceTarget>,
                        public ObjectFileProducerWithDS<CppSourceTarget>
{
  public:
    TargetType compileTargetType;
    CppSourceTarget *moduleScope = nullptr;
    inline static map<const CppSourceTarget *, ModuleScopeData> moduleScopes;
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    string targetFilePath;
    string buildCacheFilesDirPath;

    // Some State Variables
    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;

    set<SourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile> moduleSourceFileDependencies;
    void addNodeInSourceFileDependencies(const string &str);
    void addNodeInModuleSourceFileDependencies(const std::string &str);
    void setCpuType();
    bool isCpuTypeG7();

    set<const SMFile *> applicationHeaderUnits;

    string &getCompileCommand();
    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    void checkForPreBuiltAndCacheDir(Builder &builder);
    void removeUnReferencedHeaderUnits();
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile &smFile);
    string getSHUSPath() const;
    string getExtension();
    PostCompile updateSourceNodeBTarget(SourceNode &sourceNode);

    PostBasic GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void pruneAndSaveBuildCache();

    set<const Node *> usageRequirementIncludes;
    // In module scope, two different targets should not have a directory in huIncludes
    set<const Node *> huIncludes;

    string usageRequirementCompilerFlags;
    set<Define> usageRequirementCompileDefinitions;
    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;

    void initializeForBuild();
    void preSort(Builder &builder, unsigned short round) override;
    void setJson() override;
    void writeJsonFile() override;
    string getTarjanNodeName() override;
    BTarget *getBTarget() override;
    CompilerFlags getCompilerFlags();
    // TODO
  public:
    CppSourceTarget(string name_, TargetType targetType);
    CppSourceTarget(string name_, TargetType targetType, CTarget &other, bool hasFile = true);

    void getObjectFiles(vector<ObjectFile *> *objectFiles, LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void updateBTarget(unsigned short round, Builder &builder) override;
    void addRequirementDepsToBTargetDependencies();
    void populateTransitiveProperties();
    //

    CppSourceTarget &setModuleScope(CppSourceTarget *moduleScope_);
    // TODO
    //  U functions should accept one Argument atleast.
    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &INTERFACE_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_HU_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_HU_INCLUDES(U... includeDirectoryString);
    CppSourceTarget &PUBLIC_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PRIVATE_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    CppSourceTarget &PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    CppSourceTarget &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    template <same_as<char const *>... U> CppSourceTarget &SOURCE_FILES(U... sourceFileString);
    template <same_as<char const *>... U> CppSourceTarget &MODULE_FILES(U... moduleFileString);

    CppSourceTarget &SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex);
    CppSourceTarget &MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex);

    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);
}; // class Target

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_INCLUDES(U... includeDirectoryString)
{
    (requirementIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    (usageRequirementIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_INCLUDES(U... includeDirectoryString)
{
    (requirementIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::INTERFACE_INCLUDES(U... includeDirectoryString)
{
    (usageRequirementIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_HU_INCLUDES(U... includeDirectoryString)
{
    PUBLIC_INCLUDES(includeDirectoryString...);
    (huIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_HU_INCLUDES(U... includeDirectoryString)
{
    PRIVATE_INCLUDES(includeDirectoryString...);
    (huIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::SOURCE_FILES(U... sourceFileString)
{
    (addNodeInSourceFileDependencies(sourceFileString), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::MODULE_FILES(U... moduleFileString)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return SOURCE_FILES(moduleFileString...);
    }
    else
    {
        (addNodeInModuleSourceFileDependencies(moduleFileString), ...);
        return *this;
    }
}

template <Dependency dependency, typename T, typename... Property>
CppSourceTarget &CppSourceTarget::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        compiler = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), enum Link>)
    {
        link = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        cxxStd = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        cxxStdDialect = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        optimization = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        inlining = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        warningsAsErrors = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        exceptionHandling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        asyncExceptions = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        rtti = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        externCNoThrow = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        stdLib = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        compileTargetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        translateInclude = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        treatModuleAsSource = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        assignCommonFeature(property);
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

template <typename T> bool CppSourceTarget::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        return compiler == property;
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
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        return stdLib == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return compileTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    // CommonFeature properties
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
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
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        return runtimeLink == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        return arch == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        return addModel == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        return debugStore == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        return runtimeDebugging == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        return translateInclude == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        return treatModuleAsSource == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        compiler = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

template <Dependency dependency, typename T> void CppSourceTarget::assignCommonFeature(T property)
{
    if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LocalVisibility>)
    {
        localVisibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        runtimeDebugging = property;
    }
    else
    {
        compiler = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

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
    PLibrary(Configuration &variant, const path &libraryPath_, TargetType libraryType_);
};
bool operator<(const PLibrary &lhs, const PLibrary &rhs);
void to_json(Json &j, const PLibrary *pLibrary);
#endif // HMAKE_CPPSOURCETARGET_HPP
