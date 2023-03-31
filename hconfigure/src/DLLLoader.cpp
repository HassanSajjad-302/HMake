
#ifdef USE_HEADER_UNITS
import "zDLLLoader.hpp";
import "BuildSystemFunctions.hpp";
import "fmt/format.h";
#else
#include "BuildSystemFunctions.hpp"
#include "fmt/format.h"
#include "zDLLLoader.hpp"
#endif

using fmt::format;

DLLLoader::DLLLoader(const char *path)
{
#ifdef _WIN32
    // Load the DLL on Windows using the LoadLibrary function
    handle = LoadLibrary(path);
#else
    // Load the shared library on Unix-like systems using dlopen
    handle = dlopen(path, RTLD_LAZY);
#endif
    if (!handle)
    {
        // Handle the error if the library fails to load
        printErrorMessage(format("Could Not Load DLL {}", path));
    }
}

DLLLoader::~DLLLoader()
{
#ifdef _WIN32
    // Free the library handle on Windows using FreeLibrary
    FreeLibrary(handle);
#else
    // Free the library handle on Unix-like systems using dlclose
    dlclose(handle);
#endif
}