
#ifndef HMAKE_INSTRUCTIONSFILEMAIN_HPP
#define HMAKE_INSTRUCTIONSFILEMAIN_HPP


#include "projectDetails.hpp"
#include "cxx_standard.hpp"
#include "executable.hpp"

struct instructionsFileMain {
    projectDetails details;
    cxxStandard standard;
    executable mainExecutable;
    instructionsFileMain(executable mainExecutable_, projectDetails details_ = projectDetails(),
                         cxxStandard standard_= cxxStandard(cxxStandardEnum::Seventeen, true));
    std::string getScript();
};


#endif //HMAKE_INSTRUCTIONSFILEMAIN_HPP
