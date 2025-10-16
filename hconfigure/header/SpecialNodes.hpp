
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
    uint32_t targetCacheIndex = -1;
    uint32_t headerUnitIndex = -1;
    explicit HeaderUnitNode(Node *node_, bool isStandard_ = false, bool ignoreHeaderDeps_ = false,
                            uint32_t targetCacheIndex_ = UINT32_MAX, uint32_t headerUnitIndex_ = UINT32_MAX);
    static bool emplaceInList(list<HeaderUnitNode> &includes, HeaderUnitNode &libDirNode);
};

struct HuTargetPlusDir
{
    HeaderUnitNode inclNode;
    class CppSourceTarget *cppSourceTarget;
    HuTargetPlusDir(HeaderUnitNode inclNode_, CppSourceTarget *cppSourceTarget_);
};

/*
void actuallyAddInclude(vector<HuTargetPlusDir> &inclNodes, CppSourceTarget *target, const string &include,
                        bool isStandard = false, bool ignoreHeaderDeps = false);
void actuallyAddInclude(vector<HuTargetPlusDir> &inclNodes, CppSourceTarget *target, const string &include,
                        uint64_t targetCacheIndex, uint64_t headerUnitIndex, bool isStandard = false,
                        bool ignoreHeaderDeps = false);
                        */

#endif // SPECIALNODES_HPP
