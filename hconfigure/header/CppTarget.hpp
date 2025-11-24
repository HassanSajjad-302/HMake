
/// \file
/// Defines CppTarget class

#ifndef HMAKE_CPPTARGET_HPP
#define HMAKE_CPPTARGET_HPP

#include "BuildTools.hpp"
#include "Configuration.hpp"
#include "CppMod.hpp"
#include "DSC.hpp"
#include "HashedCommand.hpp"
#include "ObjectFileProducer.hpp"
#include <concepts>

using std::same_as;

struct HeaderFileOrUnit
{
    union {
        CppMod *cppMod;
        Node *node;
    } data;
    bool isUnit;
    bool isSystem;
    HeaderFileOrUnit(CppMod *cppMod_, bool isSystem_);
    HeaderFileOrUnit(Node *node_, bool isSystem_);
    HeaderFileOrUnit() = default;
};

/// Whether a file is Module, Header-File or Header-Unit
enum class FileType : uint8_t
{
    MODULE,
    HEADER_FILE,
    HEADER_UNIT,
};

/// This class is responsible for managing c++ compilation. This class compiles multiple source-files, module-files,
/// interface-module-files or header-units. The compile-command is same for all the files in one CppTarget.
/// The API is designed so that user can compile their c++ code with or without header-units. These are classified as
/// two separate modes specified by Configuration::isCppMod. To compile modules or header-units, IsCppMod::YES should be
/// explicitly set for parent Configuration.
/// To compile modules, build-system need to know the following info.
/// 1) All module-files. These files can consume header-units or module-interface-files.
/// 2) All interface-module-files. These files export module. Build-system needs to know export-name.
/// 3) All header-files. Could be public, private, interface. Build-system also needs to know the include-name.
/// 4) All header-units. Could be public, private, interface. Build-system also needs to know the include-name.
/// Build-system have a variety of functions to specify this. These functions are needed to support both header-unit and
/// header-files configurations with one switch change. Some functions have same code for both modes. Some have
/// different code for both modes. Some do stuff in one mode and do nothing in the other. While some will error-out in
/// one mode and do stuff in the other. This last category is generally not meant to be directly used.
///
/// In IsCppMod::YES, Compiler build modules and header-units using IPC. No include-directory is passed to the compiler.
/// Compiler sends the logical-name to the build-system. For module and hu, module and hu are sent respectively. But for
/// header-file, build-system can send hu instead. This depends on how the received logical-name is mapped in target and
/// its deps. So any file that can be a header-file or header-unit need to be specified with its logical-name.
///
/// Functions to specify header-files to header-units use the file-name as the logical-name. However, some functions to
/// specify header-files and header-units in IsCppMod::YES take a parameter prefix. This is added to the fileName to
/// specify the logical-name for the header-files and header-units. This is to support case where there is one umbrella
/// include with every target include-dir inside it. e.g. all libraries in boost include header-files like
/// "boost/lib-name/header-name". To support this in both modes we use following 2 function calls. First does nothing in
/// IsCppMod::YES while second does nothing in IsCppMod::NO.
/// \code
/// publicIncludesSource("boost");
/// privateHuDirs("boost/lib-name", "boost/lib-name/"); // for every target
/// \endcode
/// The first function works only in IsCppMod::NO. It adds the "boost" public-include-dir. While the second function
/// works only in IsCppMod::YES. This adds the header-units with "boost/lib-name/header-name" as the logical-name. The
/// functions that do not take the prefix add the fileName as the logical-name for header-files and header-units.
///
///
class CppTarget : public ObjectFileProducerWithDS<CppTarget>, public TargetCache
{
  public:
    /// BuildCache of this CppTarget. At config-time, entry is created for source-files, module-files and header-units.
    /// Once an entry is created, it is persisted even if it is unused or un-configured so that if it is reconfigured
    /// later, it won't be rebuilt. At build-time, CppSrc updates this using CppSrc::myBuildCacheIndex. This index is
    /// the index in array for srcFileDeps and modFileDeps. CppTarget::adjustBuildCache ensures that order is same in
    /// build-cache and config-cache for srcFileDeps and modFileDeps. It does not do this for imodFileDeps and huDeps as
    /// these might be references in their dependents build. For these, BuildCache::Cpp::imodFiles and
    /// BuildCache::Cpp::headerUnits is searched and the index is stored in config-cache and then later used at
    /// build-time.
    BuildCache::Cpp cppBuildCache;

