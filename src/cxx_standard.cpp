
#include "cxx_standard.hpp"
std::string cxxStandard::getScript(){
    std::string script = "set(CMAKE_CXX_STANDARD " + std::to_string(cxx_standard) +")\nset(CMAKE_CXX_STANDARD_REQUIRED ";
    std::string requirement = standard_required? "ON" : "OFF";
    script += requirement + ")\n\n";
    return script;
}

cxxStandard::cxxStandard(cxxStandardEnum cxx_standard, bool standard_required) {
    this->cxx_standard = cxx_standard;
    this->standard_required = standard_required;
}
