#ifndef CXX_STANDARD_HPP
#define CXX_STANDARD_HPP

#include <string>

enum CxxStandardEnum {
  Nighty_Eight = 98,
  Eleven = 11,
  Fourteen = 14,
  Seventeen = 17,
  Twenty = 20
};

struct CxxStandard {
  CxxStandardEnum cxx_standard;
  bool standard_required = false;
  CxxStandard(CxxStandardEnum cxx_standard, bool standard_required);
};
#endif// CXX_STANDARD_HPP
