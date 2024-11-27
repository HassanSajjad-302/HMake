
#ifndef BOOSTCPPTARGET_HPP
#define BOOSTCPPTARGET_HPP

#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"

enum class BoostExampleOrTestType : char
{
    RUNTEST,
    COMPILE,
    COMPILE_FAIL,
    LINK,
    LINK_FAIL,
    EXAMPLE,
};

struct ExampleOrTest
{
    DSC<CppSourceTarget> *dscTarget;
    BoostExampleOrTestType targetType;
};

struct BoostCppTarget : TargetCache
{
    Configuration *configuration = nullptr;
    DSC<CppSourceTarget> &mainTarget;
    vector<ExampleOrTest> examplesOrTests;

    BoostCppTarget(const pstring &name, Configuration *configuration_, bool headerOnly);
    ~BoostCppTarget();
    void addTestDirectory(const pstring &dir);
    /*void addTestDirectory(const pstring &dir, const pstring &prefix);
    void addExampleDirectory(const pstring &dir);*/
};

#endif // BOOSTCPPTARGET_HPP
