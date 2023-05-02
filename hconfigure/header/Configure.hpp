#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "BuildSystemFunctions.hpp";
import "BuildTools.hpp";
import "Builder.hpp";
import "Cache.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "Features.hpp";
import "GetTarget.hpp";
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
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "CTargetRoundZeroBTarget.hpp"
#include "Cache.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "Features.hpp"
#include "GetTarget.hpp"
#include "JConsts.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include "TarjanNode.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <memory>
#include <set>
#include <stack>
#include <thread>
#include <utility>
#endif

// TODO
// HMake in future will only be available as module. Hence configuration will-be a split-second process.

// TODO
// HMake is designed to have all the build specification in a single-file. This might not be sufficient for really-big
// projects. An approach to have a tugboat hmake.cpp that builds the cargo configure.dll, which then builds the project
// will be explored.

// TODO
//  configure.dll on linux is compiled with -fsanitizer=thread but no sanitizer is used on Windows. Choice for
//  using sanitizer will be optional.

// TODO
// Instead of target-cache file per target, a central JSON for target caches should be used.

extern "C" EXPORT int func2(BSMode bsMode_);

// Executes the function in try-catch block and also sets the errorMessageStrPtr equal to the exception what message
// string
template <typename T> int executeInTryCatchAndSetErrorMessagePtr(std::function<T> funcLocal)
{
    try
    {
        funcLocal();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#endif // HMAKE_CONFIGURE_HPP
