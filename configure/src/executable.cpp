#include "executable.hpp"


executable::executable(std::string targetName, file file) :runTimeDir(directory(".")){
this->targetName = targetName;
this->sourceFiles.emplace_back(std::move(file));
}

std::string executable::getScript() {
    std::string script;
    if(!libraryDependencies.empty()){
        for(auto& i : libraryDependencies){
            script += i.library_.getScript();
        }
    }


    script += "add_executable(" + targetName + "\n";

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

    if(runTimeDirectoryEnabled){
        script += "set_target_properties(" + targetName + " PROPERTIES\n" +
                  "        RUNTIME_OUTPUT_DIRECTORY " + runTimeDir.getScript() + ")";
    }
    return script;
}

void executable::setRunTimeOutputDirectory(directory dir) {
    runTimeDirectoryEnabled = true;
    runTimeDir = dir;
}

