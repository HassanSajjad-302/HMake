

#ifndef HMAKE_HCONFIGURE_HEADER_ENUMITERATOR_HPP
#define HMAKE_HCONFIGURE_HEADER_ENUMITERATOR_HPP

#include <type_traits>
template<typename C, C beginVal, C endVal>
class Iterator {
  typedef typename std::underlying_type<C>::type val_t;
  int val;

public:
  Iterator(const C &f) : val(static_cast<val_t>(f)) {}
  Iterator() : val(static_cast<val_t>(beginVal)) {}
  Iterator operator++() {
    ++val;
    return *this;
  }
  C operator*() { return static_cast<C>(val); }
  Iterator begin() { return *this; }//default ctor is good
  Iterator end() {
    static const Iterator endIter = ++Iterator(endVal);// cache it
    return endIter;
  }
  bool operator!=(const Iterator &i) { return val != i.val; }
};

#endif//HMAKE_HCONFIGURE_HEADER_ENUMITERATOR_HPP
