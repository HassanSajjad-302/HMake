#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP
#ifdef USE_HEADER_UNITS
import <vector>;
import <string>;
import <list>;
#else
#include <list>
#include <string>
#include <vector>
#endif

using std::string, std::vector, std::list;

class Builder
{
    unsigned short round = 0;
    list<struct BTarget *>::iterator finalBTargetsIterator;
    size_t finalBTargetsSizeGoal = 0;

  public:
    list<BTarget *> finalBTargets;
    bool updateBTargetFailed = false;
    explicit Builder(unsigned short roundBegin, unsigned short roundEnd, vector<BTarget *> &preSortBTargets);
    void populateFinalBTargets();

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void addNewBTargetInFinalBTargets(BTarget *bTarget);
    void updateBTargets();
};

#endif // HMAKE_BUILDER_HPP

