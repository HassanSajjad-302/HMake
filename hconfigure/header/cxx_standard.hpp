#ifndef CXX_STANDARD_HPP
#define CXX_STANDARD_HPP

#include<string>

enum cxxStandardEnum{
    Nighty_Eight = 98,
    Eleven = 11,
    Fourteen = 14,
    Seventeen = 17,
    Twenty = 20
};

struct cxxStandard{
    cxxStandardEnum cxx_standard;
    bool standard_required = false;
    cxxStandard(cxxStandardEnum cxx_standard, bool standard_required);
    std::string getScript();
};
#endif // CXX_STANDARD_HPP
