
#ifndef HMAKE_ZDLLLOADER_HPP
#define HMAKE_ZDLLLOADER_HPP

#ifdef _WIN32

#ifdef USE_HEADER_UNITS
import <Windows.h>;
import "PlatformSpecific.hpp";
#else
#include "PlatformSpecific.hpp"
#include <Windows.h>
#endif

#else

#ifdef USE_HEADER_UNITS
import "PlatformSpecific.hpp";
import <dlfcn.h>;

#else

#include "PlatformSpecific.hpp"
#include <dlfcn.h>
#endif

#endif

// Header-File is named as zDLLLoader.h so that it is at bottom of includes because otherwise Window.h cause symbol
// pollution in other header-files. What could be a better fix.
struct DLLLoader
{
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif

    DLLLoader(const char *path);
    ~DLLLoader();
    template <typename T> T getSymbol(const pstring &name);
};

template <typename T> T DLLLoader::getSymbol(const pstring &name)
{
    T symbol = nullptr;
    if (handle)
    {
#ifdef _WIN32
        symbol = reinterpret_cast<T>(GetProcAddress(handle, name.c_str()));
#else
        symbol = reinterpret_cast<T>(dlsym(handle, name.c_str()));
#endif
    }
    return symbol;
}

#endif // HMAKE_ZDLLLOADER_HPP
