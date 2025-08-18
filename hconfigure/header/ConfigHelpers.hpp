
#ifndef CONFIGHELPERS_H
#define CONFIGHELPERS_H

#ifdef USE_HEADER_UNITS
import "Node.hpp";
import "SMFile.hpp";
import "SpecialNodes.hpp";
import "TargetCache.hpp";
#else
#include "Node.hpp"
#include "SMFile.hpp"
#include "SpecialNodes.hpp"
#include "TargetCache.hpp"
#endif

template <typename T> const Node *getNodeForEquality(const T &t)
{
    if constexpr (std::is_same_v<T, struct HuTargetPlusDir>)
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
    else if constexpr (std::is_same_v<T, Value>)
    {
        return Node::getNodeFromValue(t, true, true);
    }
}

template <typename T> void testVectorHasUniqueElements(const T &container, const string &containerName)
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
                FORMAT("Repeat Value {} in container {}\n", getNodeForEquality(elem)->filePath, containerName));
        }
    }
}

void testVectorHasUniqueElementsIncrement(const Value &container, const string &containerName, uint64_t increment);

#endif // CONFIGHELPERS_H
