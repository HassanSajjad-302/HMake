# HMake

HMake is a new build system that does not invent a new DSL for its project build specification.
Currently, it only provides C++ build and C++ API.
Later on, build support for more programming languages will be added.
API in other programming languages will be provided as well.
Tested on Windows 11 with MSVC 14.39.33519 (Visual Studio 17.9.4) and Ubuntu 22.04 LTS with GCC 12.1

You can either skip to the examples for samples or just read through them.

## HMake Architecture

HMake is an extendable build-system.
class ```BTarget``` is the building block of the build-system.
HMake has a very simple algorithm.
You define all the targets and dependencies between them.
HMake then topologically sorts and informs you if there is any cycle.
Those with 0 dependencies are added to a list.
Then we start building the targets in this list.
As soon as we build a target, we decrement from all of the dependents ```target.dependenciesSize```.
If the dependent target.dependenciesSize == 0, i.e. no dependency of the target is left to be built, we add it to the
list right before the current iterator.
I have given an example in
this [link](https://mail-attachment.googleusercontent.com/attachment/u/0/?ui=2&ik=a1929870f7&attid=0.1&permmsgid=msg-a:r5672226254924593394&th=18a2ad2ea72cce82&view=att&disp=inline&realattid=f_llq1xn8n0&saddbat=ANGjdJ_iNE2wFA2eWXFMs0Y3jqjom3_xY92xWfJYKoRhgxQ3J2xOarm5YlDdIC42k6Tghka6H5LZxaSR4liy6_AL8SkWcC9M1daNysTEmZNjYGQkfyhfsohWlwMw384dSvhvKZoEEocuzMkYlFemWvxNHNOFI5fvKVO0HDPc29rf3fVdsmi8pIfQm0e3axNg8Q7-xQc3p9FSE3AC5LLzdr3H6ducaq2Ax5kCkvgufSouAF43Lesy6oaGu8NN_v_rPGkTb4H0tQZXpWpM8ATzevtJ5f5O7WKL_r4l38ishzQiJOM8nJ1TaKgAr5VgKYQifRAK1Z9S1pOJHSu5S7_JWS98-XMbubaxnKn2--aRMAR9P-Y0jr8gIsS3wyyWwkoqUauOQks8O5yo_fwhWwVf22TbSwFd7iaryBx9jCzcSca4oW3jz3hM-BD76ZAoowwWwfl1tIdHI5SkS69d7QHisSvF9Tl-nXCH37wos4LlN0EoJCECukVrMZWb-4bZbI_Y9BiDLmKTlPdVbAhGgHFypjVWfHrU_3yU9-DjBcMJCv5qC0jRfkGgYJ-Zj1iMNu3RygqpAvX-sSthJJCtbUW2-84ymBVJgO74sA4KeEhlVBSBqiuY2lkA3JT5ojQlQa8VgG-Fy6FkLH7fY0K3IjCj0hKhOh06HZC-UcqreM50bHtrSaQ3YUxFRPa50taQ37_4T0MKjHSsJbZxgBHjnTAKghTycDPVtbPQa9Cs-X5si7-jLmdAPiB79tNjzFhjhPkiwvQZvzMDSeW4JvePFr4UfqPoqv8m-ovjPx-D4uAWKW9YeFf35zDW2-OhFG0qhjgxTHmolq9xQLYrLOj61zbBa3Ow6kCxtTP-Rw2uDpTQuFWKs4Q_enmY1a5g3AtChEP97mOCuO26JmihFhJ1MkKxUiEFCk9-JEnaMRCw1bYGxxnQhG2p73nPE40VvZB8i9chWGHKQF8icjwGhdH8F15_H6BGbFOSlnmh6KJ6WYurM7IDXz0mdNd1nF9xqISd2zmBtvoQim240JG8IMH5ywTj).
This is referenced in my [paper](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2978r0.html).

Let's do some examples

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if(round == 0)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

OurTarget a("Hello");
OurTarget b("World");
void buildSpecification()
{
   b.realBTargets[0].addDependency(a);
}

MAIN_FUNCTION

```

</details>


To extend the build-system, we need to derive from the ```BTarget```
and override ```updateBTarget``` function.
If you build this example, it will print "Hello\nWorld\n" during the build.
First ```a.updateBTarget``` runs and then ```b.updateBTarget``` runs.
Because we specified a dependency relationship between ```a``` and ```b```.
But, what is the ```round == 0``` and ```realBTargets[0]```?
HMake does topological sorting and target updating 3 times in one execution.
This is to simplify C++20 modules support.
So, in ```round == 2```, we develop the compile command and link command of different targets
based on their dependencies.
This is what the CMake does.
While in ```round == 1``` we scan module source-files
and in ```round == 0```, we build the source which is what Ninja does.

Let's clarify this with more examples.

### Example 2

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if(round == 0 || round == 1)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

OurTarget a("Hello");
OurTarget b("World");
void buildSpecification()
{
   b.realBTargets[0].addDependency(a);
   a.realBTargets[1].addDependency(b);
}

MAIN_FUNCTION

```

</details>

Now, this example will print ```World\nHello\nHello\nWorld\n```.
AS we inverted the dependency relationship for round 1 compared to round 0.
```BTarget``` constructor initializes ```realBTargets``` which is ```array<RealBTarget, 3>```.
So, by declaring 1 ```BTarget```, you declare 3 ```RealBTargets```.

### Example 3

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : public BTarget
{
    unsigned short low, high;
    explicit OurTarget(unsigned short low_, unsigned short high_) : low(low_), high(high_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
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

OurTarget a(10, 20), b(50, 70), c(800, 1000);
void buildSpecification()
{
}

MAIN_FUNCTION
```

</details>

This example simulates a long-running task.
HMake is a fully multi-threaded build-system.
The ```buildSpecification``` function is executed single-threaded
but almost right after that the threads are launched and HMake updates BTargets on all cores.
So, the output in the above example will be garbled as we did not specify any dependencies,
and all 3 ```OurTarget::updateBTarget``` is executed in parallel.

### Example 4

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
    }
};

OurTarget a("Hello");
OurTarget b("World");
OurTarget c("HMake");
void buildSpecification()
{
    a.realBTargets[0].addDependency(b);
    b.realBTargets[0].addDependency(c);
    c.realBTargets[0].addDependency(a);
}

MAIN_FUNCTION
```

</details>

This will print in red (the default color for error messages)

```
There is a Cyclic-Dependency.
BTarget 2 Depends On BTarget 1.
BTarget 1 Depends On BTarget 0.
BTarget 0 Depends On BTarget 2.
```

By overriding ```BTarget::getTarjanNodeName```,
we can customize this message to differentiate between different overrides of BTarget.
By default, it prints ```BTarget``` and the id number.
```CppSourceTarget```, ```LinkOrArchiveTarget``` prints ```name```,
while ```SourceNode``` and ```SMFile``` prints ```node->filePath```.

### Example 5

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

unsigned short roundLocal = 2;
struct OurTarget : public BTarget
{
    string message;
    bool error = false;
    explicit OurTarget(string str, bool error_ = false) : message{std::move(str)}, error(error_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if (round == roundLocal || round == 1)
        {
            if (error)
            {
                throw std::runtime_error("Target " + message + " runtime error.\n");
            }
            if (realBTargets[2].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", message));
            }
        }
    }
};

OurTarget a("Hello"), b("World"), c("HMake"), d("CMake"), e("Ninja", true), f("XMake"), g("build2", true), h("Boost");
void buildSpecification()
{
    d.realBTargets[roundLocal].addDependency(e);
    h.realBTargets[roundLocal].addDependency(g);
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
If in a round, ```RealBTarget::exitStatus``` of any one of the targets is not equal to ```EXIT_SUCCESS```,
then HMake will exit early and not execute the remaining rounds.

### Example 6

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : public BTarget
{
    unsigned short low, high;
    explicit OurTarget(unsigned short low_, unsigned short high_) : low(low_), high(high_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
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

struct OurTarget2 : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (round == 0)
        {
            a = new OurTarget(10, 40);
            b = new OurTarget(50, 80);
            c = new OurTarget(800, 1000);
            a->realBTargets[0].addDependency(*c);
            b->realBTargets[0].addDependency(*c);

            {
                std::lock_guard<std::mutex> lk(builder.executeMutex);
                builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, c);
                builder.updateBTargetsSizeGoal += 3;
            }
            builder.cond.notify_one();
        }
    }
};

OurTarget2 target2;

void buildSpecification()
{
}

MAIN_FUNCTION
```

</details>

This example will print ```800``` to ```1000``` and
then it will print ```10``` to ```40``` and ```50``` to ```80``` garbled.
This is because of targets ```OurTarget *a, *b, *c;``` and the dependency relationship
between these targets.
These targets were not part of the DAG but instead dynamically added.

HMake supports dynamic targets as demonstrated.
These are HMake speciality.
However, you have to take care of the following aspects:

1. You have to update the ```Builder::updateBTargetsSizeGoal``` variable with the
   an additional number of targets whose ```updateBTarget``` will be called.
2. All those targets that do not have any dependency must be added in ```updateBTargets```
   list like we added ```c``` target.
3. Besides new targets, we can also modify the dependencies of older targets.
   But these targets ```dependenciesSize``` should not be zero.
   Because if the target ```dependenciesSize``` becomes zero,
   it is added to the ```updateBTargets``` list.
   HMake does not allow removing elements from this list.
4. These data structures must not be modified without ```Builder::executeMutex``` locked.
   And ```Builder::cond``` should be notified if we edit the ```updateBTargets``` list.

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget b, c;

struct OurTarget : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (round == 0)
        {
            b.realBTargets[0].addDependency(c);
            c.realBTargets[0].addDependency(b);
        }
    }
};

OurTarget target;

void buildSpecification()
{
    b.realBTargets[0].addDependency(target);
    c.realBTargets[0].addDependency(target);
}

MAIN_FUNCTION
```

</details>

In this example, we define a new dependency relationship between older targets.
This is completely legal as we don't break the Rule 3.
But, we have added a cyclic dependency.
This will be detected and HMake will print the following error.

```
There is a Cyclic Dependency.
BTarget 1 Depends On BTarget 0.
BTarget 0 Depends On BTarget 1.
Unknown exception
```

### Example 8

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *a;

struct OurTarget : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (round == 0)
        {
            a = new BTarget();
            std::lock_guard<std::mutex> lk(builder.executeMutex);
            ++builder.updateBTargetsSizeGoal;
            // builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, a);
        }
    }
};

OurTarget target;

void buildSpecification()
{
}

MAIN_FUNCTION
```

</details>

This breaks the rule 2.
Uncommenting the line above will fix this.
This might hang or HMake might detect and print ```HMake API misuse```.

## BTarget Selective Build Mechanism

You can skip this section. This is only needed if you are interested in extending the HMake
selectiveBuild mechanism.
<details>
<summary>
</summary>

## Key Concepts

### 1. Selective Build Flag

The `selectiveBuild` flag determines if a target should be updated during a build.
`updateBTarget` is called for all the BTargets
but `selectiveBuild` is set for a selective few.
`setSelectiveBuild` is called in round2 which sets the `selectiveBuild`.

- **Set When:**
    - The target `name` is not empty and is explicitly named in the `hbuild` command.
    - if `buildExplicit == false` and `hbuild` is executed
      in the target's build directory or its parent/child build directory.
    - `selectiveBuild` is also set for all the target's dependencies after round 1,
      before round 0.

---

### 2. Explicit Build (`buildExplicit`)

- When `buildExplicit = true`:
    - The `selectiveBuild` flag is set only if the target is explicitly named in the `hbuild` command.
- Useful for special targets (e.g., Tests or Examples) that should not be automatically built unless explicitly
  requested.
- You can mimic Ninja like behavior by simply setting `buildExplict`
  for all the targets.
  Now, these targets will be built only when mentioned on the command-line.

---

### 3. Round Logic

- **Round 2**:
    - `setSelectiveBuild` is called to set the `selectiveBuild` flag based on directory rules.
- **Round 0**:
    - The `selectiveBuild` flag is used to decide if a target is built.

---

### 4. Make Directory

- If `makeDirectory = true`, the target's directory is created during configuration.
- If `makeDirectory = false`, no directory is created for the target.
- Two targets having same name is not undefined behavior.
  Both target's `selectiveBuild` will be true, when mentioned on the command-line.

---

### 5. Empty Target Names

- Targets without a name can only have `selectiveBuild` as true when hbuild is
  executed in the configure-dir or one of the target's dependents `selectiveBuild`
  is true.

---

### Example 9

```cpp


#include <utility>

#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str, string name = "", const bool makeDirectory = true, const bool buildExplicit = false)
        : BTarget(std::move(name), buildExplicit, makeDirectory, true, false, true), message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round) override
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
    c->realBTargets[0].addDependency(*e);
}

