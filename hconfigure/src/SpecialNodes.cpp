
#include "SpecialNodes.hpp"

LibDirNode::LibDirNode(Node *node_) : node{node_}
{
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode)
{
    for (const LibDirNode &libDirNode_ : libDirNodes)
    {
        if (libDirNode_.node == libDirNode.node)
        {
            return;
        }
    }
    libDirNodes.emplace_back(libDirNode);
}

InclNode::InclNode(Node *node_, const bool isStandard_, const bool ignoreHeaderDeps_)
    : LibDirNode(node_), isStandard(isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
{
}

bool InclNode::emplaceInList(list<InclNode> &includes, InclNode &libDirNode)
{
    for (const InclNode &include : includes)
    {
        if (include.node == libDirNode.node)
        {
            return false;
        }
    }
    includes.emplace_back(libDirNode);
    return true;
}

bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return lhs.node < rhs.node;
}