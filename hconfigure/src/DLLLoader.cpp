
#ifdef USE_HEADER_UNITS
import "DLLLoader.hpp";
import "fmt/format.h";
#else
#include "DLLLoader.hpp"
#include "fmt/format.h"
#endif

using fmt::print;

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
        print(stderr, "Could Not Load DLL {}", path);
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