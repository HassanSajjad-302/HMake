
#ifndef SPECIALNODES_HPP
#define SPECIALNODES_HPP

#include "Node.hpp"
#include <list>

using std::list;
class LibDirNode
{
  public:
    Node *node = nullptr;
    explicit LibDirNode(Node *node_);
    static void emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode);
};

class InclNode : public LibDirNode
{
  public:
    // Used with includeDirs to specify whether to ignore include-files from these dirs from being stored
    // in target-cache file
    bool isStandard = false;
    bool ignoreHeaderDeps = false;
    explicit InclNode(Node *node_, bool isStandard_ = false, bool ignoreHeaderDeps_ = false);
    static bool emplaceInList(list<InclNode> &includes, InclNode &libDirNode);
};
bool operator<(const InclNode &lhs, const InclNode &rhs);

#endif // SPECIALNODES_HPP
