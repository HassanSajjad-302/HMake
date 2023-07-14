
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

using std::atomic, std::lock_guard;

class Node;
struct CompareNode
{
    using is_transparent = void;
    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const pstring_view &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const pstring_view &rhs) const;
};

inline static mutex nodeInsertMutex;

class Node
{
  public:
    pstring filePath;

  private:
    std::filesystem::file_time_type lastUpdateTime;
    atomic<bool> systemCheckCalled{};
    atomic<bool> systemCheckCompleted{};

  public:
    Node(pstring filePath_);
    pstring getFileName() const;

    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node, CompareNode> allFiles;
    std::filesystem::file_time_type getLastUpdateTime() const;

    /*    static path getFinalNodePathFromPath(const pstring &str);
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromNonNormalizedPath(const pstring &str, bool isFile, bool mayNotExist = false);*/

    static path getFinalNodePathFromPath(path filePath);

    static Node *getNodeFromNormalizedString(pstring p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNormalizedString(pstring_view p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNonNormalizedString(const pstring &p, bool isFile, bool mayNotExist = false);

    static Node *getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);
    static Node *getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist = false);

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