    flat_hash_set<Define> reqCompileDefinitions;
    flat_hash_set<Define> useReqCompileDefinitions;

    /// Maps module names to their corresponding exporting interface modules in CppTarget::imodFileDeps
    flat_hash_map<string, CppMod *> imodNames;

    /// Compile Command excluding source-file and flags that are always provided (like -o). Hash of this is stored with
    /// the corresponding source-file, module-file or header-unit.
    string compileCommand;

    string reqCompilerFlags;
    string useReqCompilerFlags;

    /// hash of the compile-command
    HashedCommand hashedCompileCommand;

    /// TargetCache::cacheIndex of our direct and transitive dependency CppTargets. It is cached in config-cache.
    vector<uint32_t> reqDepsVecIndices;

    /// source-files. initialized from config-cache at build-time
    vector<CppSrc *> srcFileDeps;

    /// module-files. initialized from config-cache at build-time
    vector<CppMod *> modFileDeps;
    /// interface-module-files. initialized from config-cache at build-time
    vector<CppMod *> imodFileDeps;
    /// header-units. initialized from config-cache at build-time
    vector<CppMod *> huDeps;

    vector<InclNode> reqIncls;
    vector<InclNode> useReqIncls;

    /// Stores the req header-names mapping with header-file or header-unit. Cached at config-time. It is the lookup
    /// table that is used to retrieve the header-file or header-unit when the compiler sends the header-name
    flat_hash_map<string_view, HeaderFileOrUnit> reqHeaderNameMapping;
    /// Stores the useReq header-names mapping with header-file or header-unit. Cached at config-time. It is the lookup
    /// table that is used to retrieve the header-file or header-unit when the compiler sends the header-name
    flat_hash_map<string_view, HeaderFileOrUnit> useReqHeaderNameMapping;

    /// Used only at configure-time. Used to check if any of header-files has changed to header-unit or vice versa. Also
    /// checks if same node is specified as 2 different FileTypes.
    flat_hash_map<const Node *, FileType> reqNodesType;
    /// Used only at configure-time. Checks if same node is specified as 2 different FileTypes in this CppTarget or its
    /// dependencies.
    flat_hash_map<const Node *, FileType> useReqNodesType;

    /// Back pointer to the Configuration
    Configuration *configuration = nullptr;

    /// Where our obj and BMI files will go
    Node *myBuildDir = nullptr;

    /// Used only at configure-time. if (CppTarget::configuration::bigHeader == BigHeaderUnit::YES), then any newly
    /// added public header-units will become a composing header of last element of the following. If the last element
    /// of the following is nullptr, then a new hu is created in CppTarget::myBuildDir of name
    /// [publicBigHus.size()]public[cacheIndex].hpp. CppTarget constructor at config-time initializes this with one
    /// nullptr element.
    vector<CppMod *> publicBigHus;
    /// Used only at configure-time. if (CppTarget::configuration::bigHeader == BigHeaderUnit::YES), then any newly
    /// added private header-units will become a composing header of last element of the following. If the last element
    /// of the following is nullptr, then a new hu is created in CppTarget::myBuildDir of name
    /// [privateBigHus.size()]public[cacheIndex].hpp. CppTarget constructor at config-time initializes this with one
    /// nullptr element.
    vector<CppMod *> privateBigHus;
    /// Used only at configure-time. if (CppTarget::configuration::bigHeader == BigHeaderUnit::YES), then any newly
    /// added interface header-units will become a composing header of last element of the following. If the last
    /// element of the following is nullptr, then a new hu is created in CppTarget::myBuildDir of name
    /// [publicBigHus.size()]public[cacheIndex].hpp. CppTarget constructor at config-time initializes this with one
    /// nullptr element.
    vector<CppMod *> interfaceBigHus;