MAIN_FUNCTION
```

### Directory Structure (After Configuration)

├───a

│ &emsp;&ensp; └───c

├───d

└───e

└───f

- **No directories** are created for targets **B** or **F**.

---

### Target Properties

- **A, C, D, E, F**: `makeDirectory = true`
- **C**: `buildExplicit = true`
- **F**: No name, not a dependency, and no `buildExplicit`.

---

### Build Outcomes

1.

Run `hbuild` in the configure directory:

Sample Output: `ABEDF`

C is skipped because `buildExplicit = true`, and it wasn’t explicitly named.

2.

Run `hbuild` in D or E:

Output: `D` (or `E` depending on directory)

Only the target in the current directory is printed; others are siblings.

3.

Run `hbuild` in A:

SampleOutput: `AB`

`C` is skipped because `buildExplicit = true` even it is a subdirectory.

4.

Run hbuild in the configure directory with A/C, `hbuild A/C`:

Sample Output: `ABEDCF`

C is explicitly named, so it’s included with other targets.

5.

Run hbuild in the A/C directory, `hbuild .`:

Sample Output: `AEC`

A is the parent directory, C is explicitly named,
E is the dependency of C.
B, D and F are sibling targets.

6.

Run hbuild in A with C, `hbuild C`:

Sample Output: `AEBC`

C is explicitly named.
E is included as a dependency of C even though it is a sibling directory.

7.

Run hbuild in A with C and ../d, `hbuild C ../D`:

Sample Output: `BAECD`
All except F, which lacks a name, dependencies, and buildExplicit.
F only prints when `hbuild` runs in the configure directory.


</details>

## Nodes

Every file and directory-path is represented by `Node` class in HMake.
This class ensures that a file once checked for timestamp is not checked again.
This is a common feature in build-systems.

But HMake has an exclusive **game-changing and unprecedented** feature.

HMake assigns a number to every new node as it is discovered, and it
writes a nodes file when it saves build-cache and config-cache.
In nodes file, it saves the nodes and their assigned number.
Now, in build-cache and config-cache, it references these files with numbers
instead of full filepath.
This results in exceptionally shorter build-cache files.
HMake, like Ninja, use compile-command hash to compare compile-commands
to reduces the build-cache size.
HMake also compresses file to reduce the file-size further.
For debugging purposes, the CMakeLists.txt file includes options
to enable or disable any of these individually or in combination.

These features combined reduce the build-cache by **200x**.

------Compare boost and predict for UE5.

Total build-system residue with cap'n'proto should be less than 5MB.


<details>

<summary> Nodes And TargetCache</summary>
</details>

## C++ Examples

### Example 1

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");
}

MAIN_FUNCTION
```

