/// \file
/// Defines `Node`, the interned filesystem-path record used by caches and build checks.

#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#include "BuildSystemFunctions.hpp"
#include "gtl/include/gtl/phmap.hpp"
#include <cassert>
#include <utility>

using std::filesystem::file_type;

class Node;
class AdaptiveManager;

/// Compile-time path guarantees accepted by the Node lookup APIs.
enum class PathType : uint8_t
{
    /// No path property is guaranteed.
    NEITHER = 0,
    /// Lexically normalized, but possibly relative to `normalizationBasePath`.
    NORMAL = 1,
    /// Absolute, but possibly containing redundant separators, `.` or `..` components.
    ABSOLUTE = 2,
    /// Absolute and lexically normalized (and lower-cased with native separators on Windows).
    NORMAL_ABSOLUTE = 3,
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
    /// Normalized path (and lower-cased on Windows) for file or directory. The view points into either the retained
    /// nodes-cache buffer or `nodeStrings`; both stores keep the bytes stable and NUL-terminated for the process.
    string_view filePath;

    /// Last-write timestamp restored from `nodes-cache.bin`, then replaced by `performSystemCheck()` with the current
    /// value.
    uint64_t lastWriteTime = -1;

    /// File size in bytes, populated by `performSystemCheck()` for regular files.
    uint64_t fileSize = 0;

    /// rapidhash restored from `nodes-cache.bin` or populated by `performContentHash()` when `doHashFile` is set.
    /// Missing files use `missingContentHash`, distinct from the hash of an empty file.
    uint64_t contentHash = 0;

    /// Sentinel used in fingerprints so deleting an empty header-file is observable.
    static constexpr uint64_t missingContentHash = -1;

    /// Total number of `Node` instances constructed so far (next id to assign).
    inline static uint32_t idCount = 0;

    /// Stable index in `nodeIndices`; IDs 0 and 1 are reserved for `srcNode` and `configureNode`.
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

    /// Internal constructor for a stable, NUL-terminated path view prepared by an interning helper.
    explicit Node(string_view filePath_);
    /// Returns a non-owning view of the basename (characters after the final path separator).
    [[nodiscard]] string_view getFileName() const noexcept
    {
        if (const uint64_t slashPos = filePath.find_last_of(slashc); slashPos != string_view::npos)
        {
            return {filePath.data() + slashPos + 1, filePath.size() - slashPos - 1};
        }
        return filePath;
    }
    /// Returns a non-owning view of the basename without its extension.
    [[nodiscard]] string_view getFileStem() const noexcept
    {
        const uint64_t slashPos = filePath.find_last_of(slashc);
        const uint64_t nameStart = slashPos == string_view::npos ? 0 : slashPos + 1;
        const uint64_t dotPos = filePath.find_last_of('.');
        if (dotPos == string_view::npos || dotPos <= nameStart)
        {
            return {filePath.data() + nameStart, filePath.size() - nameStart};
        }
        return {filePath.data() + nameStart, dotPos - nameStart};
    }
    /// Returns a non-owning view of the final extension, including its leading dot.
    [[nodiscard]] string_view getFileExtension() const noexcept
    {
        const uint64_t slashPos = filePath.find_last_of(slashc);
        const uint64_t nameStart = slashPos == string_view::npos ? 0 : slashPos + 1;
        const uint64_t dotPos = filePath.find_last_of('.');
        if (dotPos == string_view::npos || dotPos <= nameStart)
        {
            return {};
        }
        return {filePath.data() + dotPos, filePath.size() - dotPos};
    }
    /// Returns a non-owning view of filePath before its final host path separator.
    /// The returned view never ends with slashc.
    [[nodiscard]] string_view getDirectoryStringView() const noexcept
    {
        const uint64_t separator = filePath.find_last_of(slashc);
        return separator == string_view::npos ? string_view{} : filePath.substr(0, separator);
    }

  private:
    friend class Builder;
    friend class AdaptiveManager;
    /// Refreshes cached filesystem metadata once per process.
    void performSystemCheck();
    void performContentHash();

    static Node *getHalfNodeImpl(string_view filePath_);
    static Node *getHalfNodeImpl(string &&filePath_);
    static Node *getHalfNodeImpl(string_view filePath_, PathType pathType);
    static Node *getHalfNodeImpl(string &&filePath_, PathType pathType);
    static Node *finishNode(Node *node, bool isFile, bool mayNotExist);
    static void normalizeImpl(std::pmr::string &filePath_, PathType pathType);

  public:
    /// Returns whether a path is absolute according to the host platform's lexical path syntax.
    static bool isAbsolute(string_view fileSystemPath);

    /// Makes a PMR path absolute relative to `normalizationBasePath`, normalizes it in place, and lowercases it on
    /// Windows. `pathType` lets callers omit work for properties they already guarantee.
    template <PathType pathType = PathType::NEITHER> static void normalize(std::pmr::string &filePath_)
    {
        if constexpr (pathType != PathType::NORMAL_ABSOLUTE)
        {
            normalizeImpl(filePath_, pathType);
        }
    }

