# HMake

HMake is a new build system that does not invent a new DSL for its project build specification.
Currently, it only provides C++ build and C++ API.
Later on, build support for more programming languages will be added.
API in other programming languages might be provided as well.
Tested on Windows 11 with MSVC 19.35.32217.1 and Ubuntu 22.04 LTS with GCC 12.1

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp");
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

Don't be intimidated by the size of the hmake.cpp file.
Besides ```buildSpecification``` function other stuff could be a macro, but I prefer this style.

In order to run this first build the hmake project with cmake with CMAKE_BUILD_CONFIGURATION to RELEASE and then,
create symbolic links for hhelper and hbuild binaries in /usr/local/bin/ or on Windows
add the build directory in your system Environment Variables Path.
Project will create 4 binaries hhelper, hbuild, htools, and HMakeHelper and one static-lib hconfigure.
hhelper and hbuild is to be used frequently for building project while HMakeHelper can be used to debug the
hmake.cpp of the project itself.
htools command needs to be run once.
It detects all installed tools.
Unlike few other tools, HMake does not detect the tools installed every time you configure a project but
is instead done only once and the result is cached to ```C:\Program Files (x86)\HMake\toolsCache.json```
on Windows and ```/home/toolsCache.json/``` in Linux.
Currently, it is just a stud.
Administrative permissions are needed on Windows.
Also, the latest Visual Studio be installed.

Now, to build the Example you need to run hhelper once and hbuild twice.
hhelper will create the cache.json file.
cache.json file provides an opportunity to select a different toolset.
It has array indices to the arrays of different tools in toolsCache.json.
These tools must exist in toolsCache.json for HMake to work.
cache.json file also has the commands that hbuild will use to build the configure dll.
Running hbuild first time will create the configure dll,
linking hconfigure static-lib using whole-archive linker feature.
Running hbuild again will load the configure dll and call the ```func2``` with ```BSMode::BUILD```.
This will create the app executable in ```{buildDir}/app```.

configure dll also has other C API functions through which complete information about the configuration
can be obtained.
Editor/IDE can load this dll and call the ```func2``` with ```BSMode::IDE``` which will
call the ```buildSpecification``` function of the hmake.cpp file but will not build the project
in the function ```configureOrBuild```.
```buildSpecification``` function creates the global state which can be inquired later through other
functions available in ```C_API.hpp```.
The API in this file is WIP.
But later on, it will be LTS stable
and versioned,
thus you can provide your build specification in any programming language,
provided that it supports this LTS stable C API,
so that other tools like IDE/Editor could interact with it seamlessly.

The HMakeHelper is built with EXE compile-definition.
This will define the ```int main``` block in the hmake.cpp and undefine the ```func2``` block.
This way you can debug your build specification easily.
To run HMakeHelper in ```BSMode::BUILD```,pass the ```--build``` command-line argument to it.
On Linux HMake builds with ```-fsanitize=thread```

A side note, current CMakeLists.txt builds with address-sanitizer, so you need to copy the respective dll
in cmake build-dir for debugging.

This line ```GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp");``` in the file create a
```DSC<CppSourceTarget>```. ```DSC<CppSourceTarget>``` manages dependency specification as you will see
later on.
It has pointers to ```CppSourceTarget``` and ```LinkOrArchiveTarget```.
```getSourceTarget``` returns the ```CppSourceTarget``` pointer to which we add the source-files.
There are other ```Get*``` function available in ```GetTarget.hpp```.
These functions preserve by emplacing the element in ```targets``` template variable.

### Example 2

<details>
<summary>hmake.cpp</summary>

