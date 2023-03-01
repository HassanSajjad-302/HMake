#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BasicTargets.hpp"
#include "BuildTools.hpp"
#include "Settings.hpp"
#include <list>
#include <string>

using std::string, std::list;

class Builder
{
    unsigned short round = 0;
    list<BTarget *>::iterator finalBTargetsIterator;
    size_t finalBTargetsSizeGoal = 0;

    list<BTarget *> &preSortBTargets;

  public:
    list<BTarget *> finalBTargets;
    explicit Builder(unsigned short roundBegin, unsigned short roundEnd, list<BTarget *> &preSortBTargets_);
    void populateFinalBTargets();

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void addNewBTargetInFinalBTargets(BTarget *bTarget);
    void updateBTargets();
};

#endif // HMAKE_BUILDER_HPP
