#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP

#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "JConsts.hpp"
#include "LinkOrArchiveTarget.hpp"
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

class CppSourceTarget : public CommonFeatures,
                        public CompilerFeatures,
                        public CTarget,
                        public BTarget,
                        public FeatureConvenienceFunctions<CppSourceTarget>
{
  public:
    TargetType compileTargetType;
    CppSourceTarget *moduleScope = nullptr;
    class LinkOrArchiveTarget *linkOrArchiveTarget = nullptr;
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    string targetFilePath;
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
    void addNodeInModuleSourceFileDependencies(const std::string &str);
    void setCpuType();
    bool isCpuTypeG7();

    set<const SMFile *> applicationHeaderUnits;

    string &getCompileCommand();
    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    void checkForPreBuiltAndCacheDir(Builder &builder);
    void populateSourceNodesAndRemoveUnReferencedHeaderUnits() override;
    void parseModuleSourceFiles(Builder &builder);
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile &smFile);
    string getSHUSPath() const;
    string getExtension();
    PostCompile updateSourceNodeBTarget(SourceNode &sourceNode);
    /* void populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                        const PathPrint &objectFilesPathPrint);*/

    PostBasic GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void pruneAndSaveBuildCache(bool successful);

    set<const Node *> publicIncludes;
    // In module scope, two different targets should not have a directory in hu-public-includes
    set<const Node *> publicHUIncludes;
    string publicCompilerFlags;
    set<Define> publicCompileDefinitions;
    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;

    void initializeForBuild(Builder &builder) override;
    void setJson() override;
    void writeJsonFile() override;
    BTarget *getBTarget() override;
    CompilerFlags getCompilerFlags();
    // TODO
  public:
    CppSourceTarget(string name_, TargetType targetType);
    CppSourceTarget(string name_, TargetType targetType, LinkOrArchiveTarget &linkOrArchiveTarget_);
    CppSourceTarget(string name_, TargetType targetType, CTarget &other, bool hasFile = true);
    CppSourceTarget(string name_, TargetType targetType, LinkOrArchiveTarget &linkOrArchiveTarget_, CTarget &other,
                    bool hasFile = true);

  public:
    void updateBTarget(unsigned short round, Builder &builder) override;
    //

    CppSourceTarget &setModuleScope(CppSourceTarget *moduleScope_);
    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PUBLIC_HU_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> CppSourceTarget &PRIVATE_HU_INCLUDES(U... includeDirectoryString);
    template <same_as<LinkOrArchiveTarget *>... U> CppSourceTarget &PUBLIC_LIBRARIES(const U... libraries);
    template <same_as<LinkOrArchiveTarget *>... U> CppSourceTarget &PRIVATE_LIBRARIES(const U... libraries);
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

    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);

}; // class Target

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_INCLUDES(U... includeDirectoryString)
{
    (publicIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_INCLUDES(U... includeDirectoryString)
{
    (privateIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
    return *this;
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_HU_INCLUDES(U... includeDirectoryString)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return PUBLIC_INCLUDES(includeDirectoryString...);
    }
    else
    {
        (publicHUIncludes.emplace(Node::getNodeFromString(includeDirectoryString, false)), ...);
        return *this;
    }
}

template <same_as<char const *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_HU_INCLUDES(U... includeDirectoryString)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return PRIVATE_INCLUDES(includeDirectoryString...);
    }
    else
    {
        (privateHUIncludes.emplace_back(Node::getNodeFromString(includeDirectoryString, false)), ...);
        return *this;
    }
}

template <same_as<LinkOrArchiveTarget *>... U> CppSourceTarget &CppSourceTarget::PUBLIC_LIBRARIES(const U... libraries)
{
    if (linkOrArchiveTarget)
    {
        (linkOrArchiveTarget->publicLibs.emplace(libraries...));
    }
    return *this;
}

template <same_as<LinkOrArchiveTarget *>... U> CppSourceTarget &CppSourceTarget::PRIVATE_LIBRARIES(const U... libraries)
{
    if (linkOrArchiveTarget)
    {
        (linkOrArchiveTarget->privateLibs.emplace(libraries...));
    }
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
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        linkOrArchiveTarget->ltoMode = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        stdLib = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        compileTargetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        linkOrArchiveTarget->userInterface = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        linkOrArchiveTarget->strip = property;
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
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        return linkOrArchiveTarget->ltoMode == property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        return stdLib == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return compileTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        return linkOrArchiveTarget->userInterface == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        return linkOrArchiveTarget->strip == property;
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
    if (linkOrArchiveTarget)
    {
        linkOrArchiveTarget->ASSIGN(property);
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
