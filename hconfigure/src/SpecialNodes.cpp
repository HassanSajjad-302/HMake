
#ifdef USE_HEADER_UNITS
import "SpecialNodes.hpp";
#else
#include "SpecialNodes.hpp"
#endif

LibDirNode::LibDirNode(Node *node_, const bool isStandard_) : node{node_}, isStandard{isStandard_}
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

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, Node *node_, bool isStandard_)
{
    for (const LibDirNode &libDirNode : libDirNodes)
    {
        if (libDirNode.node == node_)
        {
            return;
        }
    }
    libDirNodes.emplace_back(node_, isStandard_);
}

InclNode::InclNode(Node *node_, const bool isStandard_, const bool ignoreHeaderDeps_)
    : LibDirNode(node_, isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
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

bool InclNode::emplaceInList(list<InclNode> &includes, Node *node_, bool isStandard_, bool ignoreHeaderDeps_)
{
    for (const InclNode &include : includes)
    {
        if (include.node == node_)
        {
            return false;
        }
    }
    includes.emplace_back(node_, isStandard_, ignoreHeaderDeps_);
    return true;
}