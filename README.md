# HMake

HMake is a new build system that does not invent a new DSL for its project build specification.
Currently, it only provides C++ build and C++ API.
Later on, build support for more programming languages will be added.
API in other programming languages will be provided as well.
Tested on Windows 11 with MSVC 19.36.32537 and Ubuntu 22.04 LTS with GCC 12.1

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp");
}

MAIN_FUNCTION
```

</details>

In order to run this first build the hmake project with cmake with CMAKE_BUILD_CONFIGURATION to RELEASE and then,
create symbolic links for hhelper and hbuild binaries in /usr/local/bin/ or on Windows
add the build directory in your system Environment Variables Path.
Project will create 4 binaries hhelper, hbuild, htools, hwrite and HMakeHelper
and one static-lib hconfigure.
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
in cmake build-dir for debugging on Windows.

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

MAIN_FUNCTION
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

MAIN_FUNCTION
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

MAIN_FUNCTION
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
```prebuiltLinkOrArchiveTarget``` of the ```DSC```.

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
        [&](Builder &builder, BTarget &bTarget) {
            if (bTarget.getRealBTarget(0).exitStatus == EXIT_SUCCESS &&
                bTarget.fileStatus.load(std::memory_order_acquire))
            {
                std::filesystem::copy(catShared.getLinkOrArchiveTarget().getActualOutputPath(),
                                      path(animalShared.getLinkOrArchiveTarget().getActualOutputPath()).parent_path(),
                                      std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(catShared.getLinkOrArchiveTarget().getActualOutputPath());
                std::lock_guard<std::mutex> lk(printMutex);
                printMessage("libCat.so copied to Animal/ and deleted from Cat/\n");
            }
        },
        animalShared.getLinkOrArchiveTarget(), catShared.getLinkOrArchiveTarget());
}

MAIN_FUNCTION
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
To support all the functionality HMake runs in 3 rounds.
In first round,
it populates dependencies.
In second round,
it generates smrules files.
In third round, it builds the object files.
and does different things in different rounds.
```BTarget``` is the center of all this and will be extended in the last Example.
Instead of ```GetRoundZeroUpdateBTarget```, ```outputDirectory``` could have been used in this case.

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

        DSC<CppSourceTarget> &cat =
            GetCppTargetDSC_P("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().INTERFACE_INCLUDES("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = GetCppTargetDSC("Dog" + str, targetType, true, "DOG_EXPORT");
        dog.PUBLIC_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog/src/Dog.cpp").PUBLIC_INCLUDES("Dog/header");

        DSC<CppSourceTarget> &dog2 = GetCppTargetDSC("Dog2" + str, targetType, true, "DOG2_EXPORT");
        dog2.PRIVATE_LIBRARIES(&cat).getSourceTarget().SOURCE_FILES("Dog2/src/Dog.cpp").PUBLIC_INCLUDES("Dog2/header");

        DSC<CppSourceTarget> &app = GetCppExeDSC("App" + str);
        app.getLinkOrArchiveTarget().setOutputName("app");
        app.PRIVATE_LIBRARIES(&dog).getSourceTarget().SOURCE_FILES("main.cpp");

        DSC<CppSourceTarget> &app2 = GetCppExeDSC("App2" + str);
        app2.getLinkOrArchiveTarget().setOutputName("app");
        app2.PRIVATE_LIBRARIES(&dog2).getSourceTarget().SOURCE_FILES("main2.cpp");
    };

    makeApps(TargetType::LIBRARY_STATIC);
    makeApps(TargetType::LIBRARY_SHARED);
}

MAIN_FUNCTION
```

</details>

This example showcases the consumption of a prebuilt library.
Also showcases the dependency propagation,
and the controlling prowess of ```DSC```
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
    GetCppExeDSC("app2").getSourceTarget().MODULE_FILES("main2.cpp").assignStandardIncludesToPublicHUDirectories();
}

MAIN_FUNCTION
```

</details>

This example showcases the HMake Modules Support.
In ```round == 1```, i.e. in the second-last / second round,
HMake generates the .smrule file for all the module-files and the header-units,
if any are being included by these module-files.
This .smrule file is specified by the compiler according to the P1689R5 paper.
In round 2, HMake also determines the dependencies between different modules,
and then in round 3, will build them accordingly.
```app2``` marks all the ```requirementIncludes``` for which ```isStandard = true```
as header-unit-includes by calling the function ```assignStandardIncludesToPublicHUDirectories```.
Any directory which has header-units should be marked by at-least one and only one
target as hu-include(header-unit-include) in a target or its dependencies.
So, HMake can decide what target to associate with this header-units from that directory.
Modules from a ```CppSourceTarget``` can use modules from a dependency ```CppSourceTarget```,
while at the same time being compiled by a different compile-command.

```CppSourceTarget``` member functions ```PRIVATE_HU_DIRECTORIES```,
```PUBLIC_HU_INCLUDES``` and ```PRIVATE_HU_INCLUDES```
are used for registering an include-directory for header-units.
The reason for ```PRIVATE_HU_DIRECTORIES``` besides
```PUBLIC_HU_INCLUDES``` and ```PRIVATE_HU_INCLUDES```
is that sometimes general include-directories are less specialized
while header-unit-include-directories are more specialized.
That is showcased in Example 9.

### Example 8

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_DIRECTORIES("Mod_Src/");
}

MAIN_FUNCTION
```

