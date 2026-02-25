/// \file
/// This class has definition for Node class

#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#include "BuildSystemFunctions.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"

using std::lock_guard, std::filesystem::file_time_type, std::filesystem::file_type;

class Node;
struct NodeEqual
{
    using is_transparent = void;

    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const string_view &rhs) const;
    bool operator()(const string_view &lhs, const Node &rhs) const;
};

struct NodeHash
{
    using is_transparent = void; // or std::equal_to<>

    std::size_t operator()(const Node &node) const;
    std::size_t operator()(const string_view &str) const;
};

/// This class is used to store the filesystem path in build-cache or config-cache. This class constructor is private.
/// Instead, Node::get* functions are used to retrieve a Node* which points to a pointer in nodeAllFiles set. This set
/// ensures uniqueness based on the path string. Some Node::get* functions normalize (and lower-case on windows) the
/// path. An invariant is that all paths added in nodeAllFiles must be normalized and lower-cased (on Windows).
///
///  Node::performSystemCheck is called just once as it is a slow operation. In build-mode, usually this is done in
///  parallel by Builder::nodeCheck. BTarget::completeRoundOne of different BTargets will set Node::toBeChecked for
///  different Nodes. Node::performSystemCheck is called only for these nodes.
///
///  The constructor of this class assigns an incrementing id to its object. And also assigns itself at the id index in
///  nodeIndices array. In config-cache and build-cache, instead of the path, this 4-byte id is stored. And before
///  writing config-cache or build-cache, the nodeIndices array is written to node-cache. This is also read first and
///  nodeAllFiles set is initialized before reading the config-cache or build-cache. Node cache array only increases and
///  can not be removed from.
///
///  All Node::* function also call Node::performSystemCheck except those with half in their name.
///  All Node::* function expect normalized-path except those with NonNormalized in their name.
class Node
{
  public:
    // todo
    // make this string_view to improve the initial load time from Nodes.bin
    /// The normalized (and lower-case on windows) path representing the file or directory
    string filePath;

    /// std::filesystem::file_type. assigned in Node::performSystemCheck.
    file_type fileType;

    /// assigned in performSystemCheck.
    file_time_type lastWriteTime{file_time_type::duration{UINT64_MAX}};

    inline static uint32_t idCount = 0;

    uint32_t myId;

    bool systemCheckCompleted{false};

    /// if set to true, Builder::executeMutex will call Node::performSystemCheck for this and all such nodes in parallel
    /// in ExecuteMode::NODE_CHECK before round 0 ExecuteMode::GENERAL.
    bool toBeChecked = false;

    explicit Node(string_view filePath_);
    string getFileName() const;
    string getFileStem() const;

    /// slow file-system call that initializes lastWriteTime and fileType variable.
    void performSystemCheck();

    /// filePath_ must be normalized string (and lower-cased on Windows). This function will give an error if after
    /// calling performSystemCheck, attributes do not match the provided arguments.
    static Node *getNode(string_view filePath_, bool isFile, bool mayNotExist = false);

    /// This function will give an error if after calling performSystemCheck, attributes do not match the provided
    /// arguments.
    static Node *getNodeNonNormalized(const string &filePath_, bool isFile, bool mayNotExist = false);

    /// Does not performSystemCheck.
    static Node *getHalfNode(string_view filePath_);

    /// returns the Node* at nodeIndices[index]
    static Node *getHalfNode(uint32_t index);
};

using NodeHashSet = node_hash_set<Node, NodeHash, NodeEqual>;
GLOBAL_VARIABLE(Node **, nodeIndices)
GLOBAL_VARIABLE(NodeHashSet, nodeAllFiles)

#endif // HMAKE_NODE_HPP
