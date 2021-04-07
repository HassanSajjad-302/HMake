
#include "library.hpp"
#include "libraryDependency.hpp"

library::library(std::string targetName, libraryType type_) {
    this->targetName = targetName;
    type = type_;
}

std::string library::getScript() {
    std::string script = "add_library(" + targetName + " ";
    if(sourceFiles.empty()){
        throw std::logic_error("Source File Should Be Provided");
    }
    if(type == libraryType::STATIC){
        script += " STATIC\n";
    }else if(type == libraryType::MODULE){
        script += " MODULE\n";
    }else if(type == libraryType::SHARED){
        script += " SHARED\n";
    }
    for(auto& f: sourceFiles){
        script += f.getScript() + "\n";
    }
    script += ")\n\n";

    if(!includeDirectoryDependencies.empty()){
        script += "target_include_directories(" + targetName + "\n";
        for(auto& i : includeDirectoryDependencies){
            if(i.directoryDependency == dependencyType::PUBLIC){
                script += "PUBLIC ";
            }else if(i.directoryDependency == dependencyType::PRIVATE){
                script += "PRIVATE ";
            }else if(i.directoryDependency == dependencyType::INTERFACE){
                script += "INTERFACE ";
            }
            script += i.includeDirectory.getScript() + "\n";
        }
        script += ")\n\n";
    }

    if(!libraryDependencies.empty()){
        script += "target_link_libraries(" + targetName + "\n";
        for(auto& i : libraryDependencies){
            if(i.libraryDependencyType == dependencyType::PUBLIC){
                script += "PUBLIC ";
            }else if(i.libraryDependencyType == dependencyType::PRIVATE){
                script += "PRIVATE ";
            }else if(i.libraryDependencyType == dependencyType::INTERFACE){
                script += "INTERFACE ";
            }
            script += i.library_.targetName;
        }
        script += ")\n\n";
    }

    if(!compilerOptionDependencies.empty()){
        script += "target_compile_options(" + targetName + "\n";
        for(auto& i : compilerOptionDependencies){
            if(i.compilerOptionDependency == dependencyType::PUBLIC){
                script += "PUBLIC ";
            }else if(i.compilerOptionDependency == dependencyType::PRIVATE){
                script += "PRIVATE ";
            }else if(i.compilerOptionDependency == dependencyType::INTERFACE){
                script += "INTERFACE ";
            }
            script += i.compilerOption;
        }
        script += ")\n\n";
    }

    if(!compileDefinitionDependencies.empty()){
        script += "target_compile_definitions(" + targetName + "\n";
        for(auto& i : compileDefinitionDependencies){
            if(i.compileDefinitionDependency == dependencyType::PUBLIC){
                script += "PUBLIC ";
            }else if(i.compileDefinitionDependency == dependencyType::PRIVATE){
                script += "PRIVATE ";
            }else if(i.compileDefinitionDependency == dependencyType::INTERFACE){
                script += "INTERFACE ";
            }
            script += i.compileDefinition;
        }
        script += ")\n\n";
    }
    return script;
}