#include <cassert>
#include <cstdint>
#include <type_traits>

// Traits to determine pointer alignment and available bits
template <typename T> struct PointerTraits
{
    static constexpr size_t Alignment = alignof(T);
    static constexpr unsigned NumLowBitsAvailable = Alignment >= 8 ? 3 : Alignment >= 4 ? 2 : Alignment >= 2 ? 1 : 0;
};

// Generic PointerIntPair using lower bits
template <typename PtrType, unsigned IntBits, typename IntType = unsigned> class PointerIntPair
{
    static_assert(std::is_pointer_v<PtrType>, "PtrType must be a pointer type");
    static_assert(IntBits <= PointerTraits<std::remove_pointer_t<PtrType>>::NumLowBitsAvailable,
                  "Not enough low bits available for IntBits");

    using PointeeType = std::remove_pointer_t<PtrType>;

    uintptr_t m_value = 0;

    static constexpr uintptr_t IntMask = (uintptr_t(1) << IntBits) - 1;
    static constexpr uintptr_t PtrMask = ~IntMask;

  public:
    PointerIntPair() = default;

    explicit PointerIntPair(PtrType ptr, IntType intVal = 0)
    {
        set(ptr, intVal);
    }

    // Get the pointer (with tag bits cleared)
    PtrType getPointer() const
    {
        return reinterpret_cast<PtrType>(m_value & PtrMask);
    }

    // Get the integer value from tag bits
    IntType getInt() const
    {
        return static_cast<IntType>(m_value & IntMask);
    }

    // Set pointer only
    void setPointer(PtrType ptr)
    {
        uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
        assert((ptrVal & IntMask) == 0 && "Pointer not sufficiently aligned");
        m_value = ptrVal | m_value & IntMask;
    }

    // Set integer only
    void setInt(IntType intVal)
    {
        assert(intVal < (uintptr_t(1) << IntBits) && "Integer value too large");
        m_value = m_value & PtrMask | static_cast<uintptr_t>(intVal);
    }

    // Set both atomically
    void set(PtrType ptr, IntType intVal)
    {
        uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
        assert((ptrVal & IntMask) == 0 && "Pointer not sufficiently aligned");
        assert(intVal < (uintptr_t(1) << IntBits) && "Integer value too large");
        m_value = ptrVal | static_cast<uintptr_t>(intVal);
    }

    // Get the raw combined value (for passing to IOCP, etc.)
    uintptr_t getRaw() const
    {
        return m_value;
    }

    // Reconstruct from raw value
    static PointerIntPair fromRaw(uintptr_t raw)
    {
        PointerIntPair result;
        result.m_value = raw;
        return result;
    }

    // Convenience operators
    PtrType operator->() const
    {
        return getPointer();
    }
    explicit operator bool() const
    {
        return getPointer() != nullptr;
    }
};

// Higher-level wrapper for IOCP specifically
template <typename T, unsigned EventBits = 3> class IOCompletionKey
{
    PointerIntPair<T *, EventBits> m_pair;

  public:
    IOCompletionKey() = default;
    IOCompletionKey(T *context, unsigned event) : m_pair(context, event)
    {
    }

    T *getContext() const
    {
        return m_pair.getPointer();
    }
    unsigned getEvent() const
    {
        return m_pair.getInt();
    }

    uintptr_t encode() const
    {
        return m_pair.getRaw();
    }
    static IOCompletionKey decode(uintptr_t raw)
    {
        IOCompletionKey key;
        key.m_pair = PointerIntPair<T *, EventBits>::fromRaw(raw);
        return key;
    }

    // Call the handler with full context
    template <typename... Args> void dispatch(Args &&...args) const
    {
        if (T *context = getContext())
        {
            context->onCompletion(getEvent(), std::forward<Args>(args)...);
        }
    }
};