
#ifndef STATICVECTOR_HPP
#define STATICVECTOR_HPP

#include <vector>

using std::vector;

template <typename T, uint64_t stackSize> class StaticVector
{
    T arrayContainer[stackSize];
    vector<T> vectorContainer;
    uint64_t currentArraySize = 0;
    bool usingArray = true;

  public:
    void emplace_back(T value)
    {
        if (usingArray)
        {
            if (currentArraySize == stackSize)
            {
                usingArray = false;
                vectorContainer.reserve(stackSize + 1);
                std::copy_n(arrayContainer, stackSize, vectorContainer.data());
                vectorContainer.emplace_back(value);
            }
            else
            {
                arrayContainer[currentArraySize] = value;
                ++currentArraySize;
            }
        }
        else
        {
            vectorContainer.emplace_back(value);
        }
    }

    bool empty() const
    {
        return !currentArraySize;
    }

    uint64_t size() const
    {
        return usingArray ? currentArraySize : vectorContainer.size();
    }

    T &operator[](uint64_t i)
    {
        return usingArray ? arrayContainer[i] : vectorContainer[i];
    }

    const T &operator[](uint64_t i) const
    {
        return usingArray ? arrayContainer[i] : vectorContainer[i];
    }

    // for string only
    StaticVector &operator+=(const string_view &str)
    {
        for (const char c : str)
        {
            emplace_back(c);
        }
        return *this;
    }
};
#endif // STATICVECTOR_HPP
