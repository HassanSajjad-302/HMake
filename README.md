# HMake

HMake is a build-system software.
It does not invent a new DSL for the build specification.
Currently, it only provides C++ build and C++ API.
Later on, build support for other programming languages will be added.
API in multiple other programming languages will be provided as well.

HMake has the most advanced build algorithm that supports dynamic nodes
and dynamic edges.
It also has advanced dependency specification.
See ```hconfigure/header/BTarget.hpp``` and ```hconfigure/header/Builder.hpp```.
HMake has some great optimizations.
See ```hconfigure/header/Node.hpp```,
```hconfigure/header/TargetCache.hpp```.

```hconfigure``` lib totals at around 17k SLOC.
This is much smaller compared to e.g. CMake + Ninja.

```BTarget```,```Builder```, ```Node``` and ```TargetCache```
form the core of the build-system.
These are extensively documented.
I recommend that you read until [C++ Examples](#c-examples),
then you go over these classes docs in the respective files
before the C++ Examples.

HMake has the state-of-the-art support for C++20 modules and header-units.
It is the only one that supports C++20 header-units.
And the only one that supports modules and hu without scanning them first.
These are supported using a newly invented IPC based approach.
[2978](https://htmlpreview.github.io/?https://github.com/HassanSajjad-302/iso-papers/blob/main/generated/my-paper.html).
Currently, this is available only in my
[pull-request](https://github.com/llvm/llvm-project/pull/147682).
Besides header-units, HMake also supports Big header-units.
This means that with one switch you can compile your header-units individually
or as one big hu composing all the header-files for improved build-speed.

Core of the build-system has no reference to the
```CppTarget```, ```CppSrc```, ```CppMod```, ```LOAT``` classes
which form the C++ build-system on the top of the core.
This should tell you about the extensibility of the core API .
These classes are also extensively documented.

I am fully confident of reliability of my software.
It is extensively tested.
If you run the ```Tests``` CMake target,
you can see a variety of Tests.

Currently, the build-system is set up only for my custom fork.
This means that you will have to build my fork first.
You also need to have the latest Visual Studio 2022 with C++ support installed.
As it was used to build the HMake itself
and the MSVC STL was used with my Clang fork.
Module support is only set up for Windows.
All non-module tests passed with my custom fork and gcc on Linux,
however, minor customizations are needed for modules support.
These will be done soon.
Better tool detection and easier setup is a priority.

## Boost Example

I compiled approx 20 Boost libraries
including Tests and Examples with C++20 header-units.
hu build was **2.3x(45.727 / 19.704)** faster than conventional build in debug mode.
A total of **704** executables were compiled in both hu and debug build.
Of the 20 libraries, only 5 could be successfully compiled as header-units.
This might be due to either compiler rejecting valid source-code
or due to mis-configuration.
As not all header-files can be compiled as header-units.
Due to lack of time, I did not investigate.
Also, my Clang fork does not support shared memory files.

I think if all the Boost is compiled with HMake
and all libraries are compiled as header-units,
with a compiler having full support of my paper,
this would result in **>7x** build speed-up.

For mega projects like UE5,
the difference could be bigger like **>12x**.

Mega-projects often need custom tasks, like custom tests setup,
code-generation or documentation generation
or release package preparation and deployment.
HMake is the best in this.
As it is vertically integrated,
you have full context and can directly modify the central DAG.
This makes it easy to visualize dependency relationships
and to diagnose redundant dependencies,
which improves the core saturation.
This could be a major improvement for some projects.

I profiled ```ninja``` with clang argument for zero target build.
It spent **65%** time on Parsing.
Only **15% - 25%** time was spent on files stat calls;
While HMake spends **70%-80%** in files stat calls.
With some pending optimizations,
it would be **>80%**.
Also, HMake stat files concurrently.
Compared to single-thread stat,
this makes a difference of 30%-40% in zero-target build.
While ninja did not launch new threads in zero-target build.
Due to this, I estimate that in zero-target build,
my software is 2x faster than Ninja.
I think because of these optimizations,
HMake memory-consumption is lowerer than Ninja as-well.

Instructions for running this example are given
in C++Examples Example1 below.

## HMake Architecture

class ```BTarget``` is the building block of the build-system.
HMake has a very simple algorithm.
You define all the targets and dependencies between them.
HMake then topologically sorts.
Those with 0 dependencies are added to a list, ```Builder::updatedBTargets```.
Then we start building the targets in this list.
As soon as we build a target, we decrement from all the dependents
```RealBTarget::dependenciesSize```.
If the dependent ```RealBTarget::dependenciesSize == 0```,
i.e. no dependency of the target is left to be built,
we add it to the list
```Builder::updateBTargets```.
```Builder::execute``` will continue until all targets in
```Builder::updateBTargets``` are built.

Let's do some examples

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    b->addDepNow<0>(*a);
}

MAIN_FUNCTION
```

</details>


To extend the build-system, we need to derive from the ```BTarget```
and override ```updateBTarget``` function.
If you build this example, it will print "Hello\nWorld\n" during the build.
First ```a.updateBTarget``` runs and then ```b.updateBTarget``` runs.
Because we specified a dependency relationship between ```a``` and ```b```.
But, what is the ```round == 0``` and ```addDepNow<0>```?
HMake does topological sorting and target updating 2 times in one execution.
In ```round == 1```, we develop the compile command
and link command of different targets based on their dependencies.
This is what the CMake does.
and in ```round == 0```, we build these which is what Ninja does.

Let's clarify this with more examples.

### Example 2

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0 || round == 1)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");

    b->addDepNow<0>(*a);
    a->addDepNow<1>(*b);
}

MAIN_FUNCTION
```

</details>

Now, this example will print ```World\nHello\nHello\nWorld\n```.
As we inverted the dependency relationship for round 1 compared to round 0.
```BTarget``` constructor initializes ```realBTargets```
which is ```array<RealBTarget, 2>```.
So, by declaring 1 ```BTarget```, you declare 2 ```RealBTargets```.
```addDepNow<0>``` will add dependency for round0 while
```addDepNow<1>``` will add dependency for round1.

### Example 3

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    unsigned short low, high;
    explicit OurTarget(const unsigned short low_, const unsigned short high_) : low(low_), high(high_)
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            for (unsigned short i = low; i < high; ++i)
            {
                printMessage(FORMAT("{} ", i));
                // To simulate a long-running task that continuously outputs.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget(10, 20);
    OurTarget *b = new OurTarget(50, 70);
    OurTarget *c = new OurTarget(800, 1000);
}

MAIN_FUNCTION
```

</details>

This example simulates a long-running task.
HMake is a fully multithreaded build-system.
The ```buildSpecification``` function is executed single-threaded
but almost right after that the threads are launched
and HMake calls ```BTarget::updateBTarget``` on all cores.
So, the output in the above example will be garbled as we did not specify any dependencies,
and all 3 ```OurTarget::updateBTarget``` is executed in parallel.

### Example 4

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    OurTarget *c = new OurTarget("HMake");
    a->addDepNow<0>(*b);
    b->addDepNow<0>(*c);
    c->addDepNow<0>(*a);
}

MAIN_FUNCTION
```

</details>

This will print the following.

```
Cycle found: BTarget 0 -> BTarget 1 -> BTarget 2 -> BTarget 0
```

By overriding ```BTarget::getTarjanNodeName```,
we can customize this message to differentiate between different overrides of BTarget.
By default, it prints ```BTarget``` and the id number.
```CppTarget```, ```LOAT``` prints ```name```,
while ```CppSrc``` and ```CppMod``` prints ```node->filePath```.

### Example 5

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

constexpr unsigned short roundLocal = 0;
struct OurTarget : BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, const bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == roundLocal)
        {
            if (error)
            {
                printMessage(FORMAT("Target {} runtime error.\n", name));
                realBTargets[roundLocal].exitStatus = EXIT_FAILURE;
            }
            if (realBTargets[roundLocal].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", name));
            }
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    OurTarget *c = new OurTarget("HMake");
    OurTarget *d = new OurTarget("CMake");
    OurTarget *e = new OurTarget("Ninja", true);
    OurTarget *f = new OurTarget("XMake");
    OurTarget *g = new OurTarget("build2", true);
    OurTarget *h = new OurTarget("Boost");
    d->addDepNow<roundLocal>(*e);
    h->addDepNow<roundLocal>(*g);
}

MAIN_FUNCTION
```

</details>

This example demonstrates HMake error handling.
If ```updateBTarget``` throws an exception or set ```RealBTarget::exitStatus```
to anything but ```EXIT_SUCCESS```, then HMake will set the ```RealBTarget::exitStatus```
of all the dependent targets to```EXIT_FAILURE```.
This way target can learn the execution status of its dependents
and also communicate theirs to their dependents.
If in a round, ```RealBTarget::exitStatus``` of any one of the targets is
not equal to ```EXIT_SUCCESS```,
then HMake will exit early and not execute the last round.

### Example 6

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    unsigned short low, high;
    explicit OurTarget(const unsigned short low_, const unsigned short high_)
        : BTarget(true, false), low(low_), high(high_)
    {
    }
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            for (unsigned short i = low; i < high; ++i)
            {
                printMessage(FORMAT("{} ", i));
                // To simulate a long-running task that continuously outputs.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
};

OurTarget *a, *b, *c;

struct OurTarget2 : BTarget
{
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            a = new OurTarget(10, 40);
            b = new OurTarget(50, 80);
            c = new OurTarget(800, 1000);
            a->addDepNow<0>(*c);
            b->addDepNow<0>(*c);

            std::lock_guard lk(builder.executeMutex);
            builder.updateBTargets.emplace(&c->realBTargets[0]);
            builder.updateBTargetsSizeGoal += 3;
        }
    }
};

void buildSpecification()
{
    OurTarget2 *target2 = new OurTarget2();
}

MAIN_FUNCTION
```

</details>

This example will print ```800``` to ```1000``` and
then it will print ```10``` to ```40``` and ```50``` to ```80``` garbled.
This is because of targets ```OurTarget *a, *b, *c;``` and the dependency relationship
between these targets.
These targets were not part of the DAG but instead dynamically added.
Initially, only ```target2``` was the part of the DAG.

HMake supports dynamic targets as demonstrated.
These are an HMake speciality.
Not only you can add new edges in the DAG dynamically,
but also new nodes as well.
However, you have to take care of the following aspects:

1. You have to update the ```Builder::updateBTargetsSizeGoal``` variable with the
   additional number of times ```updateBTarget``` will be called.
2. If any newly added targets do not have any dependency
   then it must be added in ```updateBTargets``` list like we added ```c``` target.
3. Besides new targets, we can also modify the dependencies of older targets.
   But these targets ```dependenciesSize``` should not be zero.
   Because if the target ```dependenciesSize``` becomes zero,
   it is added to the ```updateBTargets``` list.
   HMake does not allow removing elements from this list.
4. These data structures must not be modified without ```Builder::executeMutex``` locked.

If you have added new targets to ```Builder::updateBTargets``` list,
and there is still work left in ```BTarget::updateBTarget```,
you should notify ```Builder::cond``` to keep full core saturation.
If instead you will return right after the function,
then you don't need to as the freed core will be used to execute the next in
```Builder::updateBTargets```.

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            b->addDepNow<0>(*c);
            c->addDepNow<0>(*b);
        }
    }
};

