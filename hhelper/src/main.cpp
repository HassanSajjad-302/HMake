
#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "BuildTools.hpp";
import "Cache.hpp";
import "fmt/format.h";
import "JConsts.hpp";
import "ToolsCache.hpp";
import "Utilities.hpp";
import "zDLLLoader.hpp";
import <filesystem>;
import <fstream>;
#else
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Cache.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include "zDLLLoader.hpp"
#include <filesystem>
#include <fstream>
#endif

using std::string, std::vector, std::ifstream, std::ofstream, std::endl, std::filesystem::path,
    std::filesystem::current_path, std::filesystem::directory_iterator, std::to_string, std::runtime_error,
    std::filesystem::canonical;

void jsonAssignSpecialist(const string &jstr, Json &j, auto &container)
{
    if (container.empty())
    {
        return;
    }
    if (container.size() == 1)
    {
        j[jstr] = *container.begin();
        return;
    }
    j[jstr] = container;
}

#define THROW false
#ifndef HCONFIGURE_HEADER
#define THROW true
#endif
#ifndef JSON_HEADER
#define THROW true
#endif
#ifndef HCONFIGURE_STATIC_LIB_PATH
#define THROW true
#endif
#ifndef RAPIDHASH_HEADER
#define THROW true
#endif
#ifndef PARALLEL_HASHMAP
#define THROW true
#endif

