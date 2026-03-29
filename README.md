# HMake

HMake is a C++ build system with a pure C++ API — no DSL,
no domain-specific configuration language.
You describe your build in C++, and HMake executes it.
Support for additional programming languages and API bindings is planned.

HMake features a novel build algorithm with dynamic nodes, dynamic edges, and advanced dependency specification.
Its core is approximately 17,000 lines of C++ — significantly smaller than CMake + Ninja combined.

## Architecture

HMake separates concerns cleanly into two layers:

**Core layer** — `BTarget`, `Builder`, `Node`, `TargetCache`. These classes are general-purpose and have no knowledge of
C++. Any build system can be built on top of them.

**C++ layer** — `CppTarget`, `CppSrc`, `CppMod`, `LOAT`. These implement C++ compilation on top of the core. The core
has zero references to these classes, which demonstrates how cleanly extensible the core API is.

---

## Core Concepts

### BTarget

`BTarget` is the fundamental building block of HMake. Every unit of work in your build — compiling a file, linking a
library, running a test — is a `BTarget`. You extend it by deriving from it and overriding its virtual functions.

Each `BTarget` internally holds two `RealBTarget` instances (`realBTargets[0]` and `realBTargets[1]`), one for each
build round. Dependencies and build logic are declared per-round using `addDep<0>()` and `addDep<1>()`.

**Key virtual functions:**

| Function                      | Round   | Purpose                                                                               |
|-------------------------------|---------|---------------------------------------------------------------------------------------|
| `completeRoundOne()`          | Round 1 | Runs when all round-1 dependencies are satisfied                                      |
| `isEventRegistered(Builder&)` | Round 0 | Returns `true` if it launched an async process, `false` if it completed synchronously |
| `isEventCompleted(...)`       | Round 0 | Called when a launched child process produces IPC output or exits                     |
| `getPrintName()`              | Both    | Returns the name used in cycle/error messages                                         |

### Node

Every file and directory path in HMake is represented by a `Node`. A `Node` assigns a permanent integer ID to each
filesystem path. This ID remains stable across rebuilds and reconfigurations, allowing the build cache and config cache
to store IDs rather than full paths. The result is dramatically smaller caches — estimated under 10 MB for a project the
size of UE5 — and near-instant build startup even for very large projects.

Before round 0, HMake calls `Node::performSystemCheck` in parallel (using io_uring where available) for all nodes marked
`toBeChecked`. This bulk, parallel stat eliminates redundant filesystem queries and contributes to HMake's rebuild speed
being at least 2x faster than Ninja.

### TargetCache

`TargetCache` manages persistent data across configure-time and build-time. It takes a unique name (typically the same
as `BTarget::name`) and creates entries in both the config cache and build cache. Config cache entries are written at
configure-time and read at build-time without re-running configuration. For example, `CppTarget` uses `TargetCache` to
store normalized, deduplicated node IDs for source files, module files, header units, headers, and include directories.

### Builder

`Builder` drives the build loop. It maintains `updateBTargets`, the queue of targets that are ready to execute (those
whose `dependenciesSize` has reached zero). `dependenciesSize` holds the number for the dependencies of the target.
After sorting, those targets whose `dependenciesSize == 0` are added to the `updateBTargets` list. As each target in
this list completes, HMake decrements `dependenciesSize` on its dependents, and any dependent that reaches zero is
added in `updateBTargets` list. Build finishes once all targets are completed.

---

Currently, the build-system is set up only for my custom fork and only for Linux.
This means that you will have to build my fork first.
Better tool detection and easier setup will be a priority moving forward.

Let's do some examples

## Quick Start

**Option A — Fresh clone:**

```bash
git clone --depth=1 --branch main https://github.com/HassanSajjad-302/llvm-project.git
```

**Option B — Already have the repo cloned:**

```bash
cd llvm-project
git remote add hassan https://github.com/HassanSajjad-302/llvm-project.git
git fetch --depth=1 hassan main
git checkout hassan/main
```

**Build Clang:**

Name of the build-dir must be my-fork. It is a hard reference in `HMake/Projects/LLVM/hmake.cpp`.

```bash
cd llvm-project
mkdir my-fork && cd my-fork
cmake ../llvm \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_TARGETS_TO_BUILD=X86
ninja clang
cd ../..
```

On Linux, edit `ToolsCache::detectToolsAndInitialize` in ToolsCache.cpp — Point to the absolute path of the clang
binary:
`llvm-project/my-fork/bin/clang`.
On Windows, edit last line in `CppCompilerFeatures::initialize`.

**Clone and build HMake:**