```cpp

#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.GetCppExeDSC("app")
        .getSourceTarget()
        .SOURCE_DIRECTORIES_RG(".", "file[1-4]\\.cpp|main\\.cpp")
        .SINGLE(LTO::ON, Optimization::SPACE);
}

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");
    debug.ASSIGN(ConfigType::DEBUG);
    Configuration &release = GetConfiguration("Release");
    release.ASSIGN(LTO::ON);

    configurationSpecification(debug);
    configurationSpecification(release);
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

Building this example will create two directories ```Debug``` and ```Release```, based on the
```GetConfiguraion``` line in ```buildSpecification```.
In both of these directories the target ```app``` will be built with the respective configuration
properties, because,
we set these properties in ```ASSIGN``` call.
These features and the flags they result in are modeled on the Boost build-system b2.
Complete list of such features can be found in ```Features.hpp```.
```ASSIGN``` can assign more than one features because it is a variadic template.
```Configuration``` has ```Get*``` like functions.
These functions will return the respective element but with the properties of Configuration.
In ```configurationSpecification```, ```Optimization::SPACE``` is set based on the ```LTO``` feature.
More functions like ```SINGLE``` are available in the ```CppSourceTarget``` and ```LinkOrArchiveTarget```,
as these are inheriting from the ```FeatureConvenienceFunctions.hpp```.
These functions allow more concise conditional property-specification.
```SOURCE_DIRECTORIES_RG``` function also takes the ```regex``` argument,
which otherwise is defaulted to ```.*``` in ```SOURCE_DIRECTORIES```
while ```R_SOURCE_DIRECTORIES``` uses a recursive directory iterator.

### Example 3

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    CppSourceTarget &app = GetCppExeDSC("app").getSourceTarget();
    app.SOURCE_FILES("main.cpp");

    // Change the value of "FILE1" in cache.hmake to false and then run configure again.
    // Then run hbuild. Now file2.cpp will be used.
    // CacheVariable is template. So you can use any type with it. However, conversions from and to json should
    // exist for that type. See nlohmann/json for details. I guess mostly bool will be used.
    if (CacheVariable("FILE1", true).value)
    {
        app.SOURCE_FILES("file1.cpp");
    }
    else
    {
        app.SOURCE_FILES("file2.cpp");
    }
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases cache variable.
Changing ```FILE1``` bool to ```false``` and then reconfiguring the project will rebuild the project,
because file2 will be used this time.

### Example 4

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catStatic = GetCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    GetCppExeDSC("Animal-Static").PRIVATE_LIBRARIES(&catStatic).getSourceTarget().SOURCE_FILES("main.cpp");

    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    GetCppExeDSC("Animal-Shared").PRIVATE_LIBRARIES(&catShared).getSourceTarget().SOURCE_FILES("main.cpp");
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases dependency specification.
Also, ```GetCppStatic``` and ```GetCppShared``` are called with two optional arguments besides
the mandatory ```name``` argument.
The ```true``` means that the code would be compiled with the suitable compile-definition and
suitable compile-definition will also be propagated above,
based on whether the ```DSC``` is a static-library or a shared-library.
```DSC``` handles all that.
The third argument specifies what compile-definition to use.
By default ```name + "_EXPORT"``` compile-definition will be used.
On Windows, HMake also by-default, copies the shared library dependencies to the build-dir.
You can change that by ```ASSIGN(CopyDLLToExeDirOnNTOs::NO)``` call to the
```linkOrArchiveTarget``` of the ```DSC```.

### Example 5

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat", true);
    catShared.getSourceTarget().SOURCE_FILES("../Example4/Cat/src/Cat.cpp").PUBLIC_INCLUDES("../Example4/Cat/header");

    DSC<CppSourceTarget> &animalShared = GetCppExeDSC("Animal").PRIVATE_LIBRARIES(
        &catShared, PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animalShared.getSourceTarget().SOURCE_FILES("../Example4/main.cpp");

    GetRoundZeroUpdateBTarget(
        [&](Builder &builder, unsigned short round, BTarget &bTarget) {
            if (bTarget.getRealBTarget(0).exitStatus == EXIT_SUCCESS)
            {
                std::filesystem::copy(catShared.linkOrArchiveTarget->getActualOutputPath(),
                                      path(animalShared.linkOrArchiveTarget->getActualOutputPath()).parent_path(),
                                      std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(catShared.linkOrArchiveTarget->getActualOutputPath());
                std::lock_guard<std::mutex> lk(printMutex);
                printMessage("libCat.so copied to Animal/ and deleted from Cat/\n");
            }
        },
        *(animalShared.linkOrArchiveTarget), *(catShared.linkOrArchiveTarget));
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases ```PrebuiltDep``` and extensibility.
```PrebuiltDep``` is used to specify the properties such as Rpath which are not the properties of
the dependency or dependent, but, are the properties of the dependency relationship.
This line ```PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false}``` means
that ```$ORIGIN``` Rpath will be specified to the ```Animal``` target with the ```Cat``` shared-library.
```.defautRpath = false``` is important,
because this means that the default-rpath,
which is the shared-library build-dir,
won't be used, and, instead the value of ```requirementRpath``` will be used.
```PrebuiltDep::usageRequirementRpath``` should be used to specify the rpath in-case,
another target depends on Animal, and, Cat is propagated to that target.
Generally, if ```defaultRpath``` is set to false, then, both should be set.

This example also showcases ```GetRoundZeroUpdateBTarget```.
This takes a functor and the dependencies.
This functor will be called after the dependencies are updated.
So, we, here have directly injected in the central DAG.
```round``` variable specifies what round it is.
To support all the functionality HMake runs in 4 rounds,
and does different things in different rounds.
```BTarget``` is the center of all this and will be extended in the last Example.
Instead of ```GetRoundZeroUpdateBTarget```, ```outputDirectory``` could had been used in this case.

Because of Rpath, this is a Linux only example.

### Example 6

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    auto makeApps = [](TargetType targetType) {
        string str = targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSCPrebuilt<CPT> &cat =
            GetCPTTargetDSC("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().INTERFACE_INCLUDES("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = GetCppTargetDSC("Dog" + str, true, "DOG_EXPORT", targetType);
        dog.PUBLIC_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog/src/Dog.cpp").PUBLIC_INCLUDES("Dog/header/");

        DSC<CppSourceTarget> &dog2 = GetCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT", targetType);
        dog2.PRIVATE_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog2/src/Dog.cpp").PUBLIC_INCLUDES("Dog2/header/");

        DSC<CppSourceTarget> &app = GetCppExeDSC("App" + str);
        app.linkOrArchiveTarget->setOutputName("app");
        app.PRIVATE_LIBRARIES(&dog).getSourceTarget().SOURCE_FILES("main.cpp");

        DSC<CppSourceTarget> &app2 = GetCppExeDSC("App2" + str);
        app2.linkOrArchiveTarget->setOutputName("app");
        app2.PRIVATE_LIBRARIES(&dog2).getSourceTarget().SOURCE_FILES("main2.cpp");
    };

    makeApps(TargetType::LIBRARY_STATIC);
    makeApps(TargetType::LIBRARY_SHARED);
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases the ```CPT```,```PrebuiltLinkOrArchiveTarget``` and ```DSCPrebuilt<CPT>```.
Which shows how you can consume a prebuilt C-Library.
Also showcases the dependency propagation, and the controlling prowess of ```DSC```
e.g. if a static-library has another static-library as dependency,
then this dependency will be propagated above upto the Shared-Library or Exe.

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_FILES("main.cpp", "std.cpp");
    GetCppExeDSC("app2").getSourceTarget().MODULE_FILES("main2.cpp").assignStandardIncludesToHUIncludes();
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases the HMake Modules Support.
In ```round == 1```, i.e. in the second-last / third round,
HMake generates the .smrule file for all the module-files and the header-units,
if any are being included by these module-files.
This .smrule file is specified by the compiler according to the P1689R5 paper.
In round 3, HMake also determines the dependencies between different modules,
and then in round 4, will build them accordingly.
```app2``` marks all the ```requirementIncludes``` for which ```isStandard = true```
as header-unit-includes by calling the function ```assignStandardIncludesToHUIncludes```.
Any directory which has header-units should be marked by at-least one and only one
target as hu-include(header-unit-include) in same module-scope.
So, HMake can decide what target to associate with this header-unit.
Module-scope is used to combine different CppSourceTarget's.
So, the modules from one ```CppSourceTarget``` can use modules from another ```CppSourceTarget```,
while at the same time being compiled by a different compile-command.
module-scope of a ```CppSourceTarget``` defaults to itself.

Generally, in a build-specification, ```CppSourceTarget::setModuleScope(CppSourceTarget *)```
should be called by all except one ```CppSourceTarget``` with the value of that target.

```CppSourceTarget::PUBLIC_HU_INCLUDES``` and ```CppSourceTarget::PRIVATE_HU_INCLUDES``` can mark an
include as hu-include at the same time.

### Example 8

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_DIRECTORIES("Mod_Src/");
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

### Example 9

<details>
<summary>hmake.cpp</summary>

```cpp

