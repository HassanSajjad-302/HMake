
#ifndef HMAKE_POINTERARRAYLIST_HPP
#define HMAKE_POINTERARRAYLIST_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

/**
 * @brief Non-owning, index-linked queue optimized for scheduler front insertion.
 *
 * The logical queue is a singly linked list whose links are integer slot indices into
 * an append-only array. Every insertion consumes a new physical slot. Consumed and
 * tombstoned slots are reclaimed together by `clear()` rather than individually.
 *
 * This lets the module scheduler remember an insertion index, tombstone that entry,
 * and insert the same target at the front without searching the queue.
 *
 * `reserve()` and automatic growth preserve numeric slot indices and stored `T*`
 * values. They invalidate raw pointers/references into `storage` or `array`. `clear()`
 * retains capacity but semantically invalidates every previously returned index.
 *
 * The container does not own the pointed-to `T` objects and is not thread-safe.
 */
template <typename T> class PointerArrayList
{
    struct ArrayListItem
    {
        T *value;
        uint32_t next;
    };

    static constexpr uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();
    static constexpr uint32_t initialCapacity = 1024;

    uint32_t capacity_ = 0;

    void ensureCapacity()
    {
        if (arraySize < capacity_)
        {
            return;
        }

        if (arraySize == invalidIndex)
        {
            // The queue uses uint32_t links, so it cannot represent another entry.
            std::abort();
        }

        const uint64_t doubled = static_cast<uint64_t>(capacity_) * 2;
        reserve(static_cast<std::size_t>(std::min<uint64_t>(doubled, static_cast<uint64_t>(invalidIndex))));
    }

  public:
    // Exposed for the scheduler's indexed tombstoning. Do not retain a pointer/reference across reserve or insertion.
    ArrayListItem *storage = nullptr;
    ArrayListItem *array = nullptr;
    /// Index of the next logical queue entry, or `invalidIndex` when exhausted.
    uint32_t currentIndex = invalidIndex;
    /// Last linked queue entry, or `invalidIndex` when empty.
    uint32_t last = invalidIndex;
    /// Physical slots used since `clear()`; this is not the number of live or allocated entries.
    uint32_t arraySize = 0;

    /// Empties the queue, retains its allocation, and invalidates all previously returned slot indices.
    void clear()
    {
        array = storage;
        currentIndex = invalidIndex;
        last = invalidIndex;
        arraySize = 0;
    }

    /// Returns physical slots used since the last `clear()`, including tombstones and consumed entries.
    uint32_t size() const
    {
        return arraySize;
    }

    /// Returns the number of physical slots available without reallocation.
    uint32_t capacity() const noexcept
    {
        return capacity_;
    }

    /**
     * @brief Ensures space for at least `requestedCapacity` physical slots.
     *
     * Numeric links and insertion indices remain valid. Raw pointers/references into
     * `storage` or `array` are invalidated if growth occurs.
     */
    void reserve(const std::size_t requestedCapacity)
    {
        if (requestedCapacity <= capacity_)
        {
            return;
        }
        if (requestedCapacity > invalidIndex)
        {
            std::abort();
        }

        const auto newCapacity = static_cast<uint32_t>(requestedCapacity);
        ArrayListItem *newStorage = new ArrayListItem[newCapacity];
        if (arraySize)
        {
            std::copy_n(storage, arraySize, newStorage);
        }
        delete[] storage;
        storage = newStorage;
        array = storage;
        capacity_ = newCapacity;
    }

    PointerArrayList()
    {
        reserve(initialCapacity);
    }

    ~PointerArrayList()
    {
        delete[] storage;
    }

    PointerArrayList(const PointerArrayList &) = delete;
    PointerArrayList &operator=(const PointerArrayList &) = delete;
    PointerArrayList(PointerArrayList &&) = delete;
    PointerArrayList &operator=(PointerArrayList &&) = delete;

    /// Appends `bTarget` to the logical queue.
    void emplace_back(T *bTarget)
    {
        ensureCapacity();
        array[arraySize].value = bTarget;
        array[arraySize].next = invalidIndex;
        if (currentIndex == invalidIndex)
        {
            currentIndex = arraySize;
        }
        else
        {
            array[last].next = arraySize;
        }
        last = arraySize;
        ++arraySize;
    }

    /// Inserts `bTarget` at the front of the logical queue.
    void emplace_front(T *bTarget)
    {
        ensureCapacity();
        array[arraySize].value = bTarget;
        array[arraySize].next = currentIndex;
        if (currentIndex == invalidIndex)
        {
            last = arraySize;
        }
        currentIndex = arraySize;
        ++arraySize;
    }

    /// Inserts at the front and returns the physical slot index used for later tombstoning.
    void emplace(T *bTarget, uint32_t &insertionIndex)
    {
        ensureCapacity();
        insertionIndex = arraySize;
        array[arraySize].value = bTarget;
        array[arraySize].next = currentIndex;
        if (currentIndex == invalidIndex)
        {
            last = arraySize;
        }
        currentIndex = arraySize;
        ++arraySize;
    }

    /// Consumes and returns the next live value, skipping tombstones; returns null when exhausted.
    T *getItem()
    {
        while (true)
        {
            if (currentIndex == invalidIndex)
            {
                return nullptr;
            }

            T *bTarget = array[currentIndex].value;
            currentIndex = array[currentIndex].next;
            if (bTarget)
            {
                return bTarget;
            }
        }
    }

    /// Peeks at the next live value, advancing past tombstones without consuming the live entry.
    T *hasElement()
    {
        while (true)
        {
            if (currentIndex == invalidIndex)
            {
                return nullptr;
            }
            if (T *bTarget = array[currentIndex].value)
            {
                return bTarget;
            }
            currentIndex = array[currentIndex].next;
        }
    }

    /// Consumes the current live entry. Precondition: `hasElement()` returned non-null.
    void moveForward()
    {
        currentIndex = array[currentIndex].next;
    }
};

#endif // HMAKE_POINTERARRAYLIST_HPP