    /// Retrieves/creates a node and validates file-vs-directory shape. `pathType` states which path properties the
    /// caller already guarantees; the safe default makes no assumptions.
    /// \param isFile expected shape (`true` regular file, `false` directory).
    /// \param mayNotExist allow `not_found` without raising an error.
    template <PathType pathType = PathType::NEITHER>
    static Node *getNode(string_view filePath_, bool isFile, bool mayNotExist = false)
    {
        return finishNode(getHalfNode<pathType>(filePath_), isFile, mayNotExist);
    }

    template <PathType pathType = PathType::NEITHER>
    static Node *getNode(string &&filePath_, bool isFile, bool mayNotExist = false)
    {
        return finishNode(getHalfNode<pathType>(std::move(filePath_)), isFile, mayNotExist);
    }

    template <PathType pathType = PathType::NEITHER>
    static Node *getNode(const char *filePath_, bool isFile, bool mayNotExist = false)
    {
        return getNode<pathType>(string_view(filePath_), isFile, mayNotExist);
    }

    /// Normalizes caller-owned PMR storage in place, avoiding transient allocation when the buffer is reused.
    template <PathType pathType = PathType::NEITHER>
    static Node *getNode(std::pmr::string &filePath_, bool isFile, bool mayNotExist = false)
    {
        return finishNode(getHalfNode<pathType>(filePath_), isFile, mayNotExist);
    }

    /// Retrieves/creates a node for an entry yielded by `directory_iterator` or `recursive_directory_iterator`.
    /// Iterator paths are already normalized; this overload only lower-cases them on Windows.
    static Node *getNode(const std::filesystem::directory_entry &entry);

    /// Retrieves/creates a node without performing filesystem checks.
    template <PathType pathType = PathType::NEITHER> static Node *getHalfNode(string_view filePath_)
    {
        if constexpr (pathType == PathType::NORMAL_ABSOLUTE)
        {
            return getHalfNodeImpl(filePath_);
        }
        return getHalfNodeImpl(filePath_, pathType);
    }

    template <PathType pathType = PathType::NEITHER> static Node *getHalfNode(string &&filePath_)
    {
        if constexpr (pathType == PathType::NORMAL_ABSOLUTE)
        {
            return getHalfNodeImpl(std::move(filePath_));
        }
        return getHalfNodeImpl(std::move(filePath_), pathType);
    }

    template <PathType pathType = PathType::NEITHER> static Node *getHalfNode(const char *filePath_)
    {
        return getHalfNode<pathType>(string_view(filePath_));
    }

    /// Normalizes caller-owned PMR storage in place, avoiding transient allocation when the buffer is reused.
    template <PathType pathType = PathType::NEITHER> static Node *getHalfNode(std::pmr::string &filePath_)
    {
        normalize<pathType>(filePath_);
        return getHalfNodeImpl(string_view(filePath_));
    }

    /// Returns node by stable id index.
    static Node *getHalfNode(uint32_t index);

};

/// Transparent hashing and equality used by `NodeHashSet` for `Node` and `string_view` keys.
struct NodeHashEqual
{
    using is_transparent = void;

    uint64_t operator()(const Node &node) const
    {
        return hash_value(node.filePath);
    }

    uint64_t operator()(const string_view path) const
    {
        return hash_value(path);
    }

    bool operator()(const Node &lhs, const Node &rhs) const
    {
        return lhs.filePath == rhs.filePath;
    }

    bool operator()(const Node &lhs, const string_view rhs) const
    {
        return lhs.filePath == rhs;
    }

    bool operator()(const string_view lhs, const Node &rhs) const
    {
        return lhs == rhs.filePath;
    }
};

/// Path argument accepted by user-facing build-specification APIs.
///
/// A Node pointer avoids another normalization/lookup. String-like inputs are
/// non-owning and therefore must be resolved during the receiving function call.
struct NodeOrStr
{
    Node *node_ = nullptr;
    string_view str_;

    NodeOrStr(Node *node) : node_(node) { assert(node != nullptr); }
    NodeOrStr(const string &path) : str_(path) {}
    NodeOrStr(string_view path) : str_(path) {}
    NodeOrStr(const char *path) : str_(path) {}

    Node *resolve(const bool isFile) const
    {
        return node_ != nullptr ? node_ : Node::getNode(str_, isFile);
    }
};

using NodeHashSet = node_hash_set<Node, NodeHashEqual, NodeHashEqual>;
// nodeStrings is constructed and reserved before nodeAllFiles. It is destroyed only after nodeAllFiles so every
// string_view key remains valid throughout all hash-set operations and destruction.
GLOBAL_VARIABLE(vector<string>, nodeStrings)
GLOBAL_VARIABLE(vector<Node*>, nodeIndices)
GLOBAL_VARIABLE(NodeHashSet, nodeAllFiles)

inline Node *Node::getHalfNode(const uint32_t index)
{
    assert(index < nodeIndices.size());
    return nodeIndices[index];
}

#endif // HMAKE_NODE_HPP
