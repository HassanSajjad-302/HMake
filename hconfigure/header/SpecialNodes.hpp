
#ifndef SPECIALNODES_HPP
#define SPECIALNODES_HPP

class Node;

class LibDirNode
{
  public:
    Node *node = nullptr;
    LibDirNode(Node *node_) : node{node_}
    {
    }
};

class InclNode : public LibDirNode
{
  public:
    // TODO
    // remove following. These are not needed as isStandard and ignoreHeaderDeps are now properties of CppTarget and
    // further granularity is unnecessary. Used with includeDirs to specify whether to ignore include-files from these
    // dirs from being stored in target-cache file
    bool isStandard = false;
    bool ignoreHeaderDeps = false;
    InclNode(Node *node_, const bool isStandard_, const bool ignoreHeaderDeps_)
        : LibDirNode(node_), isStandard(isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
    {
    }
};

inline bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return lhs.node < rhs.node;
}

#endif // SPECIALNODES_HPP