</details>

To run this first build the hmake project with cmake with CMAKE_BUILD_CONFIGURATION to RELEASE and then,
create symbolic links for hhelper and hbuild binaries in /usr/local/bin/ or on Windows
add the build directory in your system Environment Variables Path.
The project will create 4 binaries hhelper, hbuild, htools, hwrite, and HMakeHelper
and one static-lib hconfigure.
hhelper and hbuild are to be used frequently for building projects while HMakeHelper can be used to debug the
hmake.cpp of the project itself.
htools command needs to be run once.
It detects all installed tools.
Unlike few other tools, HMake does not detect the tools installed every time you configure a project but
is instead done only once and the result is cached to ```C:\Program Files (x86)\HMake\toolsCache.json```
on Windows and ```/home/toolsCache.json/``` in Linux.
Currently, it is just a stud.
Administrative permissions are needed on Windows.
Also, the latest Visual Studio be installed.

Now, to build the Example you need to run hhelper twice and hbuild once.
hhelper will create the cache.json file.
cache.json file provides an opportunity to select a different toolset.
It has array indices to the arrays of different tools in toolsCache.json.
These tools must exist in toolsCache.json for HMake to work.
cache.json file also has the commands that hbuild will use to build the configure dll.
Running hhelper the second time will create the configure dll,
linking hconfigure static-lib using whole-archive linker feature.
Running hbuild will load the configure dll and call the ```func2``` with ```BSMode::BUILD```.
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
To run HMakeHelper in ```BSMode::BUILD```, pass the ```--build``` command-line argument to it.
On Linux HMake builds with ```-fsanitize=thread```

