
#ifdef USE_HEADER_UNITS
import "Configure.hpp";
import <fstream>;
#else
#include "Configure.hpp"
#include <fstream>
#endif

using std::ofstream, std::filesystem::current_path;
int main()
{
    toolsCache.detectToolsAndInitialize();
    if (!exists(toolsCache.toolsCacheFilePath))
    {
        create_directories(toolsCache.toolsCacheFilePath.parent_path());
    }
    const Json toolsCacheJson = toolsCache;
    ofstream(toolsCache.toolsCacheFilePath) << toolsCacheJson.dump(4);
}