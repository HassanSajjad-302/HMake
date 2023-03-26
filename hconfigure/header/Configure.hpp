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
import "TarjanNode.hpp";
import "ToolsCache.hpp";
import "Utilities.hpp";
import "fmt/color.h";
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
#include "TarjanNode.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "fmt/color.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <memory>
#include <set>
#include <stack>
#include <thread>
#include <utility>
#endif

extern "C" EXPORT int func2(BSMode bsMode_);
inline string *errorMessageString = nullptr;
extern "C" EXPORT const char *errorMessageStrPtr;

// If this functions is not called then the unused symbols are not exported and above globals are not initialized.
void exportAllSymbolsAndInitializeGlobals();

// Clear the error message string pointer
extern "C" EXPORT void clearError();

// Executes the function in try-catch block and also sets the errorMessageStrPtr equal to the exception what message
// string
template <typename T> int executeInTryCatchAndSetErrorMessagePtr(std::function<T> funcLocal)
{
#ifdef EXE
    funcLocal();
    return EXIT_SUCCESS;
#else
    try
    {
        funcLocal();
    }
    catch (std::exception &ec)
    {
        clearError();
        errorMessageString = new string(ec.what());
        errorMessageStrPtr = errorMessageString->c_str();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
#endif
}

#endif // HMAKE_CONFIGURE_HPP
