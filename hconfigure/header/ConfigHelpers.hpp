
#ifndef CONFIGHELPERS_H
#define CONFIGHELPERS_H

#ifdef USE_HEADER_UNITS
import "Node.hpp";
import "SMFile.hpp";
import "SpecialNodes.hpp";
#else
#include "Node.hpp"
#include "SMFile.hpp"
#include "SpecialNodes.hpp"
#endif

template <typename T> static const InclNode &getNode(const T &t)
{
    if constexpr (std::is_same_v<T, struct InclNodeTargetMap>)
    {
        return t.inclNode;
    }
    else if constexpr (std::is_same_v<T, InclNode>)
    {
        return t;
    }
}

template <typename T> void writeIncDirsAtConfigTime(const vector<T> &include, PValue &pValue, auto &rapidJsonAllocator)
{
    pValue.Reserve(include.size(), rapidJsonAllocator);
    for (auto &elem : include)
    {
        const InclNode &inclNode = getNode(elem);
        pValue.PushBack(inclNode.node->getPValue(), rapidJsonAllocator);
        pValue.PushBack(inclNode.isStandard, rapidJsonAllocator);
        pValue.PushBack(inclNode.ignoreHeaderDeps, rapidJsonAllocator);
    }
}

template <typename T> const Node *getNodeForEquality(const T &t)
{
    if constexpr (std::is_same_v<T, struct InclNodeTargetMap>)
    {
        return t.inclNode.node;
    }
    else if constexpr (std::is_same_v<T, InclNode>)
    {
        return t.node;
    }
    else if constexpr (std::is_same_v<T, SourceNode>)
    {
        return t.node;
    }
    else if constexpr (std::is_same_v<T, SMFile>)
    {
        return t.node;
    }
    else if constexpr (std::is_same_v<T, PValue>)
    {
        return Node::getNodeFromPValue(t, true, true);
    }
}

template <typename T> void testVectorHasUniqueElements(const T &container, const pstring &containerName)
{
    for (auto &elem : container)
    {
        uint8_t count = 0;
        for (auto &elem2 : container)
        {
            if (getNodeForEquality(elem)->filePath == getNodeForEquality(elem2)->filePath)
            {
                ++count;
            }
        }
        if (count != 1)
        {
            printErrorMessage(
                fmt::format("Repeat Value {} in container {}\n", getNodeForEquality(elem)->filePath, containerName));
        }
    }
}

void testVectorHasUniqueElementsIncrement(const PValue &container, const pstring &containerName, uint64_t increment);

#endif // CONFIGHELPERS_H