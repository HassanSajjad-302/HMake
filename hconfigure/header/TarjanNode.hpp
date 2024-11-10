#ifndef HMAKE_TARJANNODE_HPP
#define HMAKE_TARJANNODE_HPP

#ifdef USE_HEADER_UNITS
import "Settings.hpp";
import "BuildSystemFunctions.hpp";
import "PlatformSpecific.hpp";
import <vector>;
#else
#include "BuildSystemFunctions.hpp"
#include "PlatformSpecific.hpp"
#include "Settings.hpp"
#include <vector>
#endif

using std::vector;

template <typename T> class TarjanNode
{
    // Following 4 are reset in findSCCS();
    inline static int index = 0;
    inline static vector<TarjanNode *> nodesStack;

    // Output
    inline static vector<T *> cycle;
    inline static bool cycleExists;

  public:
    inline static vector<T *> topologicalSort;

    // Input
    inline static vector<TarjanNode *> *tarjanNodes;

    mutable flat_hash_set<TarjanNode *> deps;

    explicit TarjanNode(const T *id_);

    // Find Strongly Connected Components
    static void findSCCS();

    static void checkForCycle();

    static void clearTarjanNodes();

    const T *const id;

  private:
    void strongConnect();
    int nodeIndex = 0;
    int lowLink = 0;
    bool initialized = false;
    bool onStack = false;
};

template <typename T> void TarjanNode<T>::clearTarjanNodes()
{
    for (TarjanNode *i : *tarjanNodes)
    {
        if (i)
        {
            i->nodeIndex = 0;
            i->lowLink = 0;
            i->initialized = false;
            i->onStack = false;
        }
    }
}
template <typename T> bool operator<(const TarjanNode<T> &lhs, const TarjanNode<T> &rhs);

template <typename T> TarjanNode<T>::TarjanNode(const T *const id_) : id{id_}
{
}

template <typename T> void TarjanNode<T>::findSCCS()
{
    index = 0;
    cycleExists = false;
    cycle.clear();
    nodesStack.clear();
    topologicalSort.clear();
    for (TarjanNode *tarjanNode : *tarjanNodes)
    {
        if (tarjanNode && !tarjanNode->initialized)
        {
            tarjanNode->strongConnect();
        }
    }
}

template <typename T> void TarjanNode<T>::strongConnect()
{
    initialized = true;
    nodeIndex = index;
    lowLink = index;
    ++index;
    nodesStack.emplace_back(this);
    onStack = true;

    for (TarjanNode *tarjandep : deps)
    {
        if (!tarjandep->initialized)
        {
            tarjandep->strongConnect();
            lowLink = std::min(lowLink, tarjandep->lowLink);
        }
        else if (tarjandep->onStack)
        {
            lowLink = std::min(lowLink, tarjandep->nodeIndex);
        }
    }

    if (lowLink == nodeIndex)
    {
        vector<TarjanNode *> tempCycle;
        while (true)
        {
            TarjanNode *tarjanTemp = nodesStack.back();
            nodesStack.pop_back();
            tarjanTemp->onStack = false;
            tempCycle.emplace_back(tarjanTemp);
            if (tarjanTemp->id == this->id)
            {
                break;
            }
        }
        if (tempCycle.size() > 1)
        {
            for (const TarjanNode *c : tempCycle)
            {
                cycle.emplace_back(const_cast<T *>(c->id));
            }
            cycleExists = true;
            return;
        }
    }
    topologicalSort.emplace_back(const_cast<T *>(id));
}

template <typename T> void TarjanNode<T>::checkForCycle()
{
    if (cycleExists)
    {
        printErrorMessageColor("There is a Cyclic-Dependency.\n", settings.pcSettings.toolErrorOutput);
        size_t cycleSize = cycle.size();
        for (unsigned int i = 0; i < cycleSize; ++i)
        {
            if (i == cycleSize - 1)
            {
                printErrorMessageColor(
                    fmt::format("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(), cycle[0]->getTarjanNodeName()),
                    settings.pcSettings.toolErrorOutput);
            }
            else
            {
                printErrorMessageColor(fmt::format("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(),
                                                   cycle[i + 1]->getTarjanNodeName()),
                                       settings.pcSettings.toolErrorOutput);
            }
        }
        exit(EXIT_FAILURE);
    }
}

template <typename T> bool operator<(const TarjanNode<T> &lhs, const TarjanNode<T> &rhs)
{
    return lhs.id < rhs.id;
}

#endif // HMAKE_TARJANNODE_HPP
