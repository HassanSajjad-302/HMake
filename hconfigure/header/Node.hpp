
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

using std::atomic, std::lock_guard, std::filesystem::file_time_type;

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

    file_time_type lastWriteTime;

    inline static atomic<uint32_t> idCount = 0;
    inline static atomic<uint32_t> idCountCompleted = 0;
    // Used in multi-threading context. So, can not emplace_back. size should be same as size of nodeAllFiles
    inline static vector<Node *> nodeIndices{10000};
    uint32_t myId = UINT32_MAX;

    // While following are not atomic to keep Node copyable and moveable, all operations on these bools are done
    // atomically.
    bool systemCheckCompleted{false};

  private:
    bool systemCheckCalled = false;

  public:
    Node(Node *&node, string filePath_);
    explicit Node(string filePath_);
    string getFileName() const;
    Value getValue() const;

    static path getFinalNodePathFromPath(path filePath);

    void ensureSystemCheckCalled(bool isFile, bool mayNotExist = false);

    // This function does not do the while looping waiting for the other threads to call performSystemCheck. It either
    // calls performSystemCheck or returns.
    bool trySystemCheck(bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNormalizedString(string p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNormalizedString(string_view p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNormalizedStringNoSystemCheckCalled(string_view p);

    static Node *getNodeFromNonNormalizedString(const string &p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

    static Node *addHalfNodeFromNormalizedStringSingleThreaded(string normalizedFilePath);
    static Node *getHalfNodeFromNormalizedString(string_view p);
    static Node *getNodeFromValue(const Value &pValue, bool isFile, bool mayNotExist = false);
    static Node *getNotSystemCheckCalledNodeFromValue(const Value &pValue);
    static Node *tryGetNodeFromValue(bool &systemCheckSucceeded, const Value &pValue, bool isFile,
                                      bool mayNotExist = false);

    static Node *getLastNodeAdded();
    static rapidjson::Type getType();

  private:
    void performSystemCheck(bool isFile, bool mayNotExist);

  public:
    bool doesNotExist = false;
    static void clearNodes();
};

// flat_hash_set is used for speed but maybe we
//  This keeps info if a file is touched. If it's touched, it's not touched again.
inline phmap::parallel_node_hash_set_m<Node, NodeHash, NodeEqual> nodeAllFiles{10000};
#endif // HMAKE_NODE_HPP
