
#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "parallel_hashmap/phmap.h";
import <atomic>;
import <unordered_set>;
#else
#include "BuildSystemFunctions.hpp"
#include "phmap.h"
#include <atomic>
#include <unordered_set>
#endif

using std::atomic, std::lock_guard, std::unordered_set, std::filesystem::file_time_type;

class Node;
struct NodeEqual
{
    using is_transparent = void;

    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const pstring_view &rhs) const;
    bool operator()(const pstring_view &lhs, const Node &rhs) const;
};

struct NodeHash
{
    using is_transparent = void; // or std::equal_to<>

    std::size_t operator()(const Node &node) const;
    std::size_t operator()(const pstring_view &str) const;
};
struct CompareNode
{
    using is_transparent = void;
    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const pstring_view &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const pstring_view &rhs) const;
};

class Node
{
  public:
    pstring filePath;

    file_time_type lastWriteTime;

    inline static atomic<uint32_t> idCount = 0;
    // Used in multi-threading context. So, can not emplace_back. size should be same as size of nodeAllFiles
    inline static vector<Node *> nodeIndices{10000};
    uint32_t myId;

    // While following are not atomic to keep Node copyable and moveable, all operations on these bools are done
    // atomically.
    bool systemCheckCompleted{false};

  private:
    bool systemCheckCalled = false;

  public:
    explicit Node(pstring filePath_);
    pstring getFileName() const;
    PValue getPValue() const;

    static path getFinalNodePathFromPath(path filePath);

    void ensureSystemCheckCalled(bool isFile, bool mayNotExist = false);

    // This function does not do the while looping waiting for the other threads to call performSystemCheck. It either
    // calls performSystemCheck or returns.
    bool trySystemCheck(bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNormalizedString(pstring p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNormalizedString(pstring_view p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNonNormalizedString(const pstring &p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

    static Node *getHalfNodeFromNormalizedString(pstring normalizedFilePath);
    static void emplaceNodeInPValue(const Node *node, PValue &pValue);
    static void emplaceNodeInPValue(const Node *node, PValue &pValue, decltype(ralloc) alloc);
    static Node *getNodeFromPValue(const PValue &pValue, bool isFile, bool mayNotExist = false);

    static Node *tryGetNodeFromPValue(bool &systemCheckSucceeded, const PValue &pValue, bool isFile,
                                      bool mayNotExist = false);

  private:
    void performSystemCheck(bool isFile, bool mayNotExist);

  public:
    bool doesNotExist = false;
    bool loadedFromNodesCache = false;
    static void clearNodes();
};

//  This keeps info if a file is touched. If it's touched, it's not touched again.
inline phmap::parallel_flat_hash_set_m<Node, NodeHash, NodeEqual> nodeAllFiles{10000};
#endif // HMAKE_NODE_HPP