void buildSpecification()
{
    b = new BTarget();
    c = new BTarget();
    OurTarget *target = new OurTarget();
    b->addDepNow<0>(*target);
    c->addDepNow<0>(*target);
}

MAIN_FUNCTION
```

</details>

In this example, we define a new dependency relationship between older targets.
This is completely legal as we don't break the Rule 3.
But, we have added a cyclic dependency.
This will be detected and HMake will print the following error.

```
Cycle found: BTarget 0 -> BTarget 1 -> BTarget 0
```

### Example 8

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *a;

struct OurTarget : BTarget
{
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            a = new BTarget();
            std::lock_guard lk(builder.executeMutex);
            ++builder.updateBTargetsSizeGoal;
            // builder.updateBTargets.emplace(&a->realBTargets[0]);
        }
    }
};

OurTarget *target;

void buildSpecification()
{
    target = new OurTarget();
}

MAIN_FUNCTION
```

</details>

This breaks the rule 2.
Uncommenting the line above will fix this.
This might hang or HMake might detect and print ```HMake API misuse```.

## Nodes

<details>
<summary></summary>

Every file and dir-path is represented by `Node` class in HMake.
This class assigns a permanent number to a filesystem path,
and it will remain consistent across
rebuilds and reconfigurations.
The build-cache and config-cache can use this number instead of the full
path.
This results in upto 200x lesser total cache
(config-cache + build-cache + nodes).
I estimate that for a project like UE5, for one configuration,
the total cache would be less than 10MB.
This means that even for very big projects,
build starts instantaneously and consumes very less memory.