```

</details>

### Example 10. HMake Project Example

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

using std::filesystem::file_size;

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToHUIncludes();

    DSC<CppSourceTarget> &fmt = configuration.GetCppStaticDSC("fmt");
    fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

    configuration.markArchivePoint();

    DSC<CppSourceTarget> &hconfigure = configuration.GetCppStaticDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
    hconfigure.getSourceTarget()
        .MODULE_DIRECTORIES("hconfigure/src/")
        .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include");

    DSC<CppSourceTarget> &hhelper = configuration.GetCppExeDSC("hhelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hhelper.getSourceTarget()
        .MODULE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(
                path(hconfigure.linkOrArchiveTarget->getActualOutputPath()).parent_path().generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_PATH",
            addEscapedQuotes(path(hconfigure.linkOrArchiveTarget->getActualOutputPath()).generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(fmt.linkOrArchiveTarget->getTargetFilePath()).parent_path().generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_PATH",
            addEscapedQuotes(path(fmt.linkOrArchiveTarget->getActualOutputPath()).generic_string()));

    DSC<CppSourceTarget> &hbuild = configuration.GetCppExeDSC("hbuild").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp");
    configuration.setModuleScope(stdhu.getSourceTargetPointer());

    DSC<CppSourceTarget> &hmakeHelper =
        configuration.GetCppExeDSC("HMakeHelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hmakeHelper.getSourceTarget().MODULE_FILES("hmake.cpp").PRIVATE_COMPILE_DEFINITION("EXE");

    configuration.setModuleScope(stdhu.getSourceTargetPointer());
}

struct SizeDifference : public CTarget, public BTarget
{
    Configuration &sizeConfiguration;
    Configuration &speedConfiguration;

    SizeDifference(string name, Configuration &sizeConfiguration_, Configuration &speedConfiguration_)
        : CTarget(std::move(name)), sizeConfiguration(sizeConfiguration_), speedConfiguration(speedConfiguration_)
    {
        RealBTarget &realBTarget = getRealBTarget(0);
        if (speedConfiguration.getSelectiveBuild() && sizeConfiguration.getSelectiveBuild() && getSelectiveBuild())
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            for (LinkOrArchiveTarget *linkOrArchiveTarget : sizeConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    realBTarget.addDependency(*linkOrArchiveTarget);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    realBTarget.addDependency(*linkOrArchiveTarget);
                }
            }
        }
    }

    virtual ~SizeDifference() = default;

    void updateBTarget(Builder &, unsigned short round) override
    {
        RealBTarget &realBTarget = getRealBTarget(0);

        if (!round && realBTarget.exitStatus == EXIT_SUCCESS && getSelectiveBuild())
        {

            string sizeDirPath = getSubDirForTarget() + "Size/";
            string speedDirPath = getSubDirForTarget() + "Speed/";

            std::filesystem::create_directories(sizeDirPath);
            std::filesystem::create_directories(speedDirPath);

            unsigned long long speedSize = 0;
            unsigned long long sizeSize = 0;
            for (LinkOrArchiveTarget *linkOrArchiveTarget : sizeConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    sizeSize += file_size(linkOrArchiveTarget->getActualOutputPath());
                    std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                          sizeDirPath + linkOrArchiveTarget->actualOutputName,
                                          std::filesystem::copy_options::overwrite_existing);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    speedSize += file_size(linkOrArchiveTarget->getActualOutputPath());
                    std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                          speedDirPath + linkOrArchiveTarget->actualOutputName,
                                          std::filesystem::copy_options::overwrite_existing);
                }
            }

            std::lock_guard<mutex> lk{printMutex};
            fmt::print("Speed build size - {}\nSize build size - {}\n", speedSize, sizeSize);
            fflush(stdout);
        }
    }
};

bool operator<(const SizeDifference &lhs, const SizeDifference &rhs)
{
    return lhs.CTarget::id < rhs.CTarget::id;
}

void buildSpecification()
{
    // Configuration &debug = GetConfiguration("Debug");
    Configuration &releaseSpeed = GetConfiguration("RSpeed");
    Configuration &releaseSize = GetConfiguration("RSize");
    //  Configuration &arm = GetConfiguration("arm");

    CxxSTD cxxStd = releaseSpeed.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    /*    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::DEBUG, AddressSanitizer::OFF,
                     RuntimeDebugging::OFF);
        debug.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS");*/
    releaseSpeed.ASSIGN(cxxStd, TreatModuleAsSource::YES, TranslateInclude::YES, ConfigType::RELEASE);
    releaseSize.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);
    //  arm.ASSIGN(cxxStd, Arch::ARM, TranslateInclude::YES, ConfigType::RELEASE, TreatModuleAsSource::NO);
    /*        debug.compilerFeatures.requirementCompilerFlags += "--target=x86_64-pc-windows-msvc ";
            debug.linkerFeatures.requirementLinkerFlags += "--target=x86_64-pc-windows-msvc";*/
    //releaseSpeed.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS", "1");

    for (const Configuration &configuration : targets<Configuration>)
    {
        if (const_cast<Configuration &>(configuration).getSelectiveBuild())
        {
            configurationSpecification(const_cast<Configuration &>(configuration));
        }
    }

    targets<SizeDifference>.emplace("Size-Difference", releaseSize, releaseSpeed);
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
```

