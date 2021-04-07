#ifndef PROJECTDETAILS_HPP
#define PROJECTDETAILS_HPP

#include<string>


struct CmakeVersion{
    int major;
    int minor;
    int patch;
    int tweak;
    CmakeVersion(int major =3, int minor =0, int patch =0, int tweak =0){
        this->major = major;
        this->minor = minor;
        this->patch = patch;
        this->tweak = tweak;
    }
    std::string
    getStr() const{
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

enum Languages{
    C, CXX, BOTH
};

class projectDetails
{
    const CmakeVersion cmake_minimum_required_;
    using ProjectName = std::string;
    ProjectName projectName_;
    const Languages languages_;
public:
    projectDetails(const CmakeVersion  cmake_minimum_required = CmakeVersion(), std::string projectName = "Sample", const Languages  languages = Languages::CXX);
    const CmakeVersion getCmakeVersion();
    const ProjectName getProjectName();
    const Languages getLanguages();
    std::string getScript()const;
};

#endif // PROJECTDETAILS_HPP
