
#include "CxxStandard.hpp"

CxxStandard::CxxStandard(CxxStandardEnum cxx_standard, bool standard_required) {
  this->cxx_standard = cxx_standard;
  this->standard_required = standard_required;
}
