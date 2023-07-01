#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import <vector>;
import <list>;
#else
#include "CppSourceTarget.hpp"
#include <list>
#include <vector>
#endif

using std::vector, std::list;

enum class BuilderMode : char
{
    PRE_SORT,
    UPDATE_BTARGET,
};

class Builder
{
  public:
    list<BTarget *> updateBTargets;
    size_t updateBTargetsSizeGoal = 0;

  private:
    list<BTarget *>::iterator updateBTargetsIterator;
    vector<BTarget *>::iterator preSortBTargetsIterator;
    unsigned short threadCount = 0;
    unsigned short numberOfLaunchedThreads = 0;
    unsigned short round = 0;

  public:
    BuilderMode builderMode = BuilderMode::PRE_SORT;
    bool updateBTargetFailed = false;

  private:
    bool shouldExitAfterRoundMode = false;
    bool errorHappenedInRoundMode = false;

  public:
    explicit Builder();

    void addNewBTargetInFinalBTargets(BTarget *bTarget);
    void execute();

    bool addCppSourceTargetsInFinalBTargets(set<CppSourceTarget *> &targets);
};

#endif // HMAKE_BUILDER_HPP