A side note, current CMakeLists.txt builds with address sanitizer, so you need to copy the respective dll
in cmake build-dir for debugging on Windows.

This line ```getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");``` in the file create a
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
    configuration.getCppExeDSC("app")
        .getSourceTarget()
        .sourceDirectoriesRE(".", "file[1-4]\\.cpp|main\\.cpp")
        .SINGLE(LTO::ON, Optimization::SPACE);
}

void buildSpecification()
{
    Configuration &debug = getConfiguration("Debug");
    debug.assign(ConfigType::DEBUG);
    Configuration &release = getConfiguration("Release");
    release.assign(LTO::ON);

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
we set these properties in ```assign``` call.
These features and the flags they result in are modeled on the Boost build-system b2.
A complete list of such features can be found in ```Features.hpp```.
```assign``` can assign more than one feature because it is a variadic template.
```Configuration``` has ```Get*``` like functions.
These functions will return the respective element but with the properties of Configuration.
In ```configurationSpecification```, ```Optimization::SPACE``` is set based on the ```LTO``` feature.
More functions like ```SINGLE``` are available in the ```CppSourceTarget``` and ```LinkOrArchiveTarget```,
as these are inheriting from the ```FeatureConvenienceFunctions.hpp```.
These functions allow more concise conditional property specification.
```sourceDirectoriesRE``` function also takes the ```regex``` argument,
which otherwise is defaulted to ```.*``` in ```SOURCE_DIRECTORIES```
while ```R_SOURCE_DIRECTORIES``` uses a recursive directory iterator.

### Example 3

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    CppSourceTarget &app = getCppExeDSC("app").getSourceTarget();
    app.sourceFiles("main.cpp");

    // Change the value of "FILE1" in cache.hmake to false and then run configure again.
    // Then run hbuild. Now file2.cpp will be used.
    // CacheVariable is template. So you can use any type with it. However, conversions from and to JSON should
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

MAIN_FUNCTION
```

</details>

This example showcases the cache variable.
Changing ```FILE1``` bool to ```false``` and then reconfiguring the project will rebuild the project,
because file2 will be used this time.

### Example 4

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catStatic = getCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    getCppExeDSC("Animal-Static").privateLibraries(&catStatic).getSourceTarget().sourceFiles("main.cpp");

    DSC<CppSourceTarget> &catShared = getCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    getCppExeDSC("Animal-Shared").privateLibraries(&catShared).getSourceTarget().sourceFiles("main.cpp");
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
You can change that by ```assign(CopyDLLToExeDirOnNTOs::NO)``` call to the
```prebuiltLinkOrArchiveTarget``` of the ```DSC```.

### Example 5

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catShared = getCppSharedDSC("Cat", true);
    catShared.getSourceTarget().sourceFiles("../Example4/Cat/src/Cat.cpp").publicIncludes("../Example4/Cat/header");

    DSC<CppSourceTarget> &animalShared = getCppExeDSC("Animal").privateLibraries(
        &catShared, PrebuiltDep{.requirementRpath = "-Wl,-R -Wl,'$ORIGIN' ", .defaultRpath = false});
    animalShared.getSourceTarget().sourceFiles("../Example4/main.cpp");

    getRoundZeroUpdateBTarget(
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
```PrebuiltDep::usageRequirementRpath``` should be used to specify the rpath in case,
another target depends on Animal, and, Cat is propagated to that target.
Generally, if ```defaultRpath``` is set to false, then, both should be set.

This example also showcases ```getRoundZeroUpdateBTarget```.
This takes a functor and the dependencies.
This functor will be called after the dependencies are updated.
So, we, here have directly injected in the central DAG.
```round``` variable specifies what round it is.
To support all the functionality HMake runs in 3 rounds.
In the first round,
it populates dependencies.
In the second round,
it generates smrules files.
In the third round, it builds the object files.
and does different things in different rounds.
```BTarget``` is the center of all this and will be extended in the last Example.
Instead of ```getRoundZeroUpdateBTarget```, ```outputDirectory``` could have been used in this case.

Because of Rpath, this is a Linux-only example.

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
            getCppTargetDSC_P("Cat" + str, "../Example4/Build/Cat" + str + "/", targetType, true, "CAT_EXPORT");
        cat.getSourceTarget().interfaceIncludes("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = getCppTargetDSC("Dog" + str, targetType, true, "DOG_EXPORT");
        dog.publicLibraries(&cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppSourceTarget> &dog2 = getCppTargetDSC("Dog2" + str, targetType, true, "DOG2_EXPORT");
        dog2.privateLibraries(&cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

        DSC<CppSourceTarget> &app = getCppExeDSC("App" + str);
        app.getLinkOrArchiveTarget().setOutputName("app");
        app.privateLibraries(&dog).getSourceTarget().sourceFiles("main.cpp");

        DSC<CppSourceTarget> &app2 = getCppExeDSC("App2" + str);
        app2.getLinkOrArchiveTarget().setOutputName("app");
        app2.privateLibraries(&dog2).getSourceTarget().sourceFiles("main2.cpp");
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
then this dependency will be propagated above up to the Shared-Library or Exe.

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp", "std.cpp");
    getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp").assignStandardIncludesToPublicHUDirectories();
}

MAIN_FUNCTION
```

</details>

This example showcases the HMake Modules Support.
In ```round == 1```, i.e. in the second-last / second round,
HMake generates the .smrule file for all the module-files and the header-units,
if any are being included by these module-files.
This .smrule file is specified by the compiler according to the P1689R5 paper.
In ```round==0```, HMake also determines the dependencies between different modules,
and then in round 3, will build them accordingly.
```app2``` marks all the ```requirementIncludes``` for which ```isStandard = true```
as header-unit-includes by calling the function ```assignStandardIncludesToPublicHUDirectories```.
Any directory that has header-units should be marked by at least one and only one
target as hu-include(header-unit-include) in a target or its dependencies.
So, HMake can decide what target to associate with these header-units from that directory.
Modules from a ```CppSourceTarget``` can use modules from a dependency ```CppSourceTarget```,
while at the same time being compiled by a different compile-command.

```CppSourceTarget``` member functions ```privateHUDirectories```,
```publicHUIncludes``` and ```privateHUIncludes```
are used for registering an include-directory for header-units.
The reason for ```privateHUDirectories``` besides
```publicHUIncludes``` and ```privateHUIncludes```
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
    getCppExeDSC("app").getSourceTarget().moduleDirectories("Mod_Src/");
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
    t.moduleDirectoriesRE("src/" + str + "/", ".*cpp")
        .privateHUDirectories("src/" + str)
        .publicHUDirectories("include/" + str);

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &config)
{
    config.compilerFeatures.privateIncludes("include");

    DSC<CppSourceTarget> &stdhu = config.getCppObjectDSC("stdhu");

    stdhu.getSourceTargetPointer()->assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &lib4 = config.getCppTargetDSC("lib4", config.targetType);
    DSC<CppSourceTarget> &lib3 = config.getCppTargetDSC("lib3", config.targetType).publicLibraries(&lib4);
    DSC<CppSourceTarget> &lib2 =
        config.getCppTargetDSC("lib2", config.targetType).publicLibraries(&stdhu).privateLibraries(&lib3);
    DSC<CppSourceTarget> &lib1 = config.getCppTargetDSC("lib1", config.targetType).publicLibraries(&lib2);
    DSC<CppSourceTarget> &app = config.getCppExeDSC("app").privateLibraries(&lib1);

    initializeTargets(lib1, lib2, lib3, lib4, app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    Configuration &static_ = getConfiguration("static");
    static_.assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    configurationSpecification(static_);

    Configuration &object = getConfiguration("object");
    object.assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_OBJECT);
    configurationSpecification(object);
}

