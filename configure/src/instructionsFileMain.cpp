
#include "instructionsFileMain.hpp"

#include <utility>

std::string instructionsFileMain::getScript() {
    std::string script;
    script += details.getScript();
    script += standard.getScript();
    script += mainExecutable.getScript();
    return script;
}

instructionsFileMain::instructionsFileMain(executable mainExecutable_, projectDetails details_, cxxStandard standard_):
mainExecutable(std::move(mainExecutable_)), details(details_), standard(standard_){

}
