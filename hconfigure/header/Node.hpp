/// \file
/// Defines `Node`, the interned filesystem-path record used by caches and build checks.

#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#include "BuildSystemFunctions.hpp"
#include "gtl/include/gtl/phmap.hpp"

using std::lock_guard, std::filesystem::file_time_type, std::filesystem::file_type;

class Node;
class AdaptiveManager;
/// Heterogeneous equality for `NodeHashSet` lookups (`Node` and `string_view`).
struct NodeEqual
{
    using is_transparent = void;

    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const string_view &rhs) const;
    bool operator()(const string_view &lhs, const Node &rhs) const;
};

/// Heterogeneous hash for `NodeHashSet` lookups (`Node` and `string_view`).
struct NodeHash
{
    using is_transparent = void;

    std::size_t operator()(const Node &node) const;
    std::size_t operator()(const string_view &str) const;
};

/// Interned representation of one filesystem path.
///
/// `Node` objects are unique by normalized path and stored in `nodeAllFiles`.
/// Most callers should obtain instances via `getNode*`/`getHalfNode*` helpers, not by direct construction.
///
/// `performSystemCheck()` is intentionally cached because filesystem metadata calls are slow.
/// Build steps mark interesting nodes via `doStatFile` / `doHashFile`; `Builder::checkNodes()` refreshes those.
///
/// Each `Node` has a stable 32-bit id (`myId`) used by build/config caches instead of writing full paths.
class Node
{
  public:
    // This owns the path because nodes outlive the cache buffer from which they may be loaded.
    /// Normalized path (and lower-cased on Windows) for file or directory.
    string filePath;

    /// Last-write timestamp restored from `nodes.bin`, then replaced by `performSystemCheck()` with the current value.
    uint64_t lastWriteTime = UINT64_MAX;

    /// File size in bytes, populated by `performSystemCheck()` for regular files.
    uint64_t fileSize = 0;

    /// rapidhash restored from `nodes.bin` or populated by `performContentHash()` when `doHashFile` is set.
    /// Missing files use `missingContentHash`, distinct from the hash of an empty file.
    uint64_t contentHash = 0;

    /// Sentinel used in fingerprints so deleting an empty header-file is observable.
    inline static constexpr uint64_t missingContentHash = UINT64_MAX;

    /// Total number of `Node` instances constructed so far (next id to assign).
    inline static uint32_t idCount = 0;

    /// Stable index in `nodeIndices`.
    uint32_t myId;

    /// Cached filesystem type, assigned by `performSystemCheck()`.
    file_type fileType = file_type::none;

    /// True after filesystem metadata has been fetched at least once.
    bool statCompleted{false};

    /// When true, `Builder::checkNodes()` will call `performSystemCheck()` to refresh `fileType`, `fileSize`, and
    /// `lastWriteTime`.
    bool doStatFile : 1 = false;

    /// When true, `Builder::checkNodes()` resolves `contentHash`, reusing the persisted value when the timestamp
    /// observed by `performSystemCheck()` is unchanged.
    bool doHashFile : 1 = false;

    /// True once `contentHash` has been resolved from the cache, the missing-file sentinel, or file contents.
    bool hashCompleted : 1 = false;

    explicit Node(string_view filePath_);
    /// Returns basename (characters after final path separator).
    string getFileName() const;
    /// Returns basename without extension.
    string getFileStem() const;
    string getExtension() const;
    /// Returns a non-owning view of filePath before its final host path separator.
    /// The returned view never ends with slashc.
    string_view getDirectoryStringView() const;

  private:
    friend class Builder;
    friend class AdaptiveManager;
    /// Refreshes cached filesystem metadata once per process.
    void performSystemCheck();
    void performContentHash();

  public:
    /// Retrieves/creates a node from normalized path and validates file-vs-directory shape.
    /// \param filePath_ normalized path (lower-cased on Windows).
    /// \param isFile expected shape (`true` regular file, `false` directory).
    /// \param mayNotExist allow `not_found` without raising an error.
    static Node *getNode(string_view filePath_, bool isFile, bool mayNotExist = false);

    /// Retrieves/creates a node for an entry yielded by `directory_iterator` or `recursive_directory_iterator`.
    /// Iterator paths are already normalized; this overload only lower-cases them on Windows.
    static Node *getNode(const std::filesystem::directory_entry &entry);

    /// Same as `getNode`, but accepts a non-normalized path and normalizes it internally.
    static Node *getNodeNonNormalized(const string &filePath_, bool isFile, bool mayNotExist = false);

    /// Retrieves/creates node without performing filesystem checks.
    static Node *getHalfNode(string_view filePath_);

    /// Same as `getHalfNode`, but accepts a non-normalized path and normalizes it internally.
    static Node *getHalfNodeNonNormalized(string_view filePath_);

    /// Returns node by stable id index.
    static Node *getHalfNode(uint32_t index);
};

using NodeHashSet = node_hash_set<Node, NodeHash, NodeEqual>;
GLOBAL_VARIABLE(vector<Node*>, nodeIndices)
GLOBAL_VARIABLE(NodeHashSet, nodeAllFiles)

#endif // HMAKE_NODE_HPP
