
#include "configure.hpp"
#include "fstream"

void configure(executable ourExecutable) {
    json targetFileJSON;
    targetFileJSON = ourExecutable;
    fs::path p = ourExecutable.configureDirectory.path;
    std::string fileName = ourExecutable.targetName + ".target.hmake";
    p /= fileName;
    std::ofstream (p) << targetFileJSON.dump(4);
}

void configure(project ourProject) {
    json projectFileJSON;
    projectFileJSON = ourProject;
    fs::path p = project::BUILD_DIRECTORY.path;
    std::string fileName = project::PROJECT_NAME + ".hmake";
    p /= fileName;
    std::ofstream(p) << projectFileJSON.dump(4);
    for(auto& t: ourProject.projectExecutables){
        configure(t);
    }
}
