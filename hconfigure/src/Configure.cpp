#include "Configure.hpp"
#include <cstdint>
#include <cstdlib>
#include <new>
using std::filesystem::current_path;

bool selectiveConfigurationSpecification(void (*ptr)(Configuration &configuration))
{
    if (equivalent(path(configureNode->filePath), current_path()))
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
        return true;
    }
    for (const Configuration &configuration : targets<Configuration>)
    {
        if (configuration.isHBuildInSameOrChildDirectory())
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
    }
    return false;
}

static void parseCmdArgumentsAndSetConfigureNode(const int argc, char **argv)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (argc > 1)
        {
            printErrorMessage("Unknown cmd arguments in configure mode\n");
            for (int i = 1; i < argc; ++i)
            {
                printErrorMessage(argv[i]);
            }
        }
    }

    string configurePathString;
    if constexpr (bsMode != BSMode::CONFIGURE)
    {
        path cacheJsonPath;
        bool cacheJsonExists = false;
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            cacheJsonPath = p / "cache.json";
            if (exists(cacheJsonPath))
            {
                cacheJsonExists = true;
                break;
            }
        }

        if (cacheJsonExists)
        {
            configurePathString = cacheJsonPath.parent_path().string();
        }
        else
        {
            printErrorMessage("cache.json could not be found in current dir and dirs above\n");
        }
    }
    else
    {
        configurePathString = current_path().string();
    }

    lowerCaseOnWindows(configurePathString.data(), configurePathString.size());
    configureNode = Node::getNodeFromNormalizedString(configurePathString, false);

    if constexpr (bsMode == BSMode::BUILD)
    {
        for (int i = 1; i < argc; ++i)
        {
            string targetArgFullPath = (current_path() / argv[i]).lexically_normal().string();
            lowerCaseOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
            if (targetArgFullPath.size() <= configureNode->filePath.size())
            {
                printErrorMessage(FORMAT("Invalid Command-Line Argument {}\n", argv[i]));
            }
            if (targetArgFullPath.ends_with(slashc))
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end() - 1);
            }
            else
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end());
            }
        }
    }
}

void callConfigurationSpecification()
{
    for (Configuration &config : targets<Configuration>)
    {
        if (config.isHBuildInSameOrChildDirectory() || configureNode == currentNode)
        {
            config.initialize();
            (*configurationSpecificationFuncPtr)(config);
            config.postConfigurationSpecification();
        }
    }
}

int main2(const int argc, char **argv)
{
    parseCmdArgumentsAndSetConfigureNode(argc, argv);
    constructGlobals();
    initializeCache(bsMode);
    (*buildSpecificationFuncPtr)();
    isOneThreadRunning = true;
    const bool errorHappened = configureOrBuild();
    destructGlobals();
    fflush(stdout);
    fflush(stderr);
#ifdef NDEBUG
    if (errorHappened)
    {
        std::_Exit(EXIT_FAILURE);
    }
    std::_Exit(EXIT_SUCCESS);
#else
    if (errorHappened)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
#endif
}

struct Memory
{
    char *p = nullptr;
    uint64_t memSize = 0;
    uint64_t memUsed = 0;
};

Memory &getMemory()
{
    thread_local Memory m;
    return m;
}

inline void *allocate(const uint64_t allocationSize, const std::size_t alignment = alignof(std::max_align_t))
{
    auto &[p, memSize, memUsed] = getMemory();

    // Calculate padding needed to align current position
    char *currentPos = p + memUsed;
    auto addr = reinterpret_cast<std::uintptr_t>(currentPos);
    uint64_t misalignment = addr % alignment;
    uint64_t padding = misalignment == 0 ? 0 : alignment - misalignment;

    const uint64_t totalNeeded = padding + allocationSize;

    void *q;

    if (totalNeeded > 2 * 1024 * 1024)
    {
        memUsed = 0;
        memSize = totalNeeded;
        p = static_cast<char *>(std::malloc(memSize)); // use large page.

        // Recalculate padding for new block
        addr = reinterpret_cast<std::uintptr_t>(p);
        misalignment = addr % alignment;
        padding = misalignment == 0 ? 0 : alignment - misalignment;

        q = p + padding;
        memUsed = padding + allocationSize;
        return q;
    }

    if (memSize < memUsed + totalNeeded)
    {
        memUsed = 0;
        memSize = 2 * 1024 * 1024;
        p = static_cast<char *>(std::malloc(memSize));

        // Recalculate padding for new block
        addr = reinterpret_cast<std::uintptr_t>(p);
        misalignment = addr % alignment;
        padding = misalignment == 0 ? 0 : alignment - misalignment;

        q = p + padding;
        memUsed = padding + allocationSize;
        return q;
    }

    q = currentPos + padding;
    memUsed += totalNeeded;
    return q;
}

// ============================================================================
// C++ Operator Overrides
// ============================================================================

// Basic new operator
void *operator new(std::size_t size)
{
    if (size == 0)
        size = 1;

    void *ptr = allocate(size);
    return ptr;
}

// Basic delete operator (no-op since we don't free)
void operator delete(void *ptr) noexcept
{
    (void)ptr;
}

// Array new operator
void *operator new[](std::size_t size)
{
    if (size == 0)
        size = 1;

    void *ptr = allocate(size);
    return ptr;
}

// Array delete operator (no-op)
void operator delete[](void *ptr) noexcept
{
    (void)ptr;
}

// C++14: Sized delete
void operator delete(void *ptr, std::size_t size) noexcept
{
    (void)ptr;
    (void)size;
}

void operator delete[](void *ptr, std::size_t size) noexcept
{
    (void)ptr;
    (void)size;
}

// C++17: Aligned new/delete
void *operator new(std::size_t size, std::align_val_t alignment)
{
    if (size == 0)
        size = 1;

    void *ptr = allocate(size, static_cast<std::size_t>(alignment));
    return ptr;
}

void operator delete(void *ptr, std::align_val_t alignment) noexcept
{
    (void)ptr;
    (void)alignment;
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
    if (size == 0)
        size = 1;

    void *ptr = allocate(size, static_cast<std::size_t>(alignment));
    return ptr;
}

void operator delete[](void *ptr, std::align_val_t alignment) noexcept
{
    (void)ptr;
    (void)alignment;
}

void operator delete(void *ptr, std::size_t size, std::align_val_t alignment) noexcept
{
    (void)ptr;
    (void)size;
    (void)alignment;
}

void operator delete[](void *ptr, std::size_t size, std::align_val_t alignment) noexcept
{
    (void)ptr;
    (void)size;
    (void)alignment;
}

// Nothrow versions
void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    if (size == 0)
        size = 1;
    return allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    if (size == 0)
        size = 1;
    return allocate(size);
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    (void)ptr;
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    (void)ptr;
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    if (size == 0)
        size = 1;
    return allocate(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    if (size == 0)
        size = 1;
    return allocate(size, static_cast<std::size_t>(alignment));
}

void operator delete(void *ptr, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    (void)ptr;
    (void)alignment;
}

void operator delete[](void *ptr, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    (void)ptr;
    (void)alignment;
}
