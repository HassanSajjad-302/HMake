# HMake

HMake is a new build system that does not invent a new DSL for its project build specification.
Currently, it only provides C++ build and C++ API.
Later on, build support for other programming languages will be added.
API in multiple other programming languages will be provided as well.
Tested on Windows 11 with MSVC 14.43.34808.
It does support Linux and GCC and Clang but those are untested atm.

You can skip to the C++ Examples for C++ project examples.
The following sections showcases low-level HMake architecture.
The C++ support is built on top of that.

## HMake Architecture

HMake is an extensible build-system.
class ```BTarget``` is the building block of the build-system.
HMake has a very simple algorithm.
You define all the targets and dependencies between them.
HMake then topologically sorts and informs you if there is any cycle.
Those with 0 dependencies are added to a list.
Then we start building the targets in this list.
As soon as we build a target, we decrement from all the dependents ```target.dependenciesSize```.
If the dependent target.dependenciesSize == 0,
i.e. no dependency of the target is left to be built, we add it to the
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
    b.addDependency<0>(a);
}

MAIN_FUNCTION
```

</details>


To extend the build-system, we need to derive from the ```BTarget```
and override ```updateBTarget``` function.
If you build this example, it will print "Hello\nWorld\n" during the build.
First ```a.updateBTarget``` runs and then ```b.updateBTarget``` runs.
Because we specified a dependency relationship between ```a``` and ```b```.
But, what is the ```round == 0``` and ```addDependency<0>```?
HMake does topological sorting and target updating 3 times in one execution.
This is to simplify C++20 modules support.
In ```round == 2```, we develop the compile command and link command of different targets
based on their dependencies.
This is what the CMake does.
While in ```round == 1``` we scan module source-files and header-units
and in ```round == 0```, we build these which is what Ninja does.

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
    b.addDependency<0>(a);
    a.addDependency<1>(b);
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
HMake is a fully multithreaded build-system.
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
    a.addDependency<0>(b);
    b.addDependency<0>(c);
    c.addDependency<0>(a);
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
```CppSourceTarget```, ```LOAT``` prints ```name```,
while ```SourceNode``` and ```SMFile``` prints ```node->filePath```.

### Example 5

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

constexpr unsigned short roundLocal = 1;
struct OurTarget : public BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if (round == roundLocal)
        {
            if (error)
            {
                fmt::print("Target {} runtime error.\n", name);
                realBTargets[roundLocal].exitStatus = EXIT_FAILURE;
            }
            if (realBTargets[roundLocal].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", name));
            }
        }
    }
};

OurTarget a("Hello"), b("World"), c("HMake"), d("CMake"), e("Ninja", true), f("XMake"), g("build2", true), h("Boost");
void buildSpecification()
{
    d.addDependency<roundLocal>(e);
    h.addDependency<roundLocal>(g);
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
            a->addDependency<0>(*c);
            b->addDependency<0>(*c);

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
Initially, only ```target2``` was the part of the DAG.

HMake supports dynamic targets as demonstrated.
These are an HMake speciality.
Not only you can add new edges in the DAG dynamically,
but also new nodes as well.
However, you have to take care of the following aspects:

1. You have to update the ```Builder::updateBTargetsSizeGoal``` variable with the
   additional number of targets whose ```updateBTarget``` will be called.
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
            b.addDependency<0>(c);
            c.addDependency<0>(b);
        }
    }
};

OurTarget target;

void buildSpecification()
{
    b.addDependency<0>(target);
    c.addDependency<0>(target);
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
      in the target's build dir or its parent/child build dir.
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
    - `setSelectiveBuild` is called to set the `selectiveBuild` flag based on dir rules.
- **Round 0**:
    - The `selectiveBuild` flag is used to decide if a target is built.

---

### 4. Make Directory

- If `makeDirectory = true`, the target's dir is created during configuration.
- If `makeDirectory = false`, no dir is created for the target.
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
#include "Configure.hpp"
#include <utility>

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
    c->addDependency<0>(*e);
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

## Nodes

<details>
<summary></summary>

Every file and dir-path is represented by `Node` class in HMake.
This class ensures that a file once checked for timestamp is not checked again.
This is a common feature in build-systems.

But HMake has an exclusive **game-changing and unprecedented** feature.

To retrieve the ```lastWriteTime``` for a file,
first we have to obtain the ```Node``` for it.
```Node::get...()``` functions are used for this purpose.
This ensures that we have one ```Node``` for one file by first
checking for the filepath existence in a ```set<Node>```.
Then we can use ```Node::lastWriteTime```.
```Node``` constructor sets the ```id``` for the ```Node```.
And assigns ```this``` to a pre-allocated ```vector<Node *>```
at the ```id``` index.
Before saving the config-cache or build-cache,
we save this ```vector<Node *>``` first.
This is saved as a JSON array of absolute file-paths.
On Windows, before creating a ```Node```, path is lowercased first.

Then, in next rebuilds or reconfigurations, we load this
nodes cache first and store it in the ```set<Node>``` and its pointers
in ```vector<Node *>```.
During the run, these data-structures will be extended if a new node is
found.

Thus, we assign a permanent number to a filesystem path,
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
in cache files,
or to compress or not to compress the cache files.
You can see these in the CMakeLists.txt file.

</details>

## TargetCache

<details>
<summary></summary>

```TargetCache``` class allows for storing cache at config-time and build-time.
Its constructor takes a unique name (generally same as ```BTarget::name```)
and at config-time creates an entry in both config-cache and build-cache
(if it did not exist before).
config-cache can only be written at config-time.
For example, for ```CppSourceTarget```,
HMake stores the node numbers for source-files, module-files
and include-dirs in the config-cache.
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

To run this, build the HMake project
and add the build-dir in path environement variable.
hhelper and hbuild will be used frequently while other binaries
are used occasionally.
CMake targets ```ConfigureHelper``` and ```BuildHelper``` can be used
to debug the hmake.cpp of the project itself.
htools command needs to be run once.
This detects all the installed tools.
Unlike few other tools, HMake does not
detect the tools installed every time you configure a project but
is instead done only once and the result is cached to
```C:\Program Files (x86)\HMake\toolsCache.json```
on Windows and ```/home/toolsCache.json``` in Linux.
Currently, it is just a stud.
Administrative permissions are needed on Windows.
Also, the latest Visual Studio needs to be installed as the latest
MSVC version is baked in.

Now, create build-dir in Example1 directory
and run hhelper twice and hbuild once.
hhelper will create the cache.json file.
cache.json file provides an opportunity to select a different toolset.
It has array indices to the arrays of different tools in toolsCache.json.
These tools must exist in toolsCache.json for HMake to work.
cache.json file also has the commands that
build will use to build ```configure``` and ```build``` executables.
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

A side note, current CMakeLists.txt builds with address sanitizer,
so you need to copy the respective dll
in cmake build-dir for debugging on Windows.

```getConfiguration``` creates a default ```Configuration```
with release name and ```ConfigType::RELEASE```.
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
```DSC<CppSourceTarget>```.
```DSC<CppSourceTarget>``` manages dependency specification as you will see
later on.
It has pointers to ```CppSourceTarget``` and ```LOAT```.
```getSourceTarget``` returns the ```CppSourceTarget``` pointer to which we add the source-files.
There are other ```get*``` functions available in ```Configuration```
class.
These functions preserve by emplacing the element in ```targets``` template variable.

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

void configurationSpecification(Configuration &configuration)
{
    configuration.getCppExeDSC("app")
        .getSourceTarget().sourceDirsRE(".", "file[1-4]\\.cpp|main\\.cpp");
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
    CppSourceTarget &app = config.getCppExeDSC("app").getSourceTarget();
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
    DSC<CppSourceTarget> &catStatic = config.getCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Static").privateDeps(catStatic).getSourceTarget().sourceFiles("main.cpp");

    DSC<CppSourceTarget> &catShared = config.getCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Shared").privateDeps(catShared).getSourceTarget().sourceFiles("main.cpp");
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
    auto makeApps = [&]() {
        const string str = config.targetType == TargetType::LIBRARY_STATIC ? "-Static" : "-Shared";

        DSC<CppSourceTarget> &cat =
            config.getCppTargetDSC_P("Cat" + str, "../Example4/Build/Release/Cat" + str + "/", true, "CAT_EXPORT");
        cat.getSourceTarget().interfaceIncludes("../Example4/Cat/header");

        DSC<CppSourceTarget> &dog = config.getCppTargetDSC("Dog" + str, true, "DOG_EXPORT");
        dog.publicDeps(cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppSourceTarget> &dog2 = config.getCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT");
        dog2.privateDeps(cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

        DSC<CppSourceTarget> &app = config.getCppExeDSC("App" + str);
        app.getLOAT().setOutputName("app");
        app.privateDeps(dog).getSourceTarget().sourceFiles("main.cpp");

        DSC<CppSourceTarget> &app2 = config.getCppExeDSC("App2" + str);
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

### Example 7

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp", "std.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration("modules");
    getConfiguration("hu");
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases the HMake Modules Support.
In ```round == 1```, i.e. in the second-last / second round,
HMake generates the .smrule file for all the module-files and the header-units,
if any are being included by these module-files.
This .smrule file is specified by the compiler according to the P1689R5 paper.
In ```round==1```, HMake also determines the dependencies between different modules,
and then in ```round == 0```, will build them accordingly.

Any dir that has header-units should be marked by at least one and only one
target as hu-include(header-unit-include) in a target or its dependencies.
So, HMake can decide what target to associate with these header-units from that dir.
```std``` target marks all standard include-dirs as header-unit directories.
Modules from a ```CppSourceTarget``` can depend on modules from that target or from
it dependencies ```CppSourceTarget```.
But it can depend on an interface partition only from its own files
and not from its dependency targets.

```CppSourceTarget``` member functions ```privateHUDirs```,
```publicHUIncludes``` and ```privateHUIncludes```
are used for registering an include-dir for header-units.
The reason for ```privateHUDirs``` besides
```publicHUIncludes``` and ```privateHUIncludes```
is that sometimes general include-dirs are less specialized
while header-unit-include-dirs are more specialized.
That is showcased in Example 9.

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
    getConfiguration();
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

template <typename... T> void initializeTargets(DSC<CppSourceTarget> *target, T... targets)
{
    CppSourceTarget &t = target->getSourceTarget();
    string str = removeDashCppFromName(getLastNameAfterSlash(t.name));
    t.moduleDirsRE("src/" + str + "/", ".*cpp")
        .privateHUDirs("src/" + str)
        .publicHUDirs("include/" + str);

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSuourceTarget().interfaceIncludes("include");
    DSC<CppSourceTarget> &lib4 = config.getCppTargetDSC("lib4");
    DSC<CppSourceTarget> &lib3 = config.getCppTargetDSC("lib3").publicDeps(lib4);
    DSC<CppSourceTarget> &lib2 = config.getCppTargetDSC("lib2").privateDeps(lib3);
    DSC<CppSourceTarget> &lib1 = config.getCppTargetDSC("lib1").publicDeps(lib2);
    DSC<CppSourceTarget> &app = config.getCppExeDSC("app").privateDeps(lib1);

    initializeTargets(&lib1, &lib2, &lib3, &lib4, &app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    getConfiguration("static").assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

This example showcases the usage of ```privateHUDirs```.
In this example, if we had used ```huIncludes``` functions instead,
then this would have been a configuration error.
Because twp targets would have the same header-unit-include,
HMake won't have been able to decide which target to associate with the header-units.
Using ```privateHUDirs``` ensure that header-units from ```include/lib1/```
and ```src/lib1/``` are linked with lib1 and so on.

### Example 10

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppSourceTarget> &libB = config.getCppStaticDSC("libB");
    libB.getSourceTarget().moduleFiles("B.cpp").headerUnits("B.hpp").publicIncludes(
        path(srcNode->filePath).string());

    config.getCppExeDSC("appA")
        .privateDeps(libB)
        .getSourceTarget()
        .moduleFiles("A.cpp")
        .headerUnits("A.hpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

If in a same directory 2 header-units belong to 2 different target,
then the only option is to specify them manually in the ```hmake.cpp``` file.

# Future Direction

HMake 1.0 will only be released when, a few of the mega projects like UE5, AOSP, Qt, etc. could be
supported.
So, the API can be considered idiomatic.
Also, the support for popular languages, and frameworks like Android, IOS, and,
the popular CI/CD etc. could be supported.
I am confident about releasing 1.0 in the next 1-3 years.
This depends, depends on reception.

If you maintain the build of a mega-project,
and want to benefit from the faster compilation with C++20 header-units,
I am very interested in collaboration.
If you just need help related to any aspect of HMake or have any opinion to share,
Please feel free to approach me.

## Support

I have been working on this project, for close to three years.
Please consider donating.
Contact me here if you want to donate hassan.sajjad069@gmail.com,
or you can donate to me through Patreon. Thanks.

