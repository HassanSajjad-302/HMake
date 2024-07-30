#include "Configure.hpp"

void buildSpecification()
{
    // DSC constructor has this line         prebuiltBasic->objectFileProducers.emplace(objectFileProducer);
    // which specifies std CppSourceTarget (and transitively its object-files) as a dependency of std
    // LinkOrArchiveTarget.
    DSC<CppSourceTarget> &std = getCppObjectDSC("std");
    std.getSourceTarget().interfaceFiles("std/std.cpp").assign(CxxSTD::V_20);

    DSC<CppSourceTarget> &cat = getCppObjectDSC("cat");
    cat.getSourceTarget()
        .interfaceFiles("cat/cat.ixx")
        .moduleFiles("cat/cat.cpp")
        .assign(CxxSTD::V_20)
        .publicHUIncludes("cat");

    cat.privateLibraries(&std);

    DSC<CppSourceTarget> &app = getCppExeDSC("app");
    app.getSourceTarget().moduleFiles("main.cpp");

    // This will define two new CppSourceTargets. saveAndReplace will save the older CppSourceTargets in the
    // DSC<CppSourceTarget> while replacing it with these newer ones to be used in privateLibraries function. This new
    // target is compiled with the default value /std:c++latest same as the main.cpp. saveAndReplace will also populate
    // the module files of these newer targets with similar values to the older targets. assignObjectFileProducerDeps
    // is used instead of privateLibraries because besides adding std CppSourceTarget as a dependency of cat
    // CppSourceTarget, privateLibraries also adds std LinkOrArchiveTarget as a dependency of cat LinkOrArchiveTarget
    // which has already been done. Please notice that the older CppSourceTargets that we are replacing in the
    // following, we had already specified them (and transitively their object files) as the dependency of the
    // respective LinkOrArchiveTargets in the DSC constructor. The newer, following declared, CppSourceTargets are not
    // specified as a dependency of any LinkOrArchiveTaget, hence the object files of these are wasted but the ifc files
    // are used while compiling their dependents.

    // If the following 4 lines are commented out, two warnings of incompatible ifcs  are printed as
    // there is bmi incompatibility introduced because of differences in language versions.
    CppSourceTarget &std1 = getCppObject("std1-cpp");
    CppSourceTarget &cat1 = getCppObject("cat1-cpp");
    std.saveAndReplace(&std1);
    cat.saveAndReplace(&cat1);

    app.privateLibraries(&cat, &std);

    // Please notice that saveAndReplace() function stores the older pointer in that respective DSC and populates the
    // module files of the newer target with only the interface-file of the  older target. This means that cat.ixx and
    // cat.hpp will be compiled in cat1-cpp but not cat.cpp. saveAndReplace also copies other properties such as
    // compiler-definitions and include-directories. It, however, does not copy the compiler-flags. It also does not
    // copy the build-system features such as CxxStd. The reason is that such feature values can result to bmi sensitive
    // values to the compile-command.
}

MAIN_FUNCTION