For debugging purposes, HMake has macros to toggle between path or number
in cache files.

</details>

## TargetCache

<details>
<summary></summary>

```TargetCache``` class allows for storing cache at config-time and build-time.
Its constructor takes a unique name (generally same as ```BTarget::name```)
and at config-time creates an entry in both config-cache and build-cache
(if it did not exist before).
config-cache can only be written at config-time.
For example, for ```CppTarget```,
HMake stores the node numbers for source-files, module-files, header-units,
header-files and include-dirs in the config-cache.
User specify relative paths for these values in ```buildSpecification```
function.
HMake normalizes these and checks for uniqueness before storing in the
config-cache.
Now, at build-time ```buildSpecification``` does not make any filesystem calls
and instead retrieves these values from the config-cache.
Filesystem calls to fetch ```lastWriteTime``` are made later-on on
multiple thread.
While ```buildSpecification``` is run single-threaded.

</details>

```BTarget```, ```Node```, ```Builder``` and ```TargetCache```
form the core of the HMake build-system.
Mega projects can use these to design project specific APIs.

## C++ Examples

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

After fetching source-code,
modify the last line in ```CppCompilerFeatures::initialize```
to point to the path of release binary of my custom Clang fork.
Then build the HMake project
and add the build-dir in path environment variable.
Run ```htools``` with admin permission.
This detects all the installed tools.
Unlike few other build-systems, HMake does not
detect the tools installed every time you configure a project but
is instead done only when you run ```htools``` and the result is cached to
```C:\Program Files (x86)\HMake\toolsCache.json```
on Windows and ```/home/toolsCache.json``` in Linux.
Currently, it is just a stud.
HMake is more ```make``` like in this aspect:).
It writes in ```toolsCache.json```
whatever is specified in ```ToolsCache::detectToolsAndInitialize```.

