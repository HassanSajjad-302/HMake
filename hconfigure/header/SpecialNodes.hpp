
#ifndef SPECIALNODES_HPP
#define SPECIALNODES_HPP

#ifdef USE_HEADER_UNITS
import "Node.hpp";
import <list>;
#else
#include "Node.hpp"
#include <list>
#endif

using std::list;
class LibDirNode
{
  public:
    Node *node = nullptr;
    bool isStandard = false;
    explicit LibDirNode(Node *node_, bool isStandard_ = false);
    static void emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode);
    static void emplaceInList(list<LibDirNode> &libDirNodes, Node *node_, bool isStandard_ = false);
};

class InclNode : public LibDirNode
{
  public:
    // Used with includeDirectories to specify whether to ignore include-files from these directories from being stored
    // in target-cache file
    bool ignoreHeaderDeps = false;
    explicit InclNode(Node *node_, bool isStandard_ = false, bool ignoreHeaderDeps_ = false);
    static bool emplaceInList(list<InclNode> &includes, InclNode &libDirNode);
    static bool emplaceInList(list<InclNode> &includes, Node *node_, bool isStandard_ = false,
                              bool ignoreHeaderDeps_ = false);
};
bool operator<(const InclNode &lhs, const InclNode &rhs);

struct InclNodeTargetMap
{
    InclNode inclNode;
    class CppSourceTarget *cppSourceTarget;
    InclNodeTargetMap(InclNode inclNode_, CppSourceTarget *cppSourceTarget_);
};

struct InclNodePointerTargetMap
{
    const InclNode *inclNode;
    CppSourceTarget *cppSourceTarget;
    InclNodePointerTargetMap(const InclNode *inclNode_, CppSourceTarget *cppSourceTarget_);
};

void actuallyAddInclude(vector<InclNode> &inclNodes, const string &include, bool isStandard = false,
                        bool ignoreHeaderDeps = false);
void actuallyAddInclude(vector<InclNodeTargetMap> &inclNodes, CppSourceTarget *target, const string &include,
                        bool isStandard = false, bool ignoreHeaderDeps = false);

#endif // SPECIALNODES_HPP
