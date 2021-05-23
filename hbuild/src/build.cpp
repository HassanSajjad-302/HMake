

#include "build.hpp"
#include "fstream"
#include "iostream"
#include "nlohmann/json.hpp"

typedef nlohmann::ordered_json Json;

void build::build(fs::path exeHmakeFilePath) {
  Json exeFileJSON;
  std::ifstream(exeHmakeFilePath) >> exeFileJSON;
  fs::path filePath = exeFileJSON.at("PROJECT_FILE_PATH").get<std::string>();
  Json mainFileJSON;
  std::ifstream(filePath) >> mainFileJSON;

  fs::path compilerPath = mainFileJSON.at("COMPILER").get<Json>().at("PATH").get<std::string>();
  fs::path linkerPath = mainFileJSON.at("LINKER").get<Json>().at("PATH").get<std::string>();
  std::string compilerFlags = mainFileJSON.at("COMPILER_FLAGS").get<std::string>();
  std::string linkerFlags = mainFileJSON.at("LINKER_FLAGS").get<std::string>();
  //todo: if there is anything more specific to the executable it should be checked here. Otherwise for most parts
  // we will be defaulting to the defaults of main project file.

  std::string exeName = exeFileJSON.at("NAME").get<std::string>();
  fs::path exeBuildDirectory = exeFileJSON.at("BUILD_DIRECTORY").get<fs::path>();
  auto exeSourceFiles = exeFileJSON.at("SOURCE_FILES").get<std::vector<fs::path>>();

  //build process starts
  fs::create_directory(exeBuildDirectory);
    fs::path buildCacheFilesDirPath = exeHmakeFilePath.parent_path() / "Cache_Build_Files";
    bool skipLinking = true;
    if(fs::exists(exeBuildDirectory/exeName)){
        std::cout<<"Exe File Already exists. Will Check Using Cache Files Whether it needs and update." << std::endl;
        for(auto i : exeSourceFiles){
            if(fs::last_write_time((buildCacheFilesDirPath/i.filename()).string() + ".o") <
               fs::last_write_time(i)){
                std::cout << i.string() << " Needs An Updated .o cache file" << std::endl;
                std::string compileCommand = compilerPath.string() + " "+ compilerFlags +
                                             " -c " + i.string()
                                             + " -o " + (buildCacheFilesDirPath/i.filename()).string() + ".o";
                std::cout<<compileCommand << std::endl;
                system(compileCommand.c_str());
                skipLinking = false;
            }else{
                std::cout << "Skipping Build of file " << i.string() << std::endl;
            }

        }
    }else{
        std::cout<<"Building From Start"<<std::endl;

        fs::create_directory(buildCacheFilesDirPath);

        for(auto i : exeSourceFiles){
            std::string compileCommand = compilerPath.string() + " "+ compilerFlags +
                    " -c " + i.string()
                    + " -o " + (buildCacheFilesDirPath/i.filename()).string() + ".o";
            std::cout<<compileCommand << std::endl;
            system(compileCommand.c_str());
        }
        skipLinking = false;
    }
    if(!skipLinking){
        //Run linker now.
        std::string linkerCommand = linkerPath.string() + " " + linkerFlags +
                                    fs::path(buildCacheFilesDirPath/fs::path("")).string() + "*.o " +
                                    " -o " + (exeBuildDirectory/exeName).string();
        std::cout << "Linking" << std::endl;
        std::cout << linkerCommand << std::endl;
        system(linkerCommand.c_str());
        std::cout<< "Built Complete" << std::endl;
    }else{
        std::cout<< "Skipping Linking Part " << std::endl;
    }

}