Now, create build-dir in Example1 directory
and run hhelper twice and hbuild once.
hhelper will create the cache.json file.
cache.json file provides an opportunity to select a different toolset.
It has array indices to the arrays of different tools in toolsCache.json.
cache.json file also has the commands
that will be used to build ```configure``` and ```build``` executables.
build executable is built with ```BUILD_MODE``` macro defined.
Running hhelper second time will create these executables,
linking ```hconfigure-c``` and ```hconfigure-b``` respectively.
Only difference is that ```hconfigure-b``` is compiled with
```BUILD_MODE``` macro.
If the compilation of these executables succeed,
hhelper will run the ```configure``` exe in the build-dir
completing the configure stage.
Now running hbuild will run the ```build``` exe.
This will create the app executable in ```{buildDir}/release/app```.

To run the boost example,
download from link https://www.boost.org/releases/1.88.0/.
After unzipping,
copy the ```Projects/Boost/*``` to source-dir.
Then make ```Build``` dir in the source-dir and cd to that.
Run ```hhelper``` twice.
Now, you can cd to ```conventional-d``` and run ```hbuild```.
This will build only ```conventional-d``` configuration.
I measure the command execution time using [ptime]().
Similarly, you can build ```huBig-d```,
and measure the build-time of the both configurations.

CMakeLists.txt builds with address sanitizer,
so you need to copy the respective dll
in cmake build-dir for debugging on Windows.
It has targets for all the Examples.
You need to run these targets in the respective ```Build``` dir.

```getConfiguration``` creates a default ```Configuration```
with ```release``` name
and with config-type ```ConfigType::RELEASE```.
```CALL_CONFIGURATION_SPECIFICATION``` macro ensures that
```configurationSpecification``` for a ```Configuration```
is only called if ```hbuild``` is executed in build-dir
or build-dir/{Configuration::name}
directory.
This way if a project has multiple configurations defined,
and for the moment, we are interested in just one of them,
we can skip the others by executing the ```hbuild``` in
build-dir/{Configuration::name} of our interest.

This line ```config.getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");```
in the file create a
```DSC<CppTarget>```.
```DSC<CppTarget>``` manages dependency specification as you will see
later on.
It has pointers to ```CppTarget``` and ```LOAT```.
```getSourceTarget``` returns the ```CppTarget``` pointer to which we add the source-files.
There are other ```get*``` functions available in ```Configuration```
class.

