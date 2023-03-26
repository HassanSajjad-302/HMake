
#ifndef HMAKE_DLLLOADER_HPP
#define HMAKE_DLLLOADER_HPP

#ifdef _WIN32
#ifdef USE_HEADER_UNITS
import "Windows.h";
import <string>;
#else
#include "Windows.h"
#include "string"
#endif
#else
#include "string"
#include <dlfcn.h>
#endif

using std::string;

struct DLLLoader
{
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif

    DLLLoader(const char *path);
    ~DLLLoader();
    template <typename T> T getSymbol(const string &name);
};

template <typename T> T DLLLoader::getSymbol(const string &name)
{
    T symbol = nullptr;
    if (handle)
    {
#ifdef _WIN32
        symbol = reinterpret_cast<T>(GetProcAddress(handle, name.c_str()));
#else
        symbol = reinterpret_cast<T>(dlsym(handle_, name.c_str()));
#endif
    }
    return symbol;
}

#endif // HMAKE_DLLLOADER_HPP
