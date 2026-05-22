#ifndef HMAKE_DSDD_HPP
#define HMAKE_DSDD_HPP

#include "BTarget.hpp"
#include "CppTarget.hpp"

struct BuildCache
{
    // BTargetType::CPP_SRC
    // command-hash and launch-time are automatically stored for this.
    struct SourceFile
    {
        vector<Node *> headerFiles;
    };

    // BTargetType::CPP_MOD
    struct ModuleFile
    {
        bool headerStatusChanged;
        SourceFile srcFile;
        vector<uint32_t> depsArray;
    };

    // For LOAT, we don't store any-cache as we only needed commandHash and launchTime which are already stored

    struct Configuration
    {
    };

    // BTargetType::CPP_TARGET
    struct CppTarget
    {
    };
};

#endif // HMAKE_DSDD_HPP