    /// Used only at configure time. Specifies the number of header-files in reqHeaderNameMapping. Only for
    /// header-files, the mapping is stored in config-cache.
    uint32_t reqHeaderFilesSize = 0;
    /// Used only at configure time. Specifies the number of header-files in useReqHeaderNameMapping. Only for
    /// header-files, the mapping is stored in config-cache.
    uint32_t useReqHeaderFilesSize = 0;

    /// Used only at build-time. include-dirs are stored first in config-cache and are read in constructor while other
    /// config-cache is read later in CppTarget::updateBTarget. include-dirs are read early as our CppTarget dependents
    /// will rely on them.
    uint32_t configRead = 0;

    /// Whether this is a system target. if true, header-files, header-units and include-dirs are all system. Compilers
    /// generally ignore warnings from such code.
    bool isSystem = false;
    /// Whether to not save header-files target in build-cache. Could be set to true if source-files and header-files of
    /// a target are not meant to be edited. This can improve zero-target build-speed.
    bool ignoreHeaderDeps = false;

    /// Whenever a source-file or module-file or header-unit compiles, it sets this variable. If set,
    /// CppTarget::writeBuildCache will call CppSrc::updateBuildCache for all source-files, module-files and
    /// header-units. These will update the corresponding entry in CppTarget::cppBuildCache if they were updated. This
    /// is accessed atomically as it might have been called in Ctrl+C signal handler.
    bool buildCacheUpdated = false;

    /// Sets the compile-command using the Configuration::compilerFlags and Configuration::compilerFeatures.
    void setCompileCommand();

    /// Used in error diagnostics.
    /// \returns an amalgamated string of names of all CppTarget deps of this (direct + transitive).
    string getDependenciesString() const;
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    /// Called in signal-handler or at the end when build-system is writing build-cache.
    bool writeBuildCache(vector<char> &buffer) override;
    /// Goes over the provided \p modCache header-files and header-units and checks if one of them has become
    /// header-unit or header-file respectively. if yes, sets BuildCache::Cpp::ModuleFile::headerStatusChanged to true.
    /// This will cause the rebuild of the respective module-file or header-unit and headerStatusChanged will be set
    /// false to avoid further rebuilds.
    void setHeaderStatusChanged(BuildCache::Cpp::ModuleFile &modCache);
    /// Goes over the arrays of CppTarget::publicBigHus, CppTarget::privateBigHus and CppTarget::interfaceBigHus and
    /// writes them in myBuildDir. The content of these header-units is the header-includes for all the composing
    /// headers of these header-units
    void writeBigHeaderUnits();
    /// Writes build-cache and config-cache at config-time.
    void writeCacheAtConfigTime();
    /// Reads build-cache and config-cache at build-time
    void readCacheAtBuildTime();
    string getPrintName() const override;
    BTargetType getBTargetType() const override;

    CppTarget(const string &name_, Configuration *configuration_);
    CppTarget(bool buildExplicit, const string &name_, Configuration *configuration_);
    CppTarget(Node *myBuildDir_, const string &name_, Configuration *configuration_);
    CppTarget(Node *myBuildDir_, bool buildExplicit, const string &name_, Configuration *configuration_);
    /// internal function. called in constructor.
    void initializeCppTarget(const string &name_, Node *myBuildDir_);

    /// Called by LOAT.
    void getObjectFiles(vector<const ObjectFile *> *objectFiles) const override;

    void populateTransitiveProperties();

