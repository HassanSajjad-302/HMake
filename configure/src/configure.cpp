
#include "configure.hpp"
#include "fstream"

void configure(executable ourExecutable) {
    
}

void configure(project ourProject) {
    json projectFileJSON;
    projectFileJSON = ourProject;
    fs::path p = project::BUILD_DIRECTORY.path;
    std::string fileName = project::PROJECT_NAME + ".hmake";
    p += fileName;
    std::ofstream(p) << projectFileJSON.dump(4);
    for(auto& t: ourProject.projectExecutables){

    }
}
