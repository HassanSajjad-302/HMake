#ifndef HMAKE_C_API_HPP
#define HMAKE_C_API_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp"
#else
#include "BuildSystemFunctions.hpp"
#endif

    // All C Types are prefixed by C_
    typedef enum { C_CONFIGURE_TARGET_TYPE, C_C_TARGET_TYPE, C_CPP_TARGET_TYPE, C_RUST_TARGET_TYPE, C_LOA_TARGET_TYPE, // LOA LinkOrArchive
                   C_CONFIGURATION_TARGET_TYPE, } C_TargetType;

extern "C"
{
    typedef struct
    {
        C_TargetType type;
        void *object;
    } C_Target;

    typedef struct
    {
        unsigned short size;
        C_Target **c_cTargets;
    } C_TargetContainer;

    // struct for CTarget
    typedef struct
    {
        const char *dir;
    } C_CTarget;

    // struct for CppSourceTarget
    typedef struct
    {
        C_CTarget *parent;

        const char **sourceFiles;
        unsigned short sourceFilesCount;

        const char **moduleFiles;
        unsigned short moduleFilesCount;

        const char **includeDirs;
        unsigned short includeDirsCount;

        const char *compileCommand;
        const char *compilerPath;

    } C_CppSourceTarget;

    typedef struct
    {
        C_CTarget *parent;
    } C_Configuration;

    typedef struct
    {
        C_CTarget *parent;
    } C_LinkOrArchiveTarget;
}

// Here API will be designed to facilitate hmake integration with ide and editors. After loading the
// configure.dll/libconfigure.so of the project, the ide/editor will call the func2 function with 2 which is
// BSMode::IDE. This mode is similar to the BSMode::CONFIGURE except the JSON files are not written on the disk. After
// calling func2() the editor/ide can call the following functions to get the relevant information.

// Such API is in C for easier ABI support. It would be LTS stable and versioned. So, the rest of the build system can
// change freely. The most important point is that except func2 and the following API, the rest of the build-system can
// be in C, C++ or Rust but will integrate with editor/ide the same.

// TODO
//  The following information should be sufficient, but if not than I am confident
//  that HMake can do wonderful stuff. NEED to study ides, editor, linters like e.g. emacs and what
//  extensions are used in emacs to provide complete ide experience for C++ development and how HMake can
// integrate with these extensions better.

extern "C" EXPORT C_TargetContainer *getCTargetContainer(BSMode bsModeLocal);
#endif // HMAKE_C_API_HPP