    void actuallyAddSourceFileConfigTime(Node *node);
    void actuallyAddModuleFileConfigTime(Node *node, string exportName);
    void emplaceInHeaderNameMapping(string_view headerName, HeaderFileOrUnit type, bool addInReq, bool suppressError);
    void emplaceInNodesType(const Node *node, FileType type, bool addInReq);
    void makeHeaderFileAsUnit(const string &includeName, bool addInReq, bool addInUseReq);
    void removeHeaderFile(const string &includeName, bool addInReq, bool addInUseReq);
    void removeHeaderUnit(const Node *headerNode, const string &includeName, bool addInReq, bool addInUseReq);
    void addHeaderFile(const string &includeName, const Node *headerFile, bool suppressError, bool addInReq,
                       bool addInUseReq);
    void addHeaderUnit(const string &includeName, const Node *headerUnit, bool suppressError, bool addInReq,
                       bool addInUseReq);
    void addHeaderUnitOrFileDir(const Node *includeDir, const string &prefix, bool isHeaderFile, const string &regexStr,
                                bool addInReq, bool addInUseReq);
    void addHeaderUnitOrFileDirMSVC(const Node *includeDir, bool isHeaderFile, bool useMentioned, bool addInReq,
                                    bool addInUseReq, bool isStandard, bool ignoreHeaderDeps);
    void actuallyAddInclude(bool errorOnEmplaceFail, const Node *include, bool addInReq, bool addInUseReq);
    void readModuleMapFromDir(const string &dir);

    template <typename... U> CppTarget &deps(CppTarget *dep, DepType dependency, const U... deps);

