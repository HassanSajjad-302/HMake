
#include<filesystem>
#include"file.hpp"
file::file(std::filesystem::__cxx11::path path)
{
    if (fs::directory_entry(path).status().type() == fs::file_type::regular){
        this->path = path;
    }else{
        std::cerr <<"Not a Regular file: "<<path<<std::endl;
        exit(-1);
    }
}

std::string file::getScript()
{
    return path;
}

FileArray::FileArray(directory dir, bool (*predicate)(std::string)){
    for (auto&& x : fs::directory_iterator(dir.path))
    {
        if(x.status().type() == fs::file_type::regular)
        {
            if(predicate(x.path().filename().generic_string()))
            {
                fileNames += x.path().filename().generic_string() + "\n";
            }
        }
    }
}

std::string FileArray::getScript(){
    return fileNames;
}
