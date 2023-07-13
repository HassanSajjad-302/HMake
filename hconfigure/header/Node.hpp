
#ifndef HMAKE_NODE_HPP
#define HMAKE_NODE_HPP

#ifdef USE_HEADER_UNITS
import <atomic>;
import "BuildSystemFunctions.hpp";
import "PlatformSpecific.hpp";
#else
#include "BuildSystemFunctions.hpp"
#include <atomic>
#endif

using std::atomic;

class Node;
struct CompareNode
{
    using is_transparent = void;
    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const pstring &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const pstring &rhs) const;
};

class Node
{
  public:
    pstring filePath;

  private:
    std::filesystem::file_time_type lastUpdateTime;
    atomic<bool> systemCheckCalled{};
    atomic<bool> systemCheckCompleted{};

  public:
    Node(const pstring &filePath_);
    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node, CompareNode> allFiles;
    std::filesystem::file_time_type getLastUpdateTime() const;

    /*    static path getFinalNodePathFromPath(const pstring &str);
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromNonNormalizedPath(const pstring &str, bool isFile, bool mayNotExist = false);*/

    static path getFinalNodePathFromPath(path filePath);

    // getNodeFromNonNormalizedPath
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

    // getNodeFromNormalizedPath
    static Node *getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

    /*    static Node *getNodeFromPath(const path &p, bool isFile, bool mayNotExist = false);
        static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);*/

  private:
    void performSystemCheck(bool isFile, bool mayNotExist);
    // Because checking for lastUpdateTime is expensive, it is done only once even if file is used in multiple targets.
    bool isUpdated = false;

  public:
    bool doesNotExist = false;
};
bool operator<(const Node &lhs, const Node &rhs);
void to_json(Json &j, const Node *node);

#endif // HMAKE_NODE_HPP
