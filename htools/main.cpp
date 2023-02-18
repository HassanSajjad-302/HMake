#include "Configure.hpp"
#include <fstream>
using std::ofstream, std::filesystem::current_path;
int main()
{
    toolsCache.detectToolsAndInitialize();
    if (!exists(toolsCache.toolsCacheFilePath))
    {
        std::filesystem::create_directories(toolsCache.toolsCacheFilePath.parent_path());
    }
    Json toolsCacheJson = toolsCache;
    ofstream(toolsCache.toolsCacheFilePath) << toolsCacheJson.dump(4);
}