</details>

### Example 9

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

template <typename... T> void initializeTargets(DSC<CppSourceTarget> &target, T &...targets)
{
    CppSourceTarget &t = target.getSourceTarget();
    string str = t.name.substr(0, t.name.size() - 4); // Removing -cpp from the name
    t.MODULE_DIRECTORIES_RG("src/" + str + "/", ".*cpp")
        .PRIVATE_HU_DIRECTORIES("src/" + str)
        .PUBLIC_HU_DIRECTORIES("include/" + str);

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &config)
{
    config.compilerFeatures.PRIVATE_INCLUDES("include");

    DSC<CppSourceTarget> &stdhu = config.GetCppObjectDSC("stdhu");

    stdhu.getSourceTargetPointer()->assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &lib4 = config.GetCppTargetDSC("lib4", config.targetType);
    DSC<CppSourceTarget> &lib3 = config.GetCppTargetDSC("lib3", config.targetType).PUBLIC_LIBRARIES(&lib4);
    DSC<CppSourceTarget> &lib2 =
        config.GetCppTargetDSC("lib2", config.targetType).PUBLIC_LIBRARIES(&stdhu).PRIVATE_LIBRARIES(&lib3);
    DSC<CppSourceTarget> &lib1 = config.GetCppTargetDSC("lib1", config.targetType).PUBLIC_LIBRARIES(&lib2);
    DSC<CppSourceTarget> &app = config.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1);

    initializeTargets(lib1, lib2, lib3, lib4, app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    Configuration &static_ = GetConfiguration("static");
    static_.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    configurationSpecification(static_);

    Configuration &object = GetConfiguration("object");
    object.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_OBJECT);
    configurationSpecification(object);
}

MAIN_FUNCTION
```

</details>

This example showcases usage of ```PRIVATE_HU_DIRECTORIES```.
In this example, if we had used ```HU_INCLUDES``` functions instead,
then this would have been a configuration error.
Because twp targets would have the same header-unit-include,
and HMake won't have been able to decide which target to associate with the header-unit.
Using ```PRIVATE_HU_DIRECTORIES``` ensure that header-units from ```include/lib1/```
and ```src/lib1/``` are linked with lib1 and so on.

### Example 10

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app")
        .getSourceTarget()
        .PUBLIC_HU_INCLUDES("3rd_party/olcPixelGameEngine")
        .R_MODULE_DIRECTORIES("modules/", "src/")
        .ASSIGN(CxxSTD::V_LATEST);
}

MAIN_FUNCTION
```

</details>

### Example 11. HMake Project Example

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

using std::filesystem::file_size;

struct SizeDifference : public CTarget, public BTarget
{
    Configuration &sizeConfiguration;
    Configuration &speedConfiguration;

