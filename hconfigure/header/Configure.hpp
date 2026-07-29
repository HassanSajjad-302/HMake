#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "ConfigurationAssign.hpp"
#include "CppMod.hpp"
#include "CppTarget.hpp"
#include "ue.hpp"
#include "DSC.hpp"
#include "Features.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include <filesystem>
#include <memory>
#include <stack>
#include <thread>
#include <utility>

/// User-defined function that declares the build graph.
void buildSpecification();
/// User-defined function that customizes one configuration.
void configurationSpecification(Configuration &config);
/// Invokes the registered `configurationSpecification` callback.
void callConfigurationSpecification();

/// Shared entry point used by generated configure and build executables.
int main2(int argc, char **argv);

inline void (*buildSpecificationFuncPtr)();
inline void (*configurationSpecificationFuncPtr)(Configuration &config);

/// Defines `main()` and registers `buildSpecification`. Place once in an `hmake.cpp` file.
#define MAIN_FUNCTION                                                                                                  \
    int main(int argc, char **argv)                                                                                    \
    {                                                                                                                  \
        buildSpecificationFuncPtr = &buildSpecification;                                                               \
        return main2(argc, argv);                                                                                      \
    }

/// Registers and immediately invokes `configurationSpecification`.
#define CALL_CONFIGURATION_SPECIFICATION                                                                               \
    configurationSpecificationFuncPtr = &configurationSpecification;                                                   \
    callConfigurationSpecification();

#endif // HMAKE_CONFIGURE_HPP