MAIN_FUNCTION
```

</details>

This example showcases the usage of ```privateHUDirectories```.
In this example, if we had used ```HU_INCLUDES``` functions instead,
then this would have been a configuration error.
Because twp targets would have the same header-unit-include,
HMake won't have been able to decide which target to associate with the header-units.
Using ```privateHUDirectories``` ensure that header-units from ```include/lib1/```
and ```src/lib1/``` are linked with lib1 and so on.

### Example 10

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app")
        .getSourceTarget()
        .publicHUIncludes("3rd_party/olcPixelGameEngine")
        .R_moduleDirectories("modules/", "src/")
        .assign(CxxSTD::V_LATEST);
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
                if (linkOrArchiveTarget->evaluate(TargetType::EXECUTABLE))
                {
                    realBTarget.addDependency(*linkOrArchiveTarget);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->evaluate(TargetType::EXECUTABLE))
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
                if (linkOrArchiveTarget->evaluate(TargetType::EXECUTABLE))
                {
                    sizeSize += file_size(linkOrArchiveTarget->getActualOutputPath());
                    std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                          sizeDirPath + linkOrArchiveTarget->actualOutputName,
                                          std::filesystem::copy_options::overwrite_existing);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->evaluate(TargetType::EXECUTABLE))
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
    DSC<CppSourceTarget> &stdhu = configuration.getCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &fmt = configuration.getCppStaticDSC("fmt").publicLibraries(&stdhu);
    fmt.getSourceTarget().moduleFiles("fmt/src/format.cc", "fmt/src/os.cc").publicHUIncludes("fmt/include");

    configuration.markArchivePoint();

    DSC<CppSourceTarget> &hconfigure = configuration.getCppStaticDSC("hconfigure").publicLibraries(&fmt);
    hconfigure.getSourceTarget()
        .moduleDirectories("hconfigure/src")
        .publicHUIncludes("hconfigure/header", "cxxopts/include", "json/include", "rapidjson/include");

    DSC<CppSourceTarget> &hhelper = configuration.getCppExeDSC("hhelper").privateLibraries(&hconfigure, &stdhu);
    hhelper.getSourceTarget()
        .moduleFiles("hhelper/src/main.cpp")
        .privateCompileDefinition("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header"))
        .privateCompileDefinition("JSON_HEADER", addEscapedQuotes(srcDir + "json/include"))
        .privateCompileDefinition("RAPIDJSON_HEADER", addEscapedQuotes(srcDir + "rapidjson/include"))
        .privateCompileDefinition("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include"))
        .privateCompileDefinition(
            "HCONFIGURE_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .privateCompileDefinition(
            "HCONFIGURE_STATIC_LIB_PATH",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).string()))
        .privateCompileDefinition(
            "FMT_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .privateCompileDefinition(
            "FMT_STATIC_LIB_PATH", addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).string()));

    DSC<CppSourceTarget> &hbuild = configuration.getCppExeDSC("hbuild").privateLibraries(&hconfigure, &stdhu);
    hbuild.getSourceTarget().moduleFiles("hbuild/src/main.cpp");

    DSC<CppSourceTarget> &hmakeHelper =
        configuration.getCppExeDSC("HMakeHelper").privateLibraries(&hconfigure, &stdhu);
    hmakeHelper.getSourceTarget().moduleFiles("hmake.cpp").privateCompileDefinition("EXE");

    DSC<CppSourceTarget> &exp = configuration.getCppExeDSC("exp").privateLibraries(&stdhu);
    exp.getSourceTarget().moduleFiles("main.cpp").privateIncludes("rapidjson/include");
}

void buildSpecification()
{
    Configuration &releaseSpeed = getConfiguration("RSpeed");
    CxxSTD cxxStd = releaseSpeed.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    releaseSpeed.assign(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::RELEASE);
    // releaseSpeed.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS", "1");

    Configuration &releaseSize = getConfiguration("RSize");
    releaseSize.assign(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);

    if (selectiveConfigurationSpecification(&configurationSpecification))
    {
        targets<SizeDifference>.emplace("Size-Difference", releaseSize, releaseSpeed);
    }
}

MAIN_FUNCTION
```