```bash
git clone https://github.com/HassanSajjad-302/HMake.git
cd HMake
mkdir build && cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ../..
```

**Add HMake to PATH:**

```bash
export PATH=$PATH:/path/to/HMake/build
```

**Detect and cache installed tools — run once, not per project:**

On Windows you would need administrative permissions for that.

```bash
htools
```

**Build an example:**

You can build any example by running `hhelper`, `hhelper`, `hbuild`

1) Run hhelper — generates `cache.json`:
2) Run hhelper — compiles `configure` and `build` executables, then runs `configure`:
3) Run hbuild - runs `build` executable. Produces `{buildDir}/release/app`:

```bash
cd HMake/Examples/Example1
mkdir build && cd build
hhelper
hhelper
hbuild
```

<details>
<summary> Step-by-Step Explanation </summary>

Unlike few other build-systems, HMake does not
detect the tools installed every time you configure a project but
is instead done only when you run ```htools``` and the result is cached to
```C:\Program Files (x86)\HMake\toolsCache.json```
on Windows and ```/home/toolsCache.json``` in Linux.
Currently, it is just a stud.
HMake is more ```make``` like in this aspect:).
It writes in ```toolsCache.json```
whatever is specified in ```ToolsCache::detectToolsAndInitialize```.

First hhelper will create the cache.json file.
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

CMakeLists.txt builds with address sanitizer,
so you need to copy the respective dll
in cmake build-dir for debugging on Windows.
It has targets for all the Examples.
You need to run these targets in the respective ```Build``` dir.
E.g. for `Example1`, there is `Example1Build` and `Example1Config`.
To replicate what `configure` does, run `Example1Config` in `Examples/Example1/Build`.
Similarly, to replicate the `build`, run `Example1Build` in `Examples/Example1/Build`.

</details>

Any of the following example can be built by creating a build-dir in the example directory.
These examples are same to those in `Example/` directory.

## HMake Architecture Examples

### Example 1 — Basic dependency ordering

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

    void completeRoundOne() override
    {
        printMessage(FORMAT("{}\n", message));
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    b->addDep<1>(*a);
}

MAIN_FUNCTION
```

</details>

Output: `Hello\nWorld\n`. Because `b` depends on `a` in round 1, `a.completeRoundOne()` always runs before
`b.completeRoundOne()`.

Let's clarify this with more examples.

### Example 2 — Inverting dependency order between rounds

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

    void completeRoundOne() override
    {
        printMessage(FORMAT("{}\n", message));
    }

    bool isEventRegistered(Builder &buildeer) override
    {
        printMessage(FORMAT("{}\n", message));
        return false;
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");

    b->addDep<0>(*a);
    a->addDep<1>(*b);
}

MAIN_FUNCTION
```

</details>

Now, this example will print ```World\nHello\nHello\nWorld\n```.
As we inverted the dependency relationship for round 1 compared to round 0.
```BTarget``` constructor initializes ```realBTargets```
which is ```array<RealBTarget, 2>```.
So, by declaring 1 ```BTarget```, you declare 2 ```RealBTargets```.
```addDep<0>``` will add dependency for round0 while
```addDep<1>``` will add dependency for round1.

`isEventRegistered` should return `true` if it launched a subprocess via `run.startAsyncProcess`, and `false` if it
completed synchronously. When a subprocess writes an IPC message to stdout, or exits, HMake calls `isEventCompleted`. An
empty `message` parameter means the process exited; `run.output` contains its full output.

IPC messages are distinguished from ordinary stdout by being followed by the message size and `P2978::delimiter`. This
is the same mechanism used by `CppSrc` and `CppMod` to implement C++20 modules and header-unit support.

### Example 4 — Cycle detection

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

    void completeRoundOne() override
    {
    }

    string getPrintName() const override
    {
        return message;
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Cat1");
    OurTarget *b = new OurTarget("Cat2");
    OurTarget *c = new OurTarget("Cat3");
    a->addDep<0>(*b);
    b->addDep<0>(*c);
    c->addDep<0>(*a);
}

MAIN_FUNCTION
```

</details>

This will print the following.

```
Cycle found: Cat1 -> Cat2 -> Cat3 -> Cat1
```

Without `getPrintName()` overridden, HMake would print `BTarget 0 -> BTarget 1 -> BTarget 2 -> BTarget 0`.

### Example 5 — Error propagation

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct OurTarget : BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, const bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
        if (error)
        {
            printMessage(FORMAT("Target {} runtime error.\n", name));
            realBTargets[0].exitStatus = EXIT_FAILURE;
        }

        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            printMessage(FORMAT("{}\n", name));
        }
        return false;
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
    d->addDep<0>(*e);
    h->addDep<0>(*g);
}

MAIN_FUNCTION
```

