#include <cassert>
#include <cstdint>
#include <type_traits>

template <typename PtrType, unsigned IntBits, typename IntType = unsigned> class PointerIntPair
{
    static_assert(std::is_pointer_v<PtrType>, "PtrType must be a pointer type");
    static_assert(IntBits <= 3, "IntBits must be <= 3");
    uintptr_t m_value = 0;

    static constexpr uintptr_t IntMask = (uintptr_t(1) << IntBits) - 1;
    static constexpr uintptr_t PtrMask = ~IntMask;

  public:
    PointerIntPair() = default;
    explicit PointerIntPair(PtrType ptr, IntType intVal = 0)
    {
        set(ptr, intVal);
    }

    PtrType getPointer() const
    {
        return reinterpret_cast<PtrType>(m_value & PtrMask);
    }

    IntType getInt() const
    {
        return static_cast<IntType>(m_value & IntMask);
    }

    void setPointer(PtrType ptr)
    {
        uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
        assert((ptrVal & IntMask) == 0 && "Pointer not sufficiently aligned");
        m_value = (ptrVal) | (m_value & IntMask);
    }

    void setInt(IntType intVal)
    {
        assert(static_cast<uintptr_t>(intVal) < (uintptr_t(1) << IntBits) && "Integer value too large");
        m_value = (m_value & PtrMask) | static_cast<uintptr_t>(intVal);
    }

    void set(PtrType ptr, IntType intVal)
    {
        uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
        assert((ptrVal & IntMask) == 0 && "Pointer not sufficiently aligned");
        assert(static_cast<uintptr_t>(intVal) < (uintptr_t(1) << IntBits) && "Integer value too large");
        m_value = ptrVal | static_cast<uintptr_t>(intVal);
    }

    uintptr_t getRaw() const
    {
        return m_value;
    }

    static PointerIntPair fromRaw(uintptr_t raw)
    {
        PointerIntPair result;
        result.m_value = raw;
        return result;
    }

    PtrType operator->() const
    {
        return getPointer();
    }
    explicit operator bool() const
    {
        return getPointer() != nullptr;
    }
};