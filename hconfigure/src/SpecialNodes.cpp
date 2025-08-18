
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

bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return std::tie(lhs.node, lhs.isStandard, lhs.ignoreHeaderDeps) <
           std::tie(rhs.node, rhs.isStandard, rhs.ignoreHeaderDeps);
}

HeaderUnitNode::HeaderUnitNode(Node *node_, const bool isStandard_, const bool ignoreHeaderDeps_,
                               const uint32_t targetCacheIndex_, const uint32_t headerUnitIndex_)
    : InclNode(node_, isStandard_, ignoreHeaderDeps_), targetCacheIndex(targetCacheIndex_),
      headerUnitIndex(headerUnitIndex_)
{
}

bool HeaderUnitNode::emplaceInList(list<HeaderUnitNode> &includes, HeaderUnitNode &libDirNode)
{
    for (const HeaderUnitNode &include : includes)
    {
        if (include.node == libDirNode.node)
        {
            return false;
        }
    }
    includes.emplace_back(libDirNode);
    return true;
}

HuTargetPlusDir::HuTargetPlusDir(HeaderUnitNode inclNode_, CppSourceTarget *cppSourceTarget_)
    : inclNode(inclNode_), cppSourceTarget(cppSourceTarget_)
{
}

InclNodePointerTargetMap::InclNodePointerTargetMap(const HeaderUnitNode *inclNode_, CppSourceTarget *cppSourceTarget_)
    : inclNode(inclNode_), cppSourceTarget(cppSourceTarget_)
{
}

void actuallyAddInclude(vector<InclNode> &inclNodes, const string &include, bool isStandard, bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = Node::getNodeFromNonNormalizedPath(include, false);
        bool found = false;
        for (const InclNode &inclNode : inclNodes)
        {
            if (inclNode.node->myId == node->myId)
            {
                found = true;
                printErrorMessage(FORMAT("Include {} is already added.\n", node->filePath));
                break;
            }
        }
        if (!found)
        {
            inclNodes.emplace_back(node, isStandard, ignoreHeaderDeps);
        }
    }
}

void actuallyAddInclude(vector<HuTargetPlusDir> &inclNodes, CppSourceTarget *target, const string &include,
                        bool isStandard, bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = Node::getNodeFromNonNormalizedPath(include, false);
        bool found = false;
        for (const HuTargetPlusDir &inclNode : inclNodes)
        {
            if (inclNode.inclNode.node->myId == node->myId)
            {
                found = true;
                printErrorMessage(FORMAT("Header-unit include {} is already added.\n", node->filePath));
                break;
            }
        }
        if (!found)
        {
            inclNodes.emplace_back(HeaderUnitNode(node, isStandard), target);
        }
    }
}

void actuallyAddInclude(vector<HuTargetPlusDir> &inclNodes, CppSourceTarget *target, const string &include,
                        uint64_t targetCacheIndex, uint64_t headerUnitIndex, bool isStandard, bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = Node::getNodeFromNonNormalizedPath(include, false);
        bool found = false;
        for (const HuTargetPlusDir &inclNode : inclNodes)
        {
            if (inclNode.inclNode.node->myId == node->myId)
            {
                found = true;
                printErrorMessage(FORMAT("Header-unit include {} is already added.\n", node->filePath));
                break;
            }
        }
        if (!found)
        {
            inclNodes.emplace_back(HeaderUnitNode(node, targetCacheIndex, headerUnitIndex, isStandard), target);
        }
    }
}