
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
    // Include classification is path-specific: a target can inherit ordinary and system directories from different
    // producers. Preserve it through transitive propagation and the configuration cache so command generation can
    // choose -I versus -isystem correctly.
    bool isStandard = false;
    InclNode(Node *node_, const bool isStandard_)
        : LibDirNode(node_), isStandard(isStandard_)
    {
    }
};

inline bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return lhs.node < rhs.node;
}

#endif // SPECIALNODES_HPP
