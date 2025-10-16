
#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "parallel_hashmap/phmap.h";
import <atomic>;
#else
#include "BuildSystemFunctions.hpp"
#include "phmap.h"
#include <atomic>
#endif

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

class Node
{
  public:
    string filePath;

    file_type fileType;
    file_time_type lastWriteTime{file_time_type::duration{UINT64_MAX}};

    inline static uint32_t idCount = 0;
    inline static uint32_t idCountCompleted = 0;
    // Used in multi-threading context. So, can not emplace_back. size should be same as size of nodeAllFiles
    inline static vector<Node *> nodeIndices{60 * 1000};
    uint32_t myId = UINT32_MAX;

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

    static Node *getNodeFromNormalizedString(string p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNormalizedString(string_view p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNonNormalizedString(const string &p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

    static Node *addHalfNodeFromNormalizedStringSingleThreaded(string normalizedFilePath);
    static Node *getHalfNode(string_view p);
    static Node *getHalfNode(uint32_t);

    static void clearNodes();
};

//  This keeps info if a file is touched. If it's touched, it's not touched again.
inline phmap::parallel_node_hash_set_m<Node, NodeHash, NodeEqual> nodeAllFiles{10000};
#endif // HMAKE_NODE_HPP
