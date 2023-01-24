#ifndef HMAKE_VARIANT_HPP
#define HMAKE_VARIANT_HPP

#include "BasicTargets.hpp"
#include "Features.hpp"
#include <memory>

using std::shared_ptr;

class Executable;
struct Library;

class Variant : public Features, public CTarget
{
    friend class Target;
    vector<class CppSourceTarget *> moduleTargets;
    vector<shared_ptr<CTarget>> targetsContainer;

    struct TargetComparator
    {
        bool operator()(const class CppSourceTarget *lhs, const class CppSourceTarget *rhs) const;
    };

  public:
    set<class PLibrary *> preBuiltLibraries;
    Variant(const string &name_, bool initializeFromCache = true);
    void copyAllTargetsFromOtherVariant(Variant &variantFrom);
    void setJson() override;
    Executable &findExecutable(const string &exeName);
    Library &findLibrary(const string &libName);
    Executable &addExecutable(const string &exeName);
    Library &addLibrary(const string &libName);
    // TODO: Variant should also have functions like Target which modify the property and return the Target&.
    // These functions will return the Variant&.
};

#endif // HMAKE_VARIANT_HPP
