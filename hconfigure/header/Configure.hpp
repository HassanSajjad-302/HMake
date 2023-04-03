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
import "SizeDifference.hpp";
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
#include "Cache.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "Features.hpp"
#include "GetTarget.hpp"
#include "JConsts.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include "SizeDifference.hpp"
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

extern "C" EXPORT int func2(BSMode bsMode_);

// If this functions is not called then the unused symbols are not exported and above globals are not initialized.
void exportAllSymbolsAndInitializeGlobals();

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
