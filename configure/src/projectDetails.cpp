#include "projectDetails.hpp"

projectDetails::projectDetails
(const CmakeVersion cmake_minimum_required, std::string projectName, const Languages languages):
    cmake_minimum_required_{cmake_minimum_required},
    languages_{languages}
{
    projectName_ = projectName;
}


const CmakeVersion
projectDetails::getCmakeVersion()
{
    return cmake_minimum_required_;
}

std::string
projectDetails::getScript() const
{
    std::string script = "cmake_minimum_required(VERSION " + cmake_minimum_required_.getStr() + ")\n\n";
    std::string lang ="CXX";
    if(languages_ == Languages::BOTH)
        lang = "CXX C";
    if(languages_ == Languages::C)
        lang = "C";
    script += "project(" + projectName_ + " LANGUAGES " + lang + ")\n\n";
    return script;
}