    template <typename... U> CppTarget &moduleMaps(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as public header-files. file-name is used as the logical-name.
    /// In IsCppMod::NO, adds the public-include.
    template <typename... U> CppTarget &publicIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as private header-files. file-name is used as the
    /// logical-name. In IsCppMod::NO, adds the private-include.
    template <typename... U> CppTarget &privateIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as interface header-files. file-name is used as the
    /// logical-name. In IsCppMod::NO, adds the interface-include.
    template <typename... U> CppTarget &interfaceIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as public header-units. In IsCppMod::NO, adds the
    /// public-include.
    template <typename... U> CppTarget &publicHUIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as private header-units. file-name is used as the
    /// logical-name. In IsCppMod::NO, adds the private-include.
    template <typename... U> CppTarget &privateHUIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as interface header-units. file-name is used as the
    /// logical-name. In IsCppMod::NO, adds the interface-include.
    template <typename... U> CppTarget &interfaceHUIncludes(const string &include, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as public header-files.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the public-include.
    template <typename... U>
    CppTarget &publicIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as private header-files.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the private-include.
    template <typename... U>
    CppTarget &privateIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as interface header-files.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the interface-include.
    template <typename... U>
    CppTarget &interfaceIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as public header-units.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the public-include.
    template <typename... U>
    CppTarget &publicHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as private header-units.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the private-include.
    template <typename... U>
    CppTarget &privateHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as interface header-units.
    /// file-name is used as the logical-name. In IsCppMod::NO, adds the interface-include.
    template <typename... U>
    CppTarget &interfaceHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as public header-units. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &publicHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as private header-units. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &privateHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as private header-units. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &interfaceHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as public header-units.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &publicHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                              U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as private header-units.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &privateHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                               U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as interface header-units.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &interfaceHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                 U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as public header-files. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &publicIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as private header-files. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &privateIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory as interface header-files. prefix + file-name is used as
    /// logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &interfaceIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as public header-files.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &publicIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                               U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as private header-files.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &privateIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                U... includeDirectoryString);
    /// In IsCppMod::YES, adds all files of the directory whose file-name matches the regex as interface header-files.
    /// prefix + file-name is used as logical-name. Does nothing in IsCppMod::NO.
    template <typename... U>
    CppTarget &interfaceIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                  U... includeDirectoryString);
    /// In IsCppMod::NO, adds public include-dir. Does nothing in IsCppMod::YES.
    template <typename... U> CppTarget &publicIncludesSource(const string &include, U... includeDirectoryString);
    /// In IsCppMod::NO, adds private include-dir. Does nothing in IsCppMod::YES.
    template <typename... U> CppTarget &privateIncludesSource(const string &include, U... includeDirectoryString);
    /// In IsCppMod::NO, adds interface include-dir. Does nothing in IsCppMod::YES.
    template <typename... U> CppTarget &interfaceIncludesSource(const string &include, U... includeDirectoryString);

    CppTarget &publicCompilerFlags(const string &compilerFlags);
    CppTarget &privateCompilerFlags(const string &compilerFlags);
    CppTarget &interfaceCompilerFlags(const string &compilerFlags);

    template <typename... U>
    CppTarget &publicCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);
    template <typename... U>
    CppTarget &privateCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);
    template <typename... U>
    CppTarget &interfaceCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);

    template <typename... U>
    CppTarget &interfaceFiles(const string &modFile, const string &exportName, U... moduleFileString);
    template <typename... U>
    CppTarget &publicHeaderFiles(const string &includeName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppTarget &privateHeaderFiles(const string &includeName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppTarget &interfaceHeaderFiles(const string &includeName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppTarget &publicHeaderUnits(const string &includeName, const string &headerUnit, U... headerUnitsString);
    template <typename... U>
    CppTarget &privateHeaderUnits(const string &includeName, const string &headerUnit, U... headerUnitsString);
    template <typename... U>
    CppTarget &interfaceHeaderUnits(const string &includeName, const string &headerUnit, U... headerUnitsString);
    void parseRegexSourceDirs(bool assignToCppSrcs, const string &sourceDirectory, string regexStr, bool recursive);
    template <typename... U> CppTarget &sourceFiles(const string &srcFile, U... sourceFileString);
    template <typename... U> CppTarget &sourceDirs(const string &sourceDirectory, U... dirs);
    /// sourceDirs with Regex. Only those files of these directories will be added which match the regex.
    template <typename... U> CppTarget &sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    /// Recursive sourceDirs.
    template <typename... U> CppTarget &rSourceDirs(const string &sourceDirectory, U... dirs);
    /// Recursive sourceDirs with Regex. Only those files of these directories will be added which match the regex.
    template <typename... U> CppTarget &rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    template <typename... U> CppTarget &moduleFiles(const string &modFile, U... moduleFileString);
    template <typename... U> CppTarget &moduleDirs(const string &moduleDirectory, U... dirs);
    /// moduleDirs with Regex. Only those files of these directories will be added which match the regex.
    template <typename... U> CppTarget &moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    /// Recursive moduleDirs.
    template <typename... U> CppTarget &rModuleDirs(const string &moduleDirectory, U... dirs);
    /// Recursive moduleDirs with Regex. Only those files of these directories will be added which match the regex.
    template <typename... U> CppTarget &rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    //
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Property>
    CppTarget &assign(T property, Property... properties);
}; // class Target

bool operator<(const CppTarget &lhs, const CppTarget &rhs);

template <typename... U> CppTarget &CppTarget::moduleMaps(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        readModuleMapFromDir(include);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return moduleMaps(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::publicIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", true, "", true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::privateIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", true, "", true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::interfaceIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", true, "", false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::publicHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", false, "", true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::privateHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", false, "", true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::interfaceHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", false, "", false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(false, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicHUDirs(const string &include, const string &prefix, U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateHUDirs(const string &include, const string &prefix, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceHUDirs(const string &include, const string &prefix, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                     U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                      U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                        U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicIncDirs(const string &include, const string &prefix, U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateIncDirs(const string &include, const string &prefix, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceIncDirs(const string &include, const string &prefix, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                      U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                       U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                         U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            Node *inclNode = Node::getNodeNonNormalized(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::publicIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::privateIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeNonNormalized(include, false);
        actuallyAddInclude(true, inclNode, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::sourceFiles(const string &srcFile, U... sourceFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddSourceFileConfigTime(Node::getNodeNonNormalized(srcFile, true));
    }

    if constexpr (sizeof...(sourceFileString))
    {
        return sourceFiles(sourceFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::moduleFiles(const string &modFile, U... moduleFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::NO))
        {
            return sourceFiles(modFile, moduleFileString...);
        }
        actuallyAddModuleFileConfigTime(Node::getNodeNonNormalized(modFile, true), "");
    }

    if constexpr (sizeof...(moduleFileString))
    {
        return moduleFiles(moduleFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicCompileDefines(const string &cddName, const string &cddValue, U... compileDefines)
{
    reqCompileDefinitions.emplace(cddName, cddValue);
    useReqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return publicCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateCompileDefines(const string &cddName, const string &cddValue, U... compileDefines)
{
    reqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return privateCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceCompileDefines(const string &cddName, const string &cddValue, U... compileDefines)
{
    useReqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return interfaceCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceFiles(const string &modFile, const string &exportName, U... moduleFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddModuleFileConfigTime(Node::getNodeNonNormalized(modFile, true), exportName);
    }

    if constexpr (sizeof...(moduleFileString))
    {
        return interfaceFiles(moduleFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicHeaderFiles(const string &includeName, const string &headerFile, U... headersString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderFile(includeName, Node::getNodeNonNormalized(headerFile, true), false, true, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return publicHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateHeaderFiles(const string &includeName, const string &headerFile, U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderFile(includeName, Node::getNodeNonNormalized(headerFile, true), false, true, false);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return privateHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceHeaderFiles(const string &includeName, const string &headerFile, U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderFile(includeName, Node::getNodeNonNormalized(headerFile, true), false, false, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return interfaceHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::publicHeaderUnits(const string &includeName, const string &headerUnit, U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderUnit(includeName, Node::getNodeNonNormalized(headerUnit, true), false, true, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return publicHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::privateHeaderUnits(const string &includeName, const string &headerUnit, U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderUnit(includeName, Node::getNodeNonNormalized(headerUnit, true), false, true, false);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return privateHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppTarget &CppTarget::interfaceHeaderUnits(const string &includeName, const string &headerUnit, U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {
            addHeaderUnit(includeName, Node::getNodeNonNormalized(headerUnit, true), false, false, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return interfaceHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppTarget &CppTarget::sourceDirs(const string &sourceDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, ".*", false);
    }
    if constexpr (sizeof...(dirs))
    {
        return sourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppTarget &CppTarget::moduleDirs(const string &moduleDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, ".*", false);
    }
    if constexpr (sizeof...(dirs))
    {
        return moduleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppTarget &CppTarget::sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, regex, false);
    }
    if constexpr (sizeof...(dirs))
    {
        return sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppTarget &CppTarget::moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, regex, false);
    }
    if constexpr (sizeof...(dirs))
    {
        return moduleDirsRE(dirs...);
    }
    return *this;
}

template <typename... U> CppTarget &CppTarget::rSourceDirs(const string &sourceDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, ".*", true);
    }
    if constexpr (sizeof...(dirs))
    {
        return rSourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppTarget &CppTarget::rModuleDirs(const string &moduleDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, ".*", true);
    }
    if constexpr (sizeof...(dirs))
    {
        return rModuleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppTarget &CppTarget::rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, regex, true);
    }
    if constexpr (sizeof...(dirs))
    {
        return R_sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppTarget &CppTarget::rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, regex, true);
    }
    if constexpr (sizeof...(dirs))
    {
        return R_moduleDirsRE(dirs...);
    }
    return *this;
}

template <DepType dependency, typename T, typename... Property>
CppTarget &CppTarget::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        if constexpr (dependency == DepType::PRIVATE)
        {
            reqCompilerFlags += property;
        }
        else if constexpr (dependency == DepType::INTERFACE)
        {
            useReqCompilerFlags += property;
        }
        else
        {
            reqCompilerFlags += property;
            useReqCompilerFlags += property;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        if constexpr (dependency == DepType::PRIVATE)
        {
            reqCompileDefinitions.emplace(property);
        }
        else if constexpr (dependency == DepType::INTERFACE)
        {
            useReqCompileDefinitions.emplace(property);
        }
        else
        {
            reqCompileDefinitions.emplace(property);
            useReqCompileDefinitions.emplace(property);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        static_assert(false && "Unknown feature");
    }
    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <> DSC<CppTarget>::DSC(CppTarget *ptr, PLOAT *ploat_, bool defines, string define_);

template <> DSC<CppTarget> &DSC<CppTarget>::save(CppTarget &ptr);
template <> DSC<CppTarget> &DSC<CppTarget>::saveAndReplace(CppTarget &ptr);
template <> DSC<CppTarget> &DSC<CppTarget>::restore();

#endif // HMAKE_CPPTARGET_HPP
