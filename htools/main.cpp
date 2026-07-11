#include "Configure.hpp"
#include <fstream>

using std::ofstream, std::filesystem::current_path;
int main()
{
    toolsCache.detectToolsAndInitialize();
    if (!exists(toolsCache.toolsCacheFilePath.parent_path()))
    {
        create_directories(toolsCache.toolsCacheFilePath.parent_path());
    }
    toolsCache.writeToolsCacheFile();
}