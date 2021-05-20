#include"directory.hpp"

directory::directory(fs::path path){
    path = path.lexically_normal();
    if (fs::directory_entry(path).status().type() == fs::file_type::directory){
        this->path = path;
    }else{
        throw std::logic_error("Not a directory file");
    }
}

directory::directory(fs::path path, fs::path relative_to){
    path = relative_to += path;
    if (fs::directory_entry(path).status().type() == fs::file_type::directory){
        this->path = path;
    }else{
        throw std::logic_error("Not a directory file");
    }
}

directory::directory(fs::path path, commonDirectories relative_to){
    path.lexically_normal();
    switch (relative_to) {

        case SOURCE_DIR:

            break;
        case BUILD_DIR:
            break;
        case CURRENT_SOURCE_DIR:
            break;
        case CURRENT_BINARY_DIR:
            break;
    }
}

std::string directory::getScript(){
    return path;
}

directory::directory() {
    path = fs::current_path();
}
