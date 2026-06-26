/// \file
/// This file defines CppSrc and CppMod

#ifndef HMAKE_CPPMOD_HPP
#define HMAKE_CPPMOD_HPP

#include "IPCManagerBS.hpp"
#include "ObjectFile.hpp"
#include "gtl/include/gtl/btree.hpp"
#include <filesystem>
#include <list>
#include <utility>
#include <vector>

using std::vector, std::filesystem::path, std::pair, std::list, std::shared_ptr, gtl::btree_set, gtl::flat_hash_map;

class CppTarget;
class CppSrc;
struct HeaderFileOrUnit;

struct CompareCppSrc
{
    using is_transparent = void; // for example with void,
    // but could be int or struct CanSearchOnId;
    bool operator()(const CppSrc &lhs, const CppSrc &rhs) const;
    bool operator()(const Node *lhs, const CppSrc &rhs) const;
    bool operator()(const CppSrc &lhs, const Node *rhs) const;
};

/// Language of a `CppSrc` translation unit, inferred from the file extension.
enum class SourceType : uint8_t
{
    C,
    CPP,
    ASSEMBLY,
};

/// Responsible for compiling C++ source-files. Inherits from ObjectFile which has Node pinter ObjectFile::objectNode
class CppSrc : public ObjectFile
{
  public:
    /// header-files discovered during the build. MSVC can output duplicate files. Also, while compiling modules we can
    /// get same header-file from multiple header-unit or module-deps. So, a set is used to remove duplicates to keep
    /// the build-cache small.
    flat_hash_set<Node *> headerFiles;

    /// The back pointer to the CppTarget owning this in srcFileDeps.
    CppTarget *target;

    /// Node pointer to the source-file
    const Node *node;

    /// Hash of the compile command for this file (flags, defines, includes, etc.). Set in `CppTarget::setCommandHashes()`.
    /// Combined with source/header content hashes to form `RealBTarget::cumulativeHash`.
    uint64_t commandHash;

    /// If the file has .c extension, it is a C source-file. If .S or .s, it is an ASSEMBLY file. Otherwise, C++ file
    SourceType sourceType = SourceType::CPP;

    /// Header-file node indices restored from build-cache (`Node::getHalfNode(index)`), used in `setUpdateStatus()`.
    span<const uint32_t> cachedHeaderFiles;

    CppSrc(CppTarget *target_, const Node *node_, CppModType cppModType);
    string getPrintName() const override;
    void getCompileCommand(std::pmr::string &compileCommand) const;
    bool ignoreHeaderFile(string_view child) const;
    /// MSVC prints header-files with the compilation output. This function parses them out from that output.
    void parseDepsFromMSVCTextOutput(string &output, bool isClang);
    /// GCC outputs header-files in a .d file. This function parses that
    void parseHeadersFromGccDepsOutput(Builder &builder);
    /// Calls either of parseDepsFromGCCDepsOutput or parseDepsFromMSVCTextOutput
    void parseHeaderDeps(string &output, Builder &builder);
    /// This compares lastWrite of source-node with object-node and header-files
    void setUpdateStatus() override;

    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view) override;
    void writeConfigCacheAtConfigTime(string &buffer) override;
    void writeBuildCacheAtConfigTime(string &buffer) override;
    void writeBuildCacheAtBuildTime(string &buffer) override;
    void verifyBuildCache(string_view buildCache) const override;
    void verifyConfigCache(string_view configCache) const override;
};

bool operator<(const CppSrc &lhs, const CppSrc &rhs);
class CppMod;

/// Packs a CppMod* with a 1-bit isDirect flag.
/// CppMod contains pointer/container members so alignof(CppMod) >= 8 — 3 bits available; we use only 1.
///   bit 0 : isDirect
///   bits 1-63 : CppMod*
struct CppModWithDirect
{
    PointerIntPair<CppMod *, 1, uint8_t> ptrAndDirect;

    static constexpr uintptr_t kDirectMask = uintptr_t(0x1);

    CppModWithDirect(CppMod *ptr, bool isDirect_) : ptrAndDirect(ptr, static_cast<uint8_t>(isDirect_))
    {
    }

    CppMod *getPointer() const
    {
        return ptrAndDirect.getPointer();
    }

    bool isDirect() const
    {
        return static_cast<bool>(ptrAndDirect.getRaw() & kDirectMask);
    }
};

struct CppModWithDirectHash
{
    using is_transparent = void;

    size_t operator()(const CppModWithDirect &e) const
    {
        // alignof(CppMod) >= 8, so low 3 bits are always zero — shift for better distribution
        return reinterpret_cast<size_t>(e.getPointer()) >> 3;
    }

    size_t operator()(const CppMod *ptr) const
    {
        return reinterpret_cast<size_t>(ptr) >> 3;
    }
};

struct CppModWithDirectEqual
{
    using is_transparent = void;

    bool operator()(const CppModWithDirect &a, const CppModWithDirect &b) const
    {
        return a.getPointer() == b.getPointer();
    }
    bool operator()(const CppModWithDirect &a, const CppMod *ptr) const
    {
        return a.getPointer() == ptr;
    }
    bool operator()(const CppMod *ptr, const CppModWithDirect &a) const
    {
        return a.getPointer() == ptr;
    }
};

class CppMod : public CppSrc
{
  public:
    /// Transitive module/hu dependencies (direct and indirect), populated by `populateAllDeps()`.
    /// The `isDirect` bit marks edges that are persisted: only direct deps are written to the build-cache
    /// (`writeBuildCacheAtBuildTime()`); transitive entries are recomputed from `cachedDeps` at build-time.
    flat_hash_set<CppModWithDirect, CppModWithDirectHash, CppModWithDirectEqual> allCppModDeps;

