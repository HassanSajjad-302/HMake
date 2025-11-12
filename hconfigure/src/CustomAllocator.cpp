#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

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
