
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
};

class InclNode : public LibDirNode
{
  public:
    // Used with includeDirs to specify whether to ignore include-files from these dirs from being stored
    // in target-cache file
    bool ignoreHeaderDeps = false;
    explicit InclNode(Node *node_, bool isStandard_ = false, bool ignoreHeaderDeps_ = false);
    static bool emplaceInList(list<InclNode> &includes, InclNode &libDirNode);
};
bool operator<(const InclNode &lhs, const InclNode &rhs);

class HeaderUnitNode : public InclNode
{
  public:
    uint64_t targetCacheIndex = UINT64_MAX;
    uint64_t headerUnitIndex = UINT64_MAX;
    explicit HeaderUnitNode(Node *node_, uint64_t targetCacheIndex_ = UINT32_MAX,
                            uint64_t headerUnitIndex_ = UINT32_MAX, bool isStandard_ = false,
                            bool ignoreHeaderDeps_ = false);
    static bool emplaceInList(list<HeaderUnitNode> &includes, HeaderUnitNode &libDirNode);
};

struct InclNodeTargetMap
{
    HeaderUnitNode inclNode;
    class CppSourceTarget *cppSourceTarget;
    InclNodeTargetMap(HeaderUnitNode inclNode_, CppSourceTarget *cppSourceTarget_);
};

struct InclNodePointerTargetMap
{
    const HeaderUnitNode *inclNode;
    CppSourceTarget *cppSourceTarget;
    InclNodePointerTargetMap(const HeaderUnitNode *inclNode_, CppSourceTarget *cppSourceTarget_);
};

void actuallyAddInclude(vector<InclNode> &inclNodes, const string &include, bool isStandard = false,
                        bool ignoreHeaderDeps = false);
void actuallyAddInclude(vector<InclNodeTargetMap> &inclNodes, CppSourceTarget *target, const string &include,
                        bool isStandard = false, bool ignoreHeaderDeps = false);
void actuallyAddInclude(vector<InclNodeTargetMap> &inclNodes, CppSourceTarget *target, const string &include,
                        uint64_t targetCacheIndex, uint64_t headerUnitIndex, bool isStandard = false,
                        bool ignoreHeaderDeps = false);

#endif // SPECIALNODES_HPP
