#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "BuildSystemFunctions.hpp";
import "BuildTools.hpp";
import "Builder.hpp";
import "Cache.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "DSC.hpp";
import "Features.hpp";
import "JConsts.hpp";
import "SMFile.hpp";
import "Settings.hpp";
import "CTargetRoundZeroBTarget.hpp";
import "TarjanNode.hpp";
import "ToolsCache.hpp";
import "Utilities.hpp";
#include "nlohmann/json.hpp";
import <filesystem>;
import <memory>;
import <set>;
import <stack>;
import <thread>;
import <utility>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "CTargetRoundZeroBTarget.hpp"
#include "Cache.hpp"
#include "ConfigurationAssign.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "Features.hpp"
#include "JConsts.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <memory>
#include <stack>
#include <thread>
#include <utility>
#endif

// Will return true if all configurations are built
bool selectiveConfigurationSpecification(void (*ptr)(Configuration &configuration));

// TODO
// HMake in future will only be available as module. Hence configuration will-be a split-second process.

// TODO
// HMake is designed to have all the build specification in a single-file. This might not be sufficient for really-big
// projects. An approach to have a tugboat hmake.cpp that builds the cargo configure.dll, which then builds the project
// will be explored.

// TODO
// Thread Sanitizer is used on Linux but not Sanitizer is used on Windows. There should be an option to use sanitizer on
// Windows as well.

void buildSpecification();
void configurationSpecification(Configuration &config);
void callConfigurationSpecification();

int main2(int argc, char **argv);

inline void (*buildSpecificationFuncPtr)();
inline void (*configurationSpecificationFuncPtr)(Configuration &config);

#define MAIN_FUNCTION                                                                                                  \
    int main(int argc, char **argv)                                                                                    \
    {                                                                                                                  \
        buildSpecificationFuncPtr = &buildSpecification;                                                               \
        return main2(argc, argv);                                                                                      \
    }

#define CALL_CONFIGURATION_SPECIFICATION                                                                               \
    configurationSpecificationFuncPtr = &configurationSpecification;                                                   \
    callConfigurationSpecification();

#endif // HMAKE_CONFIGURE_HPP