Every ```Configuration``` has ```stdCppTarget``` and
```AssignStandardCppTarget``` defaulted to ```YES```.
These two variables control whether an ```stdCppTarget``` is created and
assigned as private dependency to all the ```Configuration``` targets.
This ```std``` target has standard include-dirs initialized from
```toolsCache.json``` file.
```get*``` functions adds this target as a private dependency based on
```AssignStandardCppTarget``` value.

### Example 2

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().sourceDirsRE(".", "file[1-4]\\.cpp|main\\.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(ConfigType::DEBUG);
    getConfiguration("Release").assign(LTO::ON); // LTO is OFF in ConfigType::RELEASE which is the default
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

Building this example will create two dirs ```Debug``` and ```Release```, based on the
```getConfiguraion``` line in ```buildSpecification```.
In both of these dirs the target ```app``` will be built with the respective configuration
properties, because,
we set these properties in ```assign``` call.
These features and the flags they result in are modeled on the Boost build-system b2.
A complete list of such features can be found in ```Features.hpp```.
```sourceDirsRE``` function also takes the ```regex``` argument,
which otherwise is defaulted to ```.*``` in ```sourceDirs```
while ```rSourceDirs``` uses a recursive dir iterator.

### Example 3

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    CppTarget &app = config.getCppExeDSC("app").getSourceTarget();
    app.sourceFiles("main.cpp");

    // Change the value of "FILE1" in cache.hmake to false and then run configure again.
    // Then run hbuild. Now file2.cpp will be used.
    // CacheVariable is template. So you can use any type with it. However, conversions from and to json should
    // exist for that type. See nlohmann/json for details. I guess mostly bool will be used.
    if (CacheVariable("FILE1", true).value)
    {
        app.sourceFiles("file1.cpp");
    }
    else
    {
        app.sourceFiles("file2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases the cache variable.
Changing ```FILE1``` bool to ```false``` and then
running ```hbuild``` after reconfiguring will rebuild the project,
and file2 will be used this time.

### Example 4

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &catStatic = config.getCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Static").privateDeps(catStatic).getSourceTarget().sourceFiles("main.cpp");

    DSC<CppTarget> &catShared = config.getCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Shared").privateDeps(catShared).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases dependency specification.
Also, ```getCppStatic``` and ```getCppShared``` are called with two optional arguments besides
the mandatory ```name``` argument.
The ```true``` means that the code would be compiled with the suitable compile-definition and
suitable compile-definition will also be propagated above,
based on whether the ```DSC``` is a static-library or a shared-library.
```DSC``` handles all that.
The third argument specifies what compile-definition to use.
By default ```name + "_EXPORT"``` compile-definition will be used.
On Windows, HMake by default, copies the shared library dependencies to the build-dir.
You can change that by ```assign(CopyDLLToExeDirOnNTOs::NO)``` call of the
```Configuration```.

### Example 6

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    auto makeApps = [&] {
        const string str = config.targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        Node *outputDir = bsMode == BSMode::CONFIGURE
                              ? Node::getNodeNonNormalized("../Example4/Build/Release/Cat" + str, false, false)
                              : nullptr;
        DSC<CppTarget> &cat = config.getCppTargetDSC_P("Cat" + str, outputDir, true, "CAT_EXPORT");
        cat.getSourceTarget().interfaceIncludes("../Example4/Cat/header");

        DSC<CppTarget> &dog = config.getCppTargetDSC("Dog" + str, true, "DOG_EXPORT");
        dog.publicDeps(cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppTarget> &dog2 = config.getCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT");
        dog2.privateDeps(cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

        DSC<CppTarget> &app = config.getCppExeDSC("App" + str);
        app.getLOAT().setOutputName("app");
        app.privateDeps(dog).getSourceTarget().sourceFiles("main.cpp");

        DSC<CppTarget> &app2 = config.getCppExeDSC("App2" + str);
        app2.getLOAT().setOutputName("app");
        app2.privateDeps(dog2).getSourceTarget().sourceFiles("main2.cpp");
    };

    config.targetType = TargetType::LIBRARY_STATIC;
    makeApps();
    config.targetType = TargetType::LIBRARY_SHARED;
    makeApps();
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases the consumption of a prebuilt library.
Also showcases the dependency propagation,
and the controlling prowess of ```DSC```
e.g. if a static-library has another static-library as dependency,
then this dependency will be propagated above up to the Shared-Library or Exe.
Also, static libraries will be specified in order as it may cause problems with
some linkers.

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.stdCppTarget->getSourceTarget().interfaceFiles("std.cpp", "std");
        config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration("modules").assign(IsCppMod::YES, StdAsHeaderUnit::NO);
    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases the HMake Modules Support.
if we want to compile hu or module,
then ```IsCppMod::YES``` must be supplied.
Specifying modules will be an error in ```IsCppMod::NO``` mode.
However, hu related API will instead use header-files.
This is to support backward compatibility so that if library supports
multiple compilers,
it can be compiled as hu with one and as header-files with the other.

If ```IsCppMod::YES``` supplied,
then all targets of configuration will compile using IPC based approach.

```StdAsHeaderUnit::NO``` ensures that standard target is not configured with header-units.
This ensures that all includes in MSVC ```std``` module are treated as header-files.
Compilation was failing otherwise.

```BigHeaderUnit::YES``` ensures that libraries are compiled as big hu.
This will compile 2 header-units.
One will contain all the ```stl``` includes.
While one will contain only ```Windows.h```.
This is set up in ```Configuration::initialize```.

### Example 8

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().moduleDirs("Mod_Src/");
}

void buildSpecification()
{
    getConfiguration().assign(IsCppMod::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

### Example 9

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"
#include "Configure.hpp"

template <typename... T> void initializeTargets(DSC<CppTarget> *target, T... targets)
{
    CppTarget &t = target->getSourceTarget();
    const string str = removeDashCppFromName(getLastNameAfterSlash(t.name));
    t.moduleDirsRE("src/" + str + "/", ".*cpp")
        .privateHUDirsRE("src/" + str, "", ".*hpp")
        .publicHUDirsRE("include/" + str, str + '/', ".*hpp");

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSourceTarget().interfaceIncludesSource("include");
    DSC<CppTarget> &lib4 = config.getCppTargetDSC("lib4");
    DSC<CppTarget> &lib3 = config.getCppTargetDSC("lib3").publicDeps(lib4);
    DSC<CppTarget> &lib2 = config.getCppTargetDSC("lib2").privateDeps(lib3);
    DSC<CppTarget> &lib1 = config.getCppTargetDSC("lib1").publicDeps(lib2);
    DSC<CppTarget> &app = config.getCppExeDSC("app").privateDeps(lib1);

    initializeTargets(&lib1, &lib2, &lib3, &lib4, &app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    getConfiguration("static").assign(cxxStd, IsCppMod::YES, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

Using ```interfaceIncludesSource```
and ```privateHUDirsRE``` ensure that header-units from ```include/lib1/```
and ```src/lib1/``` are considered header-units of ```lib1``` and so.

### Example 10

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &libB = config.getCppStaticDSC("libB");
    libB.getSourceTarget().moduleFiles("B.cpp").publicHeaderUnits("B.hpp", "B.hpp");

    config.getCppExeDSC("appA").privateDeps(libB).getSourceTarget().moduleFiles("A.cpp").privateHeaderUnits("A.hpp",
                                                                                                            "A.hpp");
}

void buildSpecification()
{
    getConfiguration().assign(IsCppMod::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

If in a same directory 2 header-units belong to 2 different target,
then the only option is to specify them manually in the ```hmake.cpp``` file.

## BTarget Selective Build Mechanism

<details>
<summary>
</summary>

## Key Concepts

### 1. Selective Build Flag

The `BTarget::selectiveBuild` flag determines if a target should be updated during a build.
`BTarget::updateBTarget` is called for all the BTargets
but `BTarget::selectiveBuild` is set for a selective few.
`BTarget::setSelectiveBuild` is called before round1
`BTarget::updateBTarget` call,
which sets the `BTarget::selectiveBuild`.

- **Set When:**
    - The target `BTarget::name` is not empty and is explicitly named in the `hbuild` command.
    - if `BTarget::buildExplicit == false` and `BTarget::hbuild` is executed
      in the target's build dir or its parent/child build dir.
    - `BTarget::selectiveBuild` is set for the all transitive dependencies if it is true for
      dependent.

---

### 2. Explicit Build (`buildExplicit`)

- When `BTarget::buildExplicit == true`:
    - The `BTarget:;selectiveBuild` flag is set only if the target is explicitly named in the `hbuild` command.
- Useful for special targets (e.g., Tests or Examples) that should not be automatically built unless explicitly
  requested.
- You can mimic Ninja like behavior by simply setting `buildExplict`
  for all the targets.
  Now, these targets will be built only when mentioned on the command-line.

---

### 3. Round Logic

- **Round 1**:
    - `setSelectiveBuild` is called to set the `selectiveBuild` flag based on dir rules.
- **Round 0**:
    - The `selectiveBuild` flag is used to decide if a target is built.

---

### 4. Make Directory

- If `BTarget::makeDirectory == true`, the target's dir is created during configuration.
- If `BTarget::makeDirectory == false`, no dir is created for the target.
- Two targets having same name is not undefined behavior.
  Both target's `BTarget::selectiveBuild` will be true, when mentioned on the command-line.

---

### 5. Empty Target Names

- Targets without a name can only have `selectiveBuild` as true when hbuild is
  executed in the configure-dir or one of the target's dependents `selectiveBuild`
  is true.

---

### Example 9

```cpp
#include "Configure.hpp"
#include <utility>

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str, string name = "", const bool makeDirectory = true, const bool buildExplicit = false)
        : BTarget(std::move(name), buildExplicit, makeDirectory, true, true), message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0 && selectiveBuild)
        {
            printMessage(FORMAT("{}", message));
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("A", "A");
    string str = "A";
    str += slashc;
    OurTarget *b = new OurTarget("B", str + 'B', false);
    OurTarget *c = new OurTarget("C", str + 'C', true, true);
    OurTarget *d = new OurTarget("D", "D");
    OurTarget *e = new OurTarget("E", "E");
    OurTarget *f = new OurTarget("F");
    c->addDepNow<0>(*e);
}

MAIN_FUNCTION
```

### Directory Structure (After Configuration)

├───a

│ &emsp;&ensp; └───c

├───d

└───e

└───f

- **No dirs** are created for targets **B** or **F**.

---

### Target Properties

- **A, C, D, E, F**: `makeDirectory = true`
- **C**: `buildExplicit = true`
- **F**: No name, not a dependency, and no `buildExplicit`.

---

### Build Outcomes

1.

Run `hbuild` in the configure dir:

Sample Output: `ABEDF`

C is skipped because `buildExplicit = true`, and it wasn’t explicitly named.

2.

Run `hbuild` in D or E:

Output: `D` (or `E` depending on dir)

Only the target in the current dir is printed; others are siblings.

3.

Run `hbuild` in A:

SampleOutput: `AB`

`C` is skipped because `buildExplicit = true` even it is a subdir.

4.

Run hbuild in the configure dir with A/C, `hbuild A/C`:

Sample Output: `ABEDCF`

C is explicitly named, so it’s included with other targets.

5.

Run hbuild in the A/C dir, `hbuild .`:

Sample Output: `AEC`

A is the parent dir, C is explicitly named,
E is the dependency of C.
B, D and F are sibling targets.

6.

Run hbuild in A with C, `hbuild C`:

Sample Output: `AEBC`

C is explicitly named.
E is included as a dependency of C even though it is a sibling dir.

7.

Run hbuild in A with C and ../d, `hbuild C ../D`:

Sample Output: `BAECD`
All except F, which lacks a directory, and has ```buildExplicit == false```,
and does not have any dependent targets either.
F is only printed when `hbuild` runs in the configure dir.

</details>

# Future Direction

HMake 1.0 will only be released when, a few of the mega projects like UE5, AOSP, Qt, etc. could be
supported.
So, the API can be considered idiomatic.
Also, the support for popular languages, and frameworks like Android, IOS, and,
the popular CI/CD etc. could be supported.
I am confident about releasing 1.0 in the next 1-3 years.
This depends, depends on reception.

If you maintain the build of a mega-project,
and want to compile your software 10x faster today,
I am very interested in collaboration.
Minimal source-code changes will be needed.
If you just need help related to any aspect of HMake or have any opinion to share,
Please feel free to approach me.

## Support

I have been working on this project, for close to 4 years.
Please consider donating.
Contact me here if you want to donate hassan.sajjad069@gmail.com,
or you can donate to me through Patreon. Thanks.
