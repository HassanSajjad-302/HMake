/// \file
/// Defines `Node`, the interned filesystem-path record used by caches and build checks.

#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#include "BuildSystemFunctions.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"

using std::lock_guard, std::filesystem::file_time_type, std::filesystem::file_type;

class Node;
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
/// Build steps mark interesting nodes via `toBeChecked`; `Builder::checkNodes()` refreshes those.
///
/// Each `Node` has a stable 32-bit id (`myId`) used by build/config caches instead of writing full paths.
class Node
{
  public:
    // todo
    // make this string_view to improve the initial load time from Nodes.bin
    /// Normalized path (and lower-cased on Windows) for file or directory.
    string filePath;

    /// Cached filesystem type, assigned by `performSystemCheck()`.
    file_type fileType;

    /// Cached last-write timestamp, assigned by `performSystemCheck()`.
    file_time_type lastWriteTime{file_time_type::duration{UINT64_MAX}};

    /// Total number of created nodes.
    inline static uint32_t idCount = 0;

    /// Stable index in `nodeIndices`.
    uint32_t myId;

    /// True after filesystem metadata has been fetched at least once.
    bool systemCheckCompleted{false};

    /// Marks this node for refresh in the pre-round-0 node-check phase.
    bool toBeChecked = false;

    explicit Node(string_view filePath_);
    /// Returns basename (characters after final path separator).
    string getFileName() const;
    /// Returns basename without extension.
    string getFileStem() const;
    string getExtension() const;

    /// Fetches filesystem metadata once and caches it in this object.
    void performSystemCheck();

    /// Retrieves/creates a node from normalized path and validates file-vs-directory shape.
    /// \param filePath_ normalized path (lower-cased on Windows).
    /// \param isFile expected shape (`true` regular file, `false` directory).
    /// \param mayNotExist allow `not_found` without raising an error.
    static Node *getNode(string_view filePath_, bool isFile, bool mayNotExist = false);

    /// Same as `getNode`, but accepts a non-normalized path and normalizes it internally.
    static Node *getNodeNonNormalized(const string &filePath_, bool isFile, bool mayNotExist = false);

    /// Retrieves/creates node without performing filesystem checks.
    static Node *getHalfNode(string_view filePath_);

    /// Returns node by stable id index.
    static Node *getHalfNode(uint32_t index);
};

using NodeHashSet = node_hash_set<Node, NodeHash, NodeEqual>;
GLOBAL_VARIABLE(Node **, nodeIndices)
GLOBAL_VARIABLE(NodeHashSet, nodeAllFiles)

#endif // HMAKE_NODE_HPP