    /// `cacheIndex` values of direct module/hu dependencies, read from the build-cache at build-time.
    span<const uint32_t> cachedDeps;

    /// Used only if configuration->evaluate(DuplicationWarning::YES). Otherwise, CppSrc::headerFiles is used.
    flat_hash_map<Node *, CppMod *> headerNodeCppMod;

    /// Those header-files which are #included in this module or hu. These are initialized from config-cache as big-hu
    /// have these. While Source::headerFiles have all the header-files of ours and our dependencies for accurate
    /// rebuilds.
    flat_hash_map<string, Node *> composingHeaders;

    /// A header-unit can be found by more than 1 logicalNames. Like "std/header1.hpp" and "./header1.hpp". Also, in big
    /// header-units case a big header-unit can be found by any of its composing headers. All composing includes are
    /// added in following array and is sent with requested hu to keep the number of messages minimal. In case of
    /// module, logicalNames[0] is the exportName of the module.
    vector<string> logicalNames;

    /// Snapshot of `realBTargets[0].launchTime` taken when IPC compilation starts; used to detect header changes that
    /// occurred after launch when writing the cumulative-hash to the build-cache.
    uint64_t originalLaunchTime;

    /// BMI node for header-units and module interface files. Initialized in CppTarget::readConfigCache.
    Node *interfaceNode;

    /// CppMod::updateBTarget will initialize this and then will call receiveMessage to learn about any dependencies the
    /// compiler require.
    P2978::IPCManagerBS *ipcManager;

    /// The dependency module or hu we are waiting on to compile.
    CppMod *waitingFor = nullptr;

    /// Size in bytes of `interfaceNode` after memory-mapping; sent to the compiler in IPC messages.
    uint32_t interfaceFileSize;

    /// Kind of translation unit: source, primary/partition export, header-unit, or primary implementation.
    CppModType type;

    /// Following is used only at config-time. Describes whether hu is private hu of the CppTarget.
    bool isReqHu = false;

    /// Following is used only at config-time. Describes whether hu is interface hu of the CppTarget.
    bool isUseReqHu = false;

    /// Composing headers are only sent with the first IPC message. This keeps tracks of that
    bool firstMessageSent = false;

    /// True after `makeMemoryFileMapping()` has mapped `interfaceNode` and recorded `interfaceFileSize`.
    bool memoryMappingCompleted = false;

    /// With `realBTargets[0].insertionIndex`, allows one bring-to-front per dependency: while it is already in
    /// `updateBTargets` but `isEventRegistered` has not run and it has not yet been moved to the head (`!isScheduled`).
    bool isScheduled = false;

    /// True after `populateAllDeps()` has filled `allCppModDeps` from `cachedDeps` and transitive closure.
    bool isAllDepsPopulated = false;

    CppMod(CppTarget *target_, const Node *node_, CppModType cppModType);

    /// Following ensures that build-system creates shared-memory bmi file before sending it to the compiler. if the
    /// file is updated, then the compiler creates the mapping. if not then the build-system.
    void makeMemoryFileMapping();

    void populateAllDeps();

    /// Called to send the P2978::BTCModule corresponding to a module CppMod whose compilation just completed
    void makeAndSendBTCModule(CppMod &mod);

    /// Called to send the P2978::BTCNonModule corresponding to a hu CppMod whose compilation just completed
    void makeAndSendBTCNonModule(CppMod &hu);

    /// Looks for the received module-name in just CppTarget::imodNames if module-name is of partition export. Looks in
    /// CppTarget::imodNames of dependencies CppTarget as well if it is a primary export.
    CppMod *findModule(string_view moduleName) const;

    /// Looks for the received header-name in CppTarget::reqHeaderNameMapping and Configuration::headerNameMapping
    /// (which has useReqHeaderNameMapping of all CppTarget of the Configuration) While compiling the big-hu, a request
    /// for any composing-header will map to the big-hu in these lookup tables.
    HeaderFileOrUnit findHeaderFileOrUnit(string_view headerName) const;

    /// launches the module process if it needs to be compiled
    bool isEventRegistered(Builder &builder) override;

    void completeModuleCompilation(const Builder &builder);

    /// \param message message of the c++ module or hu process if it is waiting on another dependency.
    /// \returns true if we are waiting on a dependency, false if we have completed the compilation.
    bool isEventCompleted(Builder &builder, string_view message) override;

    /// prints short status string if there is no output. prints full command + output, if there is output
    void print(const Builder &builder, const string &output) const;

    enum class CommandType
    {
        USE_IPC,
        USE_IPC_MOCK_FILE,
        CONVENTIONAL,
    };

    void getCompileCommand(std::pmr::string &compileCommand, CommandType commandType, string_view mockFilePath) const;

    /// Checks whether this needs to be updated and sets round0 RealBTarget::updateStatus to UpdateStatus::NEEDS_UPDATE.
    void setUpdateStatus() override;

    /// This function is called in standAlone mode, so the BTarget could generate stand-alone commands that could be run
    /// stand-alone without the need for the build-system.
    void generateStandAloneCommand() override;

    /// Used to generate the script for standalone hu/module compilation. Generates a batch file on Windows and a bash
    /// file on Linux.
    void cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir,
                              bool direct) override;

    void writeConfigCacheAtConfigTime(string &buffer) override;
    void writeBuildCacheAtConfigTime(string &buffer) override;
    void writeBuildCacheAtBuildTime(string &buffer) override;
    void verifyConfigCache(string_view configCache) const override;
    void verifyBuildCache(string_view buildCache) const override;
};

static_assert(alignof(CppMod) >= 2, "CppMod must be at least 2-byte aligned for PointerIntPair<CppMod*,1>");
#endif // HMAKE_CPPMOD_HPP