</details>

This example showcases HMake's extensibility and support for drop-in replacement of
header-files with header-units.
```getCppObjectDSC``` is used for a target for which there is no link-target i.e. header-only cpp-target.
In this case, header-unit only.

```markArchivePoint``` function is a WIP. It will be used to specify that fmt, json, and stdhu are never
meant to be changed. So, hbuild can ignore checking object-files of these for a rebuild.

This function call ```selectiveConfigurationSpecification(&configurationSpecification)```
means that if hbuild isn't executed in a configuration dir,
then that configuration won't even be specified.
Thus, a project can have multiple configurations, and rebuild speed won't be affected.
HMake also has a selectiveBuild feature which means that for an app and its dependency, if hbuild
executed in the app, then the app and dependency both would be built, but if executed in dependency, then
only the dependency will be built.

```TranslateInclude::YES``` will result in treating ```#include``` as ```import```.
In the MSVC case, it is ```/translateInclude``` flag specification.
```TreatModulesAsSource::YES``` causes the calls to ```moduleFiles``` and ```moduleDirectories```
routed to ```sourceFiles``` and ```SOURCE_DIRECTORIES```.
Changing this to ```TreatModulesAsSource::NO``` will cause drop-in replacement to header-units
of standard headers and headers coming from hconfigure directory but not of all
headers. The reason is ```/translateInclude``` flag checks for ```header-units.json``` file
in the header-directory and mentions only those in smulres that are mentioned in the file.
Because this file is not present in external libs include-dirs,
```USE_HEADER_UNITS``` macro is defined to consume all header-files as header-units.

