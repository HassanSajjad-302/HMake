#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP
#ifdef USE_HEADER_UNITS
import <vector>;
import <string>;
#else
#include <string>
#include <vector>
#endif

using std::string, std::vector;

class Builder
{
    unsigned short round = 0;
    vector<struct BTarget *>::iterator finalBTargetsIterator;
    size_t finalBTargetsSizeGoal = 0;

  public:
    vector<BTarget *> finalBTargets;
    bool updateBTargetFailed = false;
    explicit Builder(unsigned short roundBegin, unsigned short roundEnd, vector<BTarget *> &preSortBTargets);
    void populateFinalBTargets();

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void addNewBTargetInFinalBTargets(BTarget *bTarget);
    void updateBTargets();
};

#endif // HMAKE_BUILDER_HPP

