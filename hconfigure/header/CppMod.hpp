/// \file
/// Defines source-file and C++ module compilation targets.

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
struct HfOrCppMod;

struct CompareCppSrc
{
    /// Enables heterogeneous lookup by either `CppSrc` or its source `Node`.
    using is_transparent = void;
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

/// Compiles one C, C++, or assembly translation unit into an object file.
class CppSrc : public ObjectFile
{
  public:
    /// Headers discovered during compilation. A set removes duplicate compiler output and keeps the cache compact.
    flat_hash_set<Node *> headerFiles;

    /// The back pointer to the CppTarget owning this in srcFileDeps.
    CppTarget *target;

    /// Source file.
    const Node *node;

    /// Hash of the compile command for this file (flags, defines, includes, etc.). Set in
    /// `CppTarget::setCommandHashes()`. Combined with source/header content hashes to form
    /// `RealBTarget::cumulativeHash`.
    uint64_t commandHash;

    /// Language inferred from the source extension.
    SourceType sourceType = SourceType::CPP;

    /// True only for an adaptive manager's generated jumbo translation unit. Such a unit reaches a target's
    /// pre-compilation barrier through AdaptiveManager and must not receive a redundant direct edge.
    bool isAJumboBuild = false;

    /// Header-file node indices restored from build-cache (`Node::getHalfNode(index)`), used in `setUpdateStatus()`.
    span<const uint32_t> cachedHeaderFiles;

    CppSrc(CppTarget *target_, const Node *node_, CppModType cppModType);
    string getPrintName() const override;
    void getCompileCommand(std::pmr::string &compileCommand) const;
    /// MSVC prints header-files with the compilation output. This function parses them out from that output.
    void parseHeadersFromMSVCTextOutput(string &output, bool isClang);
    /// Parses header dependencies from a GCC-compatible `.d` file.
    void parseHeadersFromGccDepsOutput();
    /// Dispatches to the dependency parser for the selected compiler.
    void parseHeaderDeps(string &output);
    /// Computes the input fingerprint and decides whether recompilation is required.
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

    /// Headers composed directly into this module or header unit. Unlike `CppSrc::headerFiles`, this excludes headers
    /// inherited from dependencies.
    flat_hash_map<string, Node *> composingHeaders;

    vector<string_view> composingNames;

    /// Include name for a header unit, or exported name for a module.
    string_view logicalName;

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

    /// Whether composing headers have already been sent in the first IPC message.
    bool firstMessageSent = false;

    /// True after `makeMemoryFileMapping()` has mapped `interfaceNode` and recorded `interfaceFileSize`.
    bool memoryMappingCompleted = false;

    /// With `realBTargets[0].insertionIndex`, allows one bring-to-front per dependency: while it is already in
    /// `readyBTargets` but `isEventRegistered` has not run and it has not yet been moved to the head (`!isScheduled`).
    bool isScheduled = false;

    /// True after `populateAllDeps()` has filled `allCppModDeps` from `cachedDeps` and transitive closure.
    bool isAllDepsPopulated = false;

    CppMod(CppTarget *target_, const Node *node_, CppModType cppModType);

    /// Ensures that a shared-memory BMI exists before it is sent to the compiler.
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
    std::optional<HfOrCppMod> findHfOrCppMod(string_view headerName) const;

    /// Launches the module process when recompilation is required.
    bool isEventRegistered(Builder &builder) override;

    /// Resumes an already-running compiler after its dynamically requested module/header-unit finishes. If the recorded
    /// update reason has completed with `UPDATE_NOT_NEEDED`, fully re-evaluates the consumer against its still-cached
    /// completion time. A now-unneeded speculative compiler is terminated; otherwise it receives the provider response
    /// and continues.
    /// This is separate from `isEventCompleted()` so an empty message remains an unambiguous process-exit sentinel.
    /// See "Unchanged-output cutoff" in the project README.
    bool resumeAfterDependency(Builder &builder);

    void completeModuleCompilation(const Builder &builder);

    /// \param message A nonempty CTB protocol payload, or an empty view only when Builder has reaped the compiler.
    /// \returns true when another compiler message is expected; false when compilation has completed.
    bool isEventCompleted(Builder &builder, string_view message) override;

    /// Prints a short status line on success, or the full command and compiler output when present.
    void print(const Builder &builder, const string &output) const;

    enum class CommandType
    {
        USE_IPC,
        USE_IPC_MOCK_FILE,
        CONVENTIONAL,
    };

    void getCompileCommand(std::pmr::string &compileCommand, CommandType commandType, string_view mockFilePath) const;

    /// Computes the module input fingerprint and updates its round-0 `UpdateStatus`.
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

/// Round-one partitioner for one `CppTarget`.
///
/// Configure mode creates cache entries for every possible standalone and generated-jumbo compile unit. Build mode
/// uses the source-control working set and current file sizes to construct only the units selected for this invocation.
class AdaptiveManager : public BTarget
{
  public:
    CppTarget *target = nullptr;
    bool roundOneCompleted = false;

    inline static flat_hash_set<const Node *> workingSet;

    explicit AdaptiveManager(CppTarget *target_);

    /// Caches one Git/Perforce query and incrementally refreshes adaptive candidates as configurations finish.
    static void prepareWorkingSet();

    void completeRoundOne() override;
    string getPrintName() const override;
};
#endif // HMAKE_CPPMOD_HPP