You can use hwrite to write the ```header-units.json``` file.
Execute the hwrite in the respective directory, and pass it the extension.
e.g. ```hwrite .hpp```.

Please notice that HMake does not cache ```privateHUDirectories``` call.
Neither it stores the contents of ```header-units.json``` file in cache.
Both of which can impact the header-units to be built.
So, if you change these when you have already built the project,
nothing will happen.
Because, header-units are discovered during the build-process,
and as no file is compiled, these changes won't be reflected.
To reflect these changes,
delete the dependents targets build-dir.
By dependents, I mean those targets that can import a header-unit from such a directory.
Now, when such targets will be rebuilt,
the changes will be reflected.
I think, once set, you won't change it often.
But, it might be cached later on as-well.
So, if you change it all the dependent targets are completely rebuilt.

Also notice, that HMake does not check the .ifc file at all.
If you externally update the .ifc or delete it,
Please delete the corresponding .o file as well.
This is for optimization.
I think this is an acceptable tradeoff.
.smrules file is only checked if .o file is newer than source-files.
So, if you edit it externally, delete the corresponding .o file as-well.

Thus, for drop-in replacement, the compiler needs to support P1689R5 and a ```TranslateInclude::YES```
flag.
GCC14 supports P1689R5 but has not been released yet.
Clang isn't supported at the moment.
As per the clang.jam (the b2 build-system Clang file),
Clang uses GCC command-line on Linux and MSVC command-line on Windows.
So, you can masquerade it as GCC or MSVC, however, clang.jam also
sets the ```--target=``` flags on Windows which are not set by the HMake.
Also ```clang-scan-deps```, a different tool than clang supports P1689R5,
I successfully tested with locally compiled ```Clang 18.1.0```,
but the scanner does not support header-units at the moment.

