#include "Configure.hpp"

void exportAllSymbolsAndInitializeGlobals()
{
    decltype(func2) f;
    decltype(clearError) j;
    errorMessageStrPtr = nullptr;
}

extern "C" EXPORT void clearError()
{
    delete errorMessageStrPtr;
    delete errorMessageString;
}