    SizeDifference(string name, Configuration &sizeConfiguration_, Configuration &speedConfiguration_)
        : CTarget(std::move(name)), sizeConfiguration(sizeConfiguration_), speedConfiguration(speedConfiguration_)
    {
        RealBTarget &realBTarget = realBTargets.emplace_back(this, 0);
        if (speedConfiguration.getSelectiveBuild() && sizeConfiguration.getSelectiveBuild() && getSelectiveBuild())
        {
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

    BTarget *getBTarget() override
    {
        return this;
    }

    virtual ~SizeDifference() = default;

    void updateBTarget(Builder &, unsigned short round) override
    {
        RealBTarget &realBTarget = realBTargets[0];

        if (!round && realBTarget.exitStatus == EXIT_SUCCESS)
        {
            string sizeDirPath = targetSubDir + "Size";
            string speedDirPath = targetSubDir + "Speed";

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

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &fmt = configuration.GetCppStaticDSC("fmt").PUBLIC_LIBRARIES(&stdhu);
    fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

    configuration.markArchivePoint();

    DSC<CppSourceTarget> &hconfigure = configuration.GetCppStaticDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
    hconfigure.getSourceTarget()
        .MODULE_DIRECTORIES("hconfigure/src")
        .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include", "rapidjson/include");

    DSC<CppSourceTarget> &hhelper = configuration.GetCppExeDSC("hhelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hhelper.getSourceTarget()
        .MODULE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include"))
        .PRIVATE_COMPILE_DEFINITION("RAPIDJSON_HEADER", addEscapedQuotes(srcDir + "rapidjson/include"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include"))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_PATH",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_PATH", addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).string()));

    DSC<CppSourceTarget> &hbuild = configuration.GetCppExeDSC("hbuild").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp");

    DSC<CppSourceTarget> &hmakeHelper =
        configuration.GetCppExeDSC("HMakeHelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hmakeHelper.getSourceTarget().MODULE_FILES("hmake.cpp").PRIVATE_COMPILE_DEFINITION("EXE");

    DSC<CppSourceTarget> &exp = configuration.GetCppExeDSC("exp").PRIVATE_LIBRARIES(&stdhu);
    exp.getSourceTarget().MODULE_FILES("main.cpp").PRIVATE_INCLUDES("rapidjson/include");
}

void buildSpecification()
{
    Configuration &releaseSpeed = GetConfiguration("RSpeed");
    CxxSTD cxxStd = releaseSpeed.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    releaseSpeed.ASSIGN(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::RELEASE);
    // releaseSpeed.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS", "1");

    Configuration &releaseSize = GetConfiguration("RSize");
    releaseSize.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);

    if (selectiveConfigurationSpecification(&configurationSpecification))
    {
        targets<SizeDifference>.emplace("Size-Difference", releaseSize, releaseSpeed);
    }
}

MAIN_FUNCTION
```

</details>

This example showcases HMake extensibility and support for drop-in replacement of
header-files with header-units.
```GetCppObjectDSC``` is used for a target for which there is no link-target i.e. header-only cpp-target.
In this case header-unit only.

```markArchivePoint``` function is a WIP. It will be used to specify that fmt, json and stdhu are never
meant to be changed. So, hbuild can ignore checking object-files of these for rebuild.

This function call ```selectiveConfigurationSpecification(&configurationSpecification)```
means that if hbuild isn't executed in a configuration dir,
then that configuration won't even be specified.
Thus, a project can have multiple configurations and rebuild speed won't be affected.
HMake also has a selectiveBuild feature which means that for an app and its dependency, if hbuild
executed in app, then app and dependency both would be built, but if executed in dependency, then
only the dependency will be built.

```TranslateInclude::YES``` will result in treating ```#include``` as ```import```.
In the MSVC case, it is ```/translateInclude``` flag specification.
```TreatModulesAsSource::YES``` causes the calls to ```MODULE_FILES``` and ```MODULE_DIRECTORIES```
routed to ```SOURCE_FILES``` and ```SOURCE_DIRECTORIES```.
Changing this to ```TreatModulesAsSource::NO``` will cause drop-in replacement to header-units
of standard headers and of headers coming from hconfigure directory but not of all
headers. The reason is ```/translateInclude``` flag checks for ```header-units.json``` file
in the header-directory and mentions only those in smulres which are mentioned in the file.
Because this file is not present in external libs include-dirs,
```USE_HEADER_UNITS``` macro is defined to consume all header-files as header-units.

You can use hwrite to write the ```header-units.json``` file.
Execute the hwrite in the respective directory, and pass it the extension.
e.g. ```hwrite .hpp```.

A limitation of HMake is that macros imported from other header-units can't be used to dictate
header-units inclusion.
Basically, [this](https://developercommunity.visualstudio.com/t/scanDependencies-does-not-take-into-acc/10029154?q=header+unit&page=1).
HMake works by generating smurles in round 1 and then building the module in round 0.
It can't first scan, then build, then scan the dependents and then build them.
A workaround is to promote such macros to the build system,
so these are provided on command-line.

Please notice that HMake does not cache ```PRIVATE_HU_DIRECTORIES``` call.
Neither it stores the contents of ```header-units.json``` file in cache.
Both of which can impact the header-units to be built.
So, if you change these when you have already built the project,
nothing will happen.
Because, header-units are discovered during the build-process,
and as no file is compiled, these changes won't be reflected.
In order to reflect these changes,
delete the dependents targets build-dir.
By dependents, I mean those targets which can import a header-unit from such directory.
Now, when such targets will be rebuilt,
the changes will be reflected.
I think, once set, you won't change it often.
But, it might be cached later on as-well.
So, if you change it all the dependent targets are completely rebuilt.

Also notice, HMake does not check the .ifc file at all.
If you externally update the .ifc or delete it,
Please delete the corresponding .o file as well.
This is for optimization.
I think this is an acceptable tradeoff.
.smrules file is only checked if .o file is newer than source-files.
So, if you edit it externally, delete the corresponding .o file as-well.

Thus, for drop-in replacement, compiler needs to support P1689R5 and a ```TranslateInclude::YES```
flag.
GCC does not support P1689R5.
Clang isn't supported at the moment.
As per the clang.jam (the b2 build-system Clang file),
Clang uses GCC command-line on Linux and MSVC command-line on Windows.
So, you can masquerade it as GCC or MSVC, however, clang.jam also
sets the ```--target=``` flags on Windows which are not set by the HMake.
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
TODOs for these are mentioned in ```BasicTarget.hpp```. Help would be appreciated.

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