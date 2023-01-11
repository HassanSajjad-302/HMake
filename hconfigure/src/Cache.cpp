#include "Cache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "fmt/format.h"
#include "fstream"

using std::ifstream, fmt::print, std::ofstream;

Cache::~Cache() noexcept
{
    if (saveCache)
    {
        registerCacheVariables();
    }
}

void Cache::initializeCacheVariableFromCacheFile()
{
    // Cache from cacheFileJson is read in both of build system modes but is saved only in CONFIGURE mode.

    path filePath = path(configureDir) / "cache.hmake";
    ifstream(filePath) >> cacheFileJson;

    path srcDirPath = cacheFileJson.at(JConsts::sourceDirectory).get<string>();
    if (srcDirPath.is_relative())
    {
        srcDirPath = (configureDir / srcDirPath).lexically_normal();
    }

    srcDir = srcDirPath.generic_string();

    projectConfigurationType = cacheFileJson.at(JConsts::configuration).get<ConfigType>();
    compilerArray = cacheFileJson.at(JConsts::compilerArray).get<vector<Compiler>>();
    selectedCompilerArrayIndex = cacheFileJson.at(JConsts::compilerSelectedArrayIndex).get<int>();
    linkerArray = cacheFileJson.at(JConsts::linkerArray).get<vector<Linker>>();
    selectedLinkerArrayIndex = cacheFileJson.at(JConsts::compilerSelectedArrayIndex).get<int>();
    archiverArray = cacheFileJson.at(JConsts::archiverArray).get<vector<Archiver>>();
    selectedArchiverArrayIndex = cacheFileJson.at(JConsts::archiverSelectedArrayIndex).get<int>();
    libraryType = cacheFileJson.at(JConsts::libraryType).get<TargetType>();
    if (libraryType != TargetType::LIBRARY_STATIC && libraryType != TargetType::LIBRARY_SHARED)
    {
        print(stderr, "Cache libraryType TargetType is not one of LIBRARY_STATIC or LIBRARY_SHARED \n");
        exit(EXIT_FAILURE);
    }
    cacheVariables = cacheFileJson.at(JConsts::cacheVariables).get<Json>();
    // TODO
    //  Environment is initialized only for amd64
#ifdef _WIN32
    environment = Environment::initializeEnvironmentFromVSBatchCommand(
        R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64)");
#else

#endif
    // Settings are saved only if mode is configure.
    if (bsMode == BSMode::CONFIGURE)
    {
        if (!std::filesystem::exists(path(configureDir) / path("settings.hmake")))
        {
            Json settingsJson = Settings{};
            ofstream(path(configureDir) / path("settings.hmake")) << settingsJson.dump(4);
        }
    }
}

void Cache::registerCacheVariables()
{
    // Cache is saved only if mode is configure
    if (bsMode == BSMode::CONFIGURE)
    {
        path filePath = path(configureDir) / "cache.hmake";
        cacheFileJson[JConsts::cacheVariables] = Cache::cacheVariables;
        ofstream(filePath) << cacheFileJson.dump(4);
    }
}