</details>

This example showcases HMake extensibility and support for drop-in replacement of
header-files with header-units.
```GetCppObjectDSC``` is used for a target for which there is no link-target i.e. header-only cpp-target.
In this case header-unit only.
This line ```configuration.setModuleScope(stdhu.getSourceTargetPointer());``` sets the
module-scope for all the targets in the configuration.

```markArchivePoint``` function is a WIP. It will be used to specify that fmt, json and stdhu are never
meant to be changed. So, hbuild can ignore checking object-files of these for rebuild.

This if block ```if (const_cast<Configuration &>(configuration).getSelectiveBuild())```
means that if hbuild isn't executed in a configuration, then that configuration won't even be
specified. Thus, a project can have multiple configurations and rebuild speed won't be affected.
HMake also has a selectiveBuild feature which means that for an app and its dependency, if hbuild
executed in app, then app and dependency both would be built, but if executed in dependency, then
only the dependency will be built.

```TranslateInclude::YES``` will result in treating ```#include``` as ```import```.
In the MSVC case, it is ```/translateInclude``` flag specification.
```TreatModulesAsSource::YES``` causes the calls to ```MODULE_FILES``` and ```MODULE_DIRECTORIES```
routed to ```SOURCE_FILES``` and ```SOURCE_DIRECTORIES```.
Changing this to ```TreatModulesAsSource::NO``` should cause drop-in replacement to header-units.
But it doesn't. Because using ```/translateInclude``` with MSVC only causes the standard
header-files to be treated as header-units, but not the project header-files.
So, the ```USE_HEADER_UNITS``` line needs to be uncommented. This, however, causes the build-failure,
which I am investigating. However, commenting this line and setting ```TranslateInclude::YES```
results in successful building i.e. build works fine with standard header-units.

