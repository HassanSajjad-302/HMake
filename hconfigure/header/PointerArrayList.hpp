
#ifndef HMAKE_POINTERARRAYLIST_HPP
#define HMAKE_POINTERARRAYLIST_HPP

#ifdef USE_HEADER_UNITS
#else
#endif
#include <cstdint>

template <typename T> class PointerArrayList
{
    struct ArrayListItem
    {
        T *value;
        uint32_t next;
    };

    ArrayListItem *array = nullptr;
    uint32_t currentIndex = -1;
    uint32_t start = 0;
    uint32_t last = 0;
    uint32_t arraySize = 0;

  public:
    void clear()
    {
        last = 0;
        arraySize = 0;
        currentIndex = 0;
        // TODO
        // arraySize should be recorded to report max array size to not get into memory issues.
    }

    uint32_t size() const
    {
        return arraySize;
    }

    PointerArrayList()
    {
        array = new ArrayListItem[1024 * 1024];
    }

    void emplace_back(T *bTarget)
    {
        array[arraySize].value = bTarget;
        array[arraySize].next = -1;
        if (arraySize)
        {
            // array is not empty, so making the last array element point to the newly inserted one.
            array[last].next = arraySize;
        }
        last = arraySize;
        // just added a new item after we had exhausted the list in previous iteration.
        if (currentIndex == -1)
        {
            currentIndex = last;
        }
        ++arraySize;
    }

    void emplace_front(T *bTarget)
    {
        array[arraySize].value = bTarget;
        if (!arraySize)
        {
            array[arraySize].next = -1;
        }
        else
        {
            // array is not empty, so making the newly inserted element point to the last one.
            array[arraySize].next = currentIndex;
        }
        currentIndex = arraySize;
        ++arraySize;
    }

    // at least one item should be present. there is no empty check.
    void emplace(T *bTarget)
    {
        array[arraySize].value = bTarget;
        array[arraySize].next = currentIndex;
        currentIndex = arraySize;
        ++arraySize;
    }

    T *getItem()
    {
        if (currentIndex == -1)
        {
            return nullptr;
        }

        T *bTarget = array[currentIndex].value;
        currentIndex = array[currentIndex].next;
        return bTarget;
    }
};

#endif // HMAKE_POINTERARRAYLIST_HPP
