/// \file
/// This class has definition for Node class

#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#include "BuildSystemFunctions.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <atomic>

using std::atomic, std::lock_guard, std::filesystem::file_time_type, std::filesystem::file_type;

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
/// path. An invariant is that all paths added in nodeAllFiles must be normalized and lower-cased (on Windows). Some of
/// these Node::get* functions also call Node::ensureSystemCheckCalled. This function checks the file existence and
/// retrieves its last-write-time. Most of these Node::get* functions are thread-safe.
///
///  Node::ensureSystemCheckCalled ensures that this info is checked just once as it is a slow operation. In build-mode,
///  usually this is done in parallel by Builder::execute in ExecuteMode::NODE_CHECK. Before this mode,
///  BTarget::updateBTarget of different BTargets will set Node::toBeChecked for different Nodes. Then
///  Builder::executeMutex will go over all nodes of nodeIndices array and collect those with Node::toBeChecked == true.
///  Then all threads will go over same size chunks of this collection and call Node::ensureSystemCheckCalled in
///  parallel. This improves zero-target build speed as this is a slow operation.
///
///  The constructor of this class assigns an incrementing id to its object. And also assigns itself at the id index in
///  nodeIndices array. In config-cache and build-cache, instead of the path, this 4-byte id is stored. And before
///  writing config-cache or build-cache, the nodeIndices array is written to node-cache. This is also read first and
///  nodeAllFiles set is initialized before reading the config-cache or build-cache. Node cache array only increases and
///  can not be removed from.
class Node
{
  public:
    /// The normalized (and lower-case on windows) path representing the file or directory
    string filePath;

    /// std::filesystem::file_type. assigned in ensureSystemCheckCalled
    file_type fileType;

    /// assigned in ensureSystemCheckCalled
    file_time_type lastWriteTime{file_time_type::duration{UINT64_MAX}};

    inline static uint32_t idCount = 0;

    uint32_t myId;

    // While following are not atomic to keep Node copyable and moveable, all operations on these bools are done
    // atomically.
    bool systemCheckCompleted{false};

    // private:
    bool systemCheckCalled = false;
    bool toBeChecked = false;

    Node(Node *&node, string filePath_);
    explicit Node(string filePath_);
    string getFileName() const;
    string getFileStem() const;

    void performSystemCheck();
    void ensureSystemCheckCalled(bool isFile, bool mayNotExist = false);

    static Node *getNode(string_view p, bool isFile, bool mayNotExist = false);
    static Node *getNodeNonNormalized(const string &p, bool isFile, bool mayNotExist = false);

    static Node *getHalfNodeST(string normalizedFilePath);
    static Node *getHalfNode(string_view p);
    static Node *getHalfNode(uint32_t);
};

using NodeHashSet = phmap::parallel_node_hash_set_m<Node, NodeHash, NodeEqual>;
GLOBAL_VARIABLE(Node **, nodeIndices)
GLOBAL_VARIABLE(NodeHashSet, nodeAllFiles)

#endif // HMAKE_NODE_HPP