Thus, for drop-in replacement, compiler needs to support P1689R5 and a ```TranslateInclude::YES```
flag.
GCC does not support P1689R5.
Clang isn't supported at the moment.
As per the clang.jam (the b2 build-system Clang file),
Clang uses GCC command-line on Linux and MSVC command-line on Windows.
So, you can masquerade it as GCC or MSVC, however, clang.jam also
sets the ```--target=``` flags which are not set by the HMake.
Also ```clang-scan-deps```, a different tool than clang supports P1689R5,
but this isn't supported in HMake.

This leaves us with ```SizeDifference```.
This is used to determine the difference of sizes between Speed and Size builds.
Only Executable targets are compared.
The constructor set the dependencies. In round 0, then the ```updateBTarget``` will be called only
after building the exe deps.
SizeDifference most probably will be promoted to the HMake code-base, because, feature like
this might be interesting for other users as-well.
Any feature in wild that is needed by more than one project (preferably 5),
will be integrated in the HMake.
Extension points will be provided as the need arises.

# Future Direction

I want to propose a new unconventional approach to the code compilation.
Instead of compiler and linker being executables, these can be shared-libs.
And the build-system will load these libs and call the respective functions.
So the process setup-cost is evaded.
Also, the API can have functions that are to be called by the compiler or linker, whenever,
a new file is to be read from the disk.
This way, a header-file or .ifc file can be preserved for longer usage.
And a same file can be used by multiple processes.
These are low-hanging fruits, but a complete API can also be designed.
So, the compiler does not have the driver at all.
The build-system itself acts as the driver.
And when invoked, the compiler jumps straight into the act without first doing the argument parsing.
If two compilers are doing same thing conceptually, then one could be a drop-in
replacement of the other.

I am also trying to expand the build algorithm, so, it can have
smart thread-allocation for different tasks based on priority and quota.
TODOs for these are mentioned in ```RealBTarget```, ```Builder.cpp```
and ```SMFile::duringSort```. Help would be appreciated.

Also, IDE/Editor tools need to parse the source-code for intelligent suggestions.  
Can this be cached and supplied to compiler backend, when user builds the code,
instead of doing full-build of the file?

HMake 1.0 will only be released when, few of the mega projects like UE5, AOSP, Qt etc. could be
supported.
So, the API can be considered idiomatic.
Also, the support for popular languages, frameworks like Android, IOS, and,
the popular CI/CD etc. could be supported.
I am confident about releasing 1.0 in next 1-3 years.
This depends, depends on reception.

If you maintain the build of a mega-project, or want to add a futuristic feature or
have code that uses C++20 modules,
I am very interested in collaboration.
If you just need help related to any aspect of HMake, or have any opinion to share,
Please feel free to approach.

## Support

I have been working on this project, on and off, for close to two years.
Please consider donating.
Contact here if you want to donate hassan.sajjad069@gmail.com. Thanks.