int main(int argc, char **argv)
{
    /*    PDocument d(kObjectType);

        d.AddMember(PValue("Foo").Move(), PValue("Bar").Move(), ralloc)
            .AddMember(PValue("Bar").Move(), PValue("Foo").Move(), ralloc);

        writePValueToFile("check.json", d);

        return 0;*/

    if (THROW)
    {
        printErrorMessage("Macros Required for hhelper are not provided.\n");
        exit(EXIT_FAILURE);
    }
    bool onlyConfigure = false;
    if (argc == 2)
    {
        string argument(argv[1]);
        if (argument == "--configure")
        {
            onlyConfigure = true;
        }
    }
    int count = 0;
    path cacheFilePath;
    path cp = current_path();
    for (const auto &i : directory_iterator(cp))
    {
        if (i.is_regular_file() && i.path().filename() == "cache.json")
        {
            cacheFilePath = i.path();
            ++count;
        }
    }
    if (count > 1)
    {
        printErrorMessage("More than one file with cache.json name present\n");
        exit(EXIT_FAILURE);
    }
    if (count == 0)
    {
        path hconfigureHeaderPath = path(HCONFIGURE_HEADER);
        path jsonHeaderPath = path(JSON_HEADER);
        path rapidjsonHeaderPath = path(RAPIDJSON_HEADER);
        path rapidHashHeaderPath = path(RAPIDHASH_HEADER);
        path parallelHashMap = path(PARALLEL_HASHMAP);
        path fmtHeaderPath = path(FMT_HEADER);
        path hconfigureStaticLibDirectoryPath = path(HCONFIGURE_STATIC_LIB_DIRECTORY);
        path fmtStaticLibDirectoryPath = path(FMT_STATIC_LIB_DIRECTORY);
        path hconfigureStaticLibPath = path(HCONFIGURE_STATIC_LIB_PATH);
        path fmtStaticLibPath = path(FMT_STATIC_LIB_PATH);

        if constexpr (os == OS::LINUX)
        {

#ifdef USE_COMMAND_HASH
            string commandHashCompileDef = " -D USE_COMMAND_HASH ";
#else
            string commandHashCompileDef = "";
#endif

#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
            string useNodesCacheIndicesInCacheDef = " -D USE_NODES_CACHE_INDICES_IN_CACHE ";
#else
            string useNodesCacheIndicesInCacheDef = "";
#endif

            string compileCommand =
                "c++ -std=c++2b -fvisibility=hidden -fsanitize=thread -fno-omit-frame-pointer -fPIC " +
                commandHashCompileDef + useNodesCacheIndicesInCacheDef +
                " -I " HCONFIGURE_HEADER " -I " JSON_HEADER " -I " RAPIDJSON_HEADER "  -I " FMT_HEADER
                "  -I " RAPIDHASH_HEADER " -I " PARALLEL_HASHMAP
                " {SOURCE_DIRECTORY}/hmake.cpp -shared -Wl,--whole-archive -L " HCONFIGURE_STATIC_LIB_DIRECTORY
                " -l hconfigure -Wl,--no-whole-archive -L " FMT_STATIC_LIB_DIRECTORY
                " -l fmt -o {CONFIGURE_DIRECTORY}/" +
                getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure");
            cache.compileConfigureCommands.push_back(compileCommand);
        }
        else
        {

#ifdef USE_COMMAND_HASH
            string commandHashCompileDef = " /D USE_COMMAND_HASH ";
#else
            string commandHashCompileDef = "";
#endif

#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
            string useNodesCacheIndicesInCacheDef = " /D USE_NODES_CACHE_INDICES_IN_CACHE ";
#else
            string useNodesCacheIndicesInCacheDef = "";
#endif

            toolsCache.initializeToolsCacheVariableFromToolsCacheFile();
            if (toolsCache.vsTools.empty() && toolsCache.compilers.empty())
            {
                printErrorMessage(
                    "No compiler found from ToolsCache variable. Please ensure htools has been run as admin before\n");
                exit(EXIT_FAILURE);
            }

            // hhelper currently only works with MSVC compiler expected in toolsCache vsTools[0]
            string compileCommand = addQuotes(toolsCache.vsTools[0].compiler.bTPath.make_preferred().string()) + " ";
            for (const string &str : toolsCache.vsTools[0].includeDirectories)
            {
                compileCommand += "/I " + addQuotes(str) + " ";
            }
            compileCommand += commandHashCompileDef + useNodesCacheIndicesInCacheDef;
            compileCommand += "/I " + hconfigureHeaderPath.string() + " /I " + jsonHeaderPath.string() + " /I " +
                              rapidjsonHeaderPath.string() + " /I " + fmtHeaderPath.string() + " /I " +
                              rapidHashHeaderPath.string() + " /I " + parallelHashMap.string() +
                              " /std:c++latest /GL /EHsc /MD /nologo " +
                              "{SOURCE_DIRECTORY}/hmake.cpp /link /SUBSYSTEM:CONSOLE /NOLOGO /DLL ";
            for (const string &str : toolsCache.vsTools[0].libraryDirectories)
            {
                compileCommand += "/LIBPATH:" + addQuotes(str) + " ";
            }
            compileCommand += "/WHOLEARCHIVE:" + addQuotes(hconfigureStaticLibPath.string()) + " " +
                              addQuotes(fmtStaticLibPath.string()) + " /OUT:{CONFIGURE_DIRECTORY}/" +
                              getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure");
            compileCommand = addQuotes(compileCommand);
            cache.compileConfigureCommands.push_back(compileCommand);
        }

        Json cacheJson = cache;
        ofstream("cache.json") << cacheJson.dump(4);
    }
    else
    {
        string configureSharedLibPath =
            (current_path() / getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure")).string();
        if (onlyConfigure)
        {
            if (!exists(path(configureSharedLibPath)))
            {
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            Json cacheJson;
            ifstream("cache.json") >> cacheJson;
            configureNode = Node::getNodeFromNonNormalizedPath(current_path(), false);
            Cache cacheLocal = cacheJson;
            path sourceDirPath = cacheJson.at(JConsts::sourceDirectory).get<string>();
            if (sourceDirPath.is_relative())
            {
                sourceDirPath = (current_path() / sourceDirPath).lexically_normal();
            }
            sourceDirPath = sourceDirPath.string();

            string srcDirString = "{SOURCE_DIRECTORY}";
            string confDirString = "{CONFIGURE_DIRECTORY}";

            if (!cacheLocal.compileConfigureCommands.empty())
            {
                printMessage("Executing commands as specified in cache.json to produce configure executable\n");
            }

            for (string &compileConfigureCommand : cacheLocal.compileConfigureCommands)
            {
                if (const size_t position = compileConfigureCommand.find(srcDirString); position != string::npos)
                {
                    compileConfigureCommand.replace(position, srcDirString.size(), sourceDirPath.string());
                }
                if (const size_t position = compileConfigureCommand.find(confDirString); position != string::npos)
                {
                    compileConfigureCommand.replace(position, confDirString.size(), current_path().string());
                }
                printMessage(fmt::format("{}\n", compileConfigureCommand));
                int code = system(compileConfigureCommand.c_str());
                if (code != EXIT_SUCCESS)
                {
                    exit(code);
                }
            }
        }
        DLLLoader loader(configureSharedLibPath.c_str());
        typedef int (*Func2)(BSMode bsMode);
        auto func2 = loader.getSymbol<Func2>("func2");
        if (!func2)
        {
            printErrorMessage("Symbol func2 could not be loaded from configure dynamic library\n");
            exit(EXIT_FAILURE);
        }
        return func2(BSMode::CONFIGURE);
    }
}