</details>

This example demonstrates HMake error handling.
If ```updateBTarget``` set ```RealBTarget::exitStatus```
to anything but ```EXIT_SUCCESS```, then HMake will set the ```RealBTarget::exitStatus```
of all the dependent targets to```EXIT_FAILURE```.

This way target can learn the execution status of its dependents
and also communicate theirs to their dependents.
If in round1, ```RealBTarget::exitStatus``` of any one of the targets is
not equal to ```EXIT_SUCCESS```,
then HMake will exit early and not execute the round0.

### Example 6 — Dynamic targets

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

    bool isEventRegistered(Builder &builder) override
    {
        for (unsigned short i = low; i < high; ++i)
        {
            printMessage(FORMAT("{} ", i));
        }
        return false;
    }
};

OurTarget *a, *b, *c;

struct OurTarget2 : BTarget
{
    bool isEventRegistered(Builder &builder) override
    {
        a = new OurTarget(10, 40);
        b = new OurTarget(50, 80);
        c = new OurTarget(800, 1000);
        a->addDep<0>(*c);
        b->addDep<0>(*c);

        builder.updateBTargets.emplace(&c->realBTargets[0]);
        builder.updateBTargetsSizeGoal += 3;
        return false;
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
then it will print ```10``` to ```40``` and ```50``` to ```80``` in no-order.
This is because of targets ```OurTarget *a, *b, *c;``` and the dependency relationship
between these targets.
These targets were not part of the DAG but instead dynamically added.
Initially, only ```target2``` was the part of the DAG.

HMake supports dynamic targets in round0 as demonstrated.
These are an HMake speciality.
Not only you can add new edges in the DAG dynamically,
but also new nodes as well.
However, you have to take care of the following aspects:

1. You have to update the ```Builder::updateBTargetsSizeGoal``` variable with the
   additional number of times ```isEventRegistered``` will be called.
2. If any newly added targets do not have any dependency
   then it must be added in ```updateBTargets``` list like we added ```c``` target.
3. Besides new targets, we can also modify the dependencies of older targets.
   But these targets ```dependenciesSize``` should not be zero.
   Because if the target ```dependenciesSize``` becomes zero,
   it is added to the ```updateBTargets``` list.
   HMake does not allow removing or modifying elements in this list.

### Example 7 — Dynamic edges with cycle detection

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    bool isEventRegistered(Builder &builder) override
    {
        b->addDep<0>(*c);
        c->addDep<0>(*b);
        return false;
    }
};

void buildSpecification()
{
    b = new BTarget();
    c = new BTarget();
    OurTarget *target = new OurTarget();
    b->addDep<0>(*target);
    c->addDep<0>(*target);
}

MAIN_FUNCTION
```

</details>


Adding edges dynamically that form a cycle is detected and reported the same way as static cycles.

```
Cycle found: BTarget 0 -> BTarget 1 -> BTarget 0
```

### Example 8 — Breaking dynamic target rules

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *a;

struct OurTarget : BTarget
{
    bool isEventRegistered(Builder &builder) override
    {
        a = new BTarget();
        ++builder.updateBTargetsSizeGoal;
        // builder.updateBTargets.emplace(&a->realBTargets[0]);
        
        return false;
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

### Example 10 — Child Process IPC

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

struct Process : BTarget
{
    static constexpr const char *cmd = "./a.out";

    bool isEventRegistered(Builder &builder) override
    {
        // Pass true so HMake keeps the child's stdin pipe open — this process
        // sends a message to the build-system and then waits for a reply before
        // continuing.
        run.startAsyncProcess(cmd, builder, this, /*keepStdinOpen=*/true);
        // launched async process. would have returned false otherwise (e.g. a module-file was already updated).
        // HMake will call isEventCompleted when process exits or there is message for build-system.
        return true;
    }

    bool firstReceived = false;

    bool isEventCompleted(Builder &builder, const string_view message) override
    {
        if (message.empty())
        {
            // Empty message means the child process has exited. exitStatus reflects the exitStatus of child process.
            const bool ok = run.exitStatus == EXIT_SUCCESS;

            string out = getColorCode(ok ? ColorIndex::light_green : ColorIndex::red);
            out += ok ? "./a.out finished successfully:\n" : "./a.out failed:\n";
            out += getColorCode(ColorIndex::reset);
            out += run.output;
            printMessage(out);

            return false; // stop waiting; we are done with this target
        }

        // The child sent an IPC message to the build-system (distinguished from ordinary stdout by the delimiter
        // protocol).  Print it, then on the first message reply with the module name it requested.

        string out = getColorCode(ColorIndex::orange);
        out += "./a.out → build-system message:\n";
        out += getColorCode(ColorIndex::reset);
        out += string{message};
        printMessage(out);

        if (!firstReceived)
        {
            // Reply: send the module name the child asked for.
            const string reply = "std\n";
            if (write(run.writePipe, reply.data(), reply.size()) == -1)
            {
                printMessage("Warning: failed to write to child stdin\n");
            }
            firstReceived = true;
        }

        return true; // keep listening; more messages or exit event may follow
    }
};

void buildSpecification()
{
    // Any BTarget must have application life-time.
    new Process();
}

MAIN_FUNCTION
```

</details>



<details>
<summary>main.cpp</summary>

```cpp
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

// ---------------------------------------------------------------------------
// P2978 IPC protocol helpers
//
// HMake distinguishes build-system messages from ordinary stdout by checking for
// a fixed 32-byte delimiter after each message.  The format of one message is:
//
//   <payload bytes>  <uint32 payload-length (LE)>  <32-byte delimiter>
//
// HMake strips message from the child's normal stdout which is run.output in
// the build-system
// ---------------------------------------------------------------------------

namespace ipc
{

// The delimiter must match the one compiled into HMake exactly.
inline constexpr char delimiter[] =
    "DELIMITER"
    "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
    "DELIMITER"; // 32 bytes total

static void writeAll(int fd, const char *buf, std::size_t len)
{
    std::size_t written = 0;
    while (written < len)
    {
        const ssize_t n = ::write(fd, buf + written, len - written);
        if (n == -1)
        {
            if (errno == EINTR)
                continue; // interrupted by signal — retry
            std::perror("ipc::writeAll");
            std::exit(EXIT_FAILURE);
        }
        written += static_cast<std::size_t>(n);
    }
}

// Send a single IPC message to the build-system.
// The payload is an arbitrary string; the build-system receives it verbatim
// inside isEventCompleted(builder, message).
void send(const std::string &payload)
{
    // Frame layout: payload | uint32 length | delimiter
    const auto len = static_cast<uint32_t>(payload.size());

    std::string frame;
    frame.reserve(payload.size() + sizeof(uint32_t) + sizeof(delimiter) - 1);
    frame.append(payload);
    frame.append(reinterpret_cast<const char *>(&len), sizeof(len)); // little-endian on x86/ARM
    frame.append(delimiter, sizeof(delimiter) - 1);                  // exclude null terminator

    writeAll(STDOUT_FILENO, frame.data(), frame.size());
}

} // namespace ipc

int main()
{
    // Tell the build-system which module we need.  HMake will call
    // isEventCompleted() with this string and can write a reply to our stdin.
    ipc::send("First message to build-system: this module depends on 'std'. Please provide it.\n");

    // Wait for the build-system's reply (the module name).
    std::string module;
    if (!(std::cin >> module))
    {
        std::cerr << "main: failed to read module name from build-system\n";
        return EXIT_FAILURE;
    }

    std::cout << "Hello World\n";
    std::cout << "Module received: " << module << "\nYey\n";

    // Optionally send a second message — demonstrates multiple IPC rounds.
    ipc::send("Final message to build-system: compilation finished.\n");

    return EXIT_SUCCESS;
}
```

</details>

This example is only for Linux currently.
Before running `hbuild`, compile `main.cpp` to `a.out` in the build directory:

```bash
clang++ main.cpp -o a.out
```

This prints the following output:

```
./a.out → build-system message:
First message to build-system: this module depends on 'std'. Please provide it.
./a.out → build-system message:
Final message to build-system: compilation finished.
./a.out finished successfully:
Hello World
Module received: std
Yey
```

The child process and build-system communicate over two channels simultaneously: ordinary stdout for human-readable
output (captured in `run.output` and printed after exit), and the P2978 IPC protocol for structured messages that the
build-system intercepts and routes to `isEventCompleted`. This is the same mechanism used internally by `CppMod` to
implement C++20 module and header-unit support.

## Examples: C++ Build

### Example 1 — Minimal executable

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


`getConfiguration()` creates a default `Configuration` named `release` with `ConfigType::RELEASE`.
`CALL_CONFIGURATION_SPECIFICATION` ensures `configurationSpecification` is only invoked when `hbuild` is executed in the
build directory or a matching configuration subdirectory. This allows a multi-configuration project to build only the
active configuration without running the others.

`getCppExeDSC` returns a `DSC<CppTarget>` (Dependency Specification Container). `getSourceTarget()` returns the
`CppTarget` to which source files, include directories, and module files are attached.

Every `Configuration` creates a `stdCppTarget` by default, which carries the standard include directories from
`toolsCache.json`. All targets created via `get*` functions receive this as a private dependency automatically.

### Example 2 — Multiple configurations and source filtering

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


Each `getConfiguration` call creates a named configuration subdirectory. `assign()` sets build features on the
configuration. The full list of available features (optimization level, LTO, RTTI, exceptions, sanitizers, etc.) is in
`Features.hpp`, modeled on the Boost.Build feature system.

`sourceDirsRE` accepts a regex to filter files; `sourceDirs` defaults the regex to `.*`; `rSourceDirs` uses a recursive
directory iterator.

### Example 3 — Cache variables

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

`CacheVariable` persists a typed value in `cache.hmake`. Edit the value and re-run configure to change which branch is
taken without modifying source. Any type with nlohmann/json serialization support can be used.

### Example 4 — Static and shared libraries

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


The second argument `true` enables automatic export macro handling: HMake emits the appropriate compile definition for a
static vs. shared build and propagates it to dependents. The third argument overrides the default macro name (which
would otherwise be `Cat-Static_EXPORT`).

On Windows, HMake copies shared library dependencies to the executable directory by default. Disable with
`config.assign(CopyDLLToExeDirOnNTOs::NO)`.

### Example 6 — Prebuilt libraries and dependency propagation

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


`DSC` correctly handles transitive static library dependencies. If a static library depends on another static library,
`DSC` propagates that dependency up the chain until it reaches a shared library or executable, and ensures the link
order is correct for linkers that require it.

`getCppTargetDSC_P` accepts an output directory `Node*`, allowing consumption of a prebuilt library from another build
tree. Pass `nullptr` at build time (only needed at configure time. why do an extra `Node::getNodeNonNormalized` function
call).

### Example 7 — C++20 modules and header units

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


HMake has first-class support for C++20 modules and header units, including the ability to compile them without a prior
scanning step. It is currently the only build system with this capability. The approach is described
in [ISO paper P2978](https://htmlpreview.github.io/?https://github.com/HassanSajjad-302/iso-papers/blob/main/generated/my-paper.html)
and requires [this Clang fork](https://github.com/llvm/llvm-project/pull/147682).

HMake also supports **Big Header Units**: with a single configuration flag, all header files for a target can be
compiled as individual header units, or merged into one large composite header unit for faster incremental builds.

To enable modules for a configuration:

```cpp
getConfiguration("modules").assign(IsCppMod::YES);
```

All targets in that configuration will be compiled using the IPC-based module protocol. Setting `IsCppMod::YES` is
required to use `moduleFiles()`, `moduleFilesRE()`, and related APIs. In `IsCppMod::NO` mode, header-unit API calls fall
back to treating files as ordinary headers, enabling source-compatible support across compilers.

`StdAsHeaderUnit::NO` prevents the standard library target from being configured as a header unit, which is required
when using the MSVC `std` module (certain includes inside it fail when treated as header units).

`BigHeaderUnit::YES` compiles two composite header units per platform (one for STL headers, one for platform headers
such as `Windows.h` on Windows). This is set up in `Configuration::initialize()` function.

### Example 8 — Module directories

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

### Example 9 — Header-Units across multiple targets from one parent directory

<details>
<summary>hmake.cpp</summary>

```cpp
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

### Example 10 — Header units across multiple targets in the same directory

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
`BTarget::completeRoundOne` is called for all the BTargets
but `BTarget::selectiveBuild` is set for a selective few.
`BTarget::setSelectiveBuild` is called before round1
`BTarget::completeRoundOne` call,
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
    void completeRoundOne(Builder &builder, const unsigned short round, bool &isComplete) override
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
    c->addDep<0>(*e);
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

C is skipped because `buildExplicit = true`, and it wasn't explicitly named.

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

C is explicitly named, so it's included with other targets.

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
F is only printed when `hbuild` runs in to configure dir.

</details>

## Future Direction

HMake 1.0 will be released when it can build several large projects (UE5, AOSP, Qt, and similar) using the idiomatic
API, with support for popular languages, frameworks (Android, iOS), and CI/CD systems. A 1.0 release is expected within
1–3 years, depending on reception.

If you maintain the build system of a large project and are interested in significantly faster builds, collaboration is
welcome. The required source changes are minimal.

---

## Support

HMake has been in development for over 4 years. If you find it useful, please consider supporting the project
via [Patreon](https://www.patreon.com) or by contacting hassan.sajjad069@gmail.com.