This leaves us with ```SizeDifference```.
This is used to determine the difference of sizes between Speed and Size builds.
Only Executable targets are compared.
The constructor set the dependencies. In round 0, then the ```updateBTarget``` will be called only
after building the exe deps.
SizeDifference most probably will be promoted to the HMake code-base, because, a feature like
this might be interesting for other users as-well.
Any feature in the wild that is needed by more than one project (preferably 5),
will be integrated into the HMake.
Extension points will be provided as the need arises.

# Future Direction

I want to propose a new unconventional approach to code compilation.
Instead of compiler and linker being executables, these can be shared-libs.
The build-system will load these libs and call the respective functions.
So the process setup cost is evaded.
Also, the API can have functions that are to be called by the compiler or linker, whenever,
a new file is to be read from the disk.
This way, a header-file or .ifc file can be preserved for longer usage.
The same file can be used by multiple processes.
These are low-hanging fruits, but a complete API can also be designed.
So, the compiler does not have the driver at all.
The build-system itself acts as the driver.
And when invoked, the compiler jumps straight into the act without first doing the argument parsing.
If two compilers are doing the same thing conceptually, then one could be a drop-in
replacement of the other.

I am also trying to expand the build algorithm, so, it can have
smart thread-allocation for different tasks based on priority and quota.
TODOs for these are mentioned in ```BasicTarget.hpp```. Help would be appreciated.

Also, IDE/Editor tools need to parse the source-code for intelligent suggestions.  
Can this be cached and supplied to the compiler backend, when the user builds the code,
instead of doing full-build of the file?

HMake 1.0 will only be released when, a few of the mega projects like UE5, AOSP, Qt, etc. could be
supported.
So, the API can be considered idiomatic.
Also, the support for popular languages, and frameworks like Android, IOS, and,
the popular CI/CD etc. could be supported.
I am confident about releasing 1.0 in the next 1-3 years.
This depends, depends on reception.

If you maintain the build of a mega-project, want to add a futuristic feature or
have code that uses C++20 modules,
I am very interested in collaboration.
If you just need help related to any aspect of HMake or have any opinion to share,
Please feel free to approach me.

## Support

I have been working on this project, on and off, for close to two years.
Please consider donating.
Contact me here if you want to donate hassan.sajjad069@gmail.com
or you can donate to me through Patreon. Thanks.
