# HMake

Hassan's Make or HMake is a C++ build system with a pure C++ API — no DSL,
no domain-specific configuration language.
You describe your build in C++, and HMake executes it.
Support for additional programming languages and API bindings is planned.

HMake features a novel build algorithm with dynamic nodes, dynamic edges, and advanced dependency specification.
Its core is approximately 17,000 lines of C++ — significantly smaller than CMake + Ninja combined.

HMake requires a 64-bit host toolchain. Runtime buffer sizes, byte offsets, and capacities use explicit
`uint64_t`/`int64_t` values; narrower integer fields are retained only where an external API, serialized format, or
deliberately compact data structure requires them.

## Architecture

HMake separates concerns cleanly into two layers:

**Core layer** — `BTarget`, `Builder`, `Node`. These classes are general-purpose and have no knowledge of
C++. Any build system can be built on top of these.

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

`initializeCache()` retains `nodes-cache.bin`; loaded nodes refer directly to its NUL-terminated path bytes, while newly
interned paths use stable process-owned strings. It restores filesystem snapshots without stating or hashing them yet.
Before round 0, `Builder::checkNodes()` runs `performSystemCheck()` and `performContentHash()` in parallel on nodes marked
`doStatFile` / `doHashFile`. Skip/rebuild decisions use `Node::contentHash` (rapidhash of file contents) inside
`setUpdateStatus()`, not file modification times alone. After the build, `getBuildCache()` may call `checkNodes(false)` for
nodes that were flagged during compilation (for example headers discovered from compiler output).

### BTargetCache

Each target has a `BTargetCache` row in memory, backed by on-disk `config-cache.bin` and `build-cache.bin` files under the
configure directory (see `initializeCache()` / `configureOrBuild()` in `BuildSystemFunctions.cpp`).

The binary files contain only their payloads. HMake retains the previously read bytes and compares a newly serialized
payload directly, avoiding a temporary write and atomic replacement when the cache is unchanged.

| File | When written | Contents |
|------|----------------|----------|
| `nodes-cache.bin` | Configure or build | Repeated `[u16 path size][path][NUL][u64 modification time][u64 content hash]` records in `Node::myId` order |
| `config-cache.bin` | End of configure | Per target: `cacheName` + sized blob (`writeConfigCacheAtConfigTime`) |
| `build-cache.bin` | HMake take-off, configure, or build | The last successful configuration time, recompile/reconfigure node-ID arrays, then per-target dependency lists and sized bodies; process targets may end in a 16-byte `cumulativeHash`/`completionTime` footer |

At startup, `readConfigCache()` and `readBuildCache()` populate `bTargetCaches` before `buildSpecification()` constructs
live targets. `CppTarget` stores node IDs for sources, modules, header units, and includes in config-cache. At the end of
a build, only targets with `buildCacheUpdated` or `buildFooterUpdated` are rewritten; unchanged blobs are copied as-is.

### Builder

`Builder` is constructed after caches are loaded and `buildSpecification()` has registered targets. It runs **round 1**
(`completeRoundOne()` — configure-time setup), then in build mode **round 0** (async compilation/linking). Configure mode
stops after round 1 and writes the cache files.

Round 0 maintains `readyBTargets`, the queue of targets whose `dependenciesSize` has reached zero. After topological
sorting, ready targets are enqueued; completion decrements dependents and enqueues any that become ready. Round-0
dependency lists are persisted into each target's build-cache entry when the build finishes.

**Bring-to-front scheduling (`CppMod`).** When a module or header-unit compilation discovers that another unit is already
in the ready queue but `isEventRegistered` has not run on it yet, and consumers are blocked waiting on that unit, HMake
can move it to the head of `readyBTargets` using `RealBTarget::insertionIndex` (the previous queue slot is nulled so
the dependency is not scheduled twice). That prioritizes work with known waiters over other ready targets and lowers peak
memory by reducing how long compiler processes sit idle.

**Incremental decisions.** After `checkNodes(true)`, selective targets call `setUpdateStatus()`, which compares
`Node::contentHash`, cached `cumulativeHash`, and dependency `completionTime` from the build-cache footer — not mtimes
alone. When inputs change during the build, targets set `buildCacheUpdated` / `buildFooterUpdated` so `getBuildCache()`
refreshes hashes and rewrites those entries before saving `build-cache`.

### Unchanged-output cutoff

A process target can initially require an update, run successfully, and then discover that its observable output did
not change. A code generator is the simplest example: its inputs or executable may have changed, but the newly generated
file can still be identical to the existing file. In that case the target may change
`realBTargets[0].updateStatus` from `UPDATE_NEEDED` to `UPDATE_NOT_NEEDED`. HMake then treats the execution as an
unchanged-output cutoff: the target succeeds, but the unchanged result is not propagated as a rebuild reason to its
dependents.

HMake deliberately keeps this contract small and easy to adopt. Once a target has successfully proved that all of its
observable outputs are unchanged, the cutoff itself is a single assignment to `updateStatus`; the target does not need
to edit dependency edges, manipulate timestamps, or implement dependent cancellation. HMake owns those mechanics and
conservatively re-evaluates affected targets at scheduler decision points before suppressing work.

Process targets do not assign their own `completionTime`. At the scheduler commit point,
`Builder::decrementFromDependents()` records the current time only when a process target completed successfully, its
final status is `UPDATE_NEEDED`, and it marked its footer updated. Consequently, changing the final status to
`UPDATE_NOT_NEEDED` both suppresses update propagation and preserves the cached completion time. The target can still
set `buildFooterUpdated = true` to cache its new `cumulativeHash`; this accepts the input change without falsely
claiming that the output changed, so the generator does not need to run again on the next HMake invocation.

The essential completion pattern is:

```cpp
// After process completion and an application-specific output comparison:
if (rb.exitStatus == EXIT_SUCCESS)
{
    if (outputsAreUnchanged)
    {
        rb.updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;
    }
    buildFooterUpdated = true; // Cache the current cumulativeHash.
}
```

Only apply this transition after a successful command, after proving that every observable output is unchanged, and
after preparing the target's normal cache/footer update. Leave the target `UPDATE_NEEDED` when any output changed, and
never convert a failed execution into a successful cutoff. With that ordinary completion bookkeeping in place, changing
`updateStatus` is the complete cutoff operation: HMake clears stale dependency reasons and runs the target's full virtual
update check again. If the target itself, a static dependency, or a dynamic dependency still requires an update, the
work continues. This narrow contract makes unchanged-output cutoff both straightforward to use and conservative by
construction.

Prefer writing generated data to temporary files and replacing final outputs only when their contents differ; this also
keeps the last successful output safe if the process fails or is cancelled.

[`HeaderGen`](hconfigure/header/CustomCodeGenerator.hpp) is a minimal asynchronous code-generator model that can be
extended with this comparison.

#### C++ modules already in flight

Module dependencies are not restricted to the initial static scheduling graph: a running compiler can discover a module
or header-unit dependency over IPC and wait for it. Consequently, a `CppMod` consumer may already be running when the
provider finishes and changes its status back to `UPDATE_NOT_NEEDED`.

`CppMod::setUpdateStatus()` records the direct dependency that made the consumer stale in
`RealBTarget::reasonForUpdate`. Once that recorded reason completes without changing its output, HMake performs a full
re-evaluation while the consumer's cached `completionTime` is still visible. The check occurs before initial event
registration, after a dependency wake-up, and before responding with a provider that had already completed:

- If the consumer is still stale because of its own inputs or another dependency, compilation resumes and the
  provider response is sent to the compiler.
- If no rebuild reason remains, HMake terminates the speculative compiler process, records a successful completion, and
  preserves the consumer's previous cached output time.

`Builder` records `BTarget::initiationTime` once, immediately before the target's first `isEventRegistered()` call.
Module resumptions do not change it. C++ source and module cache writers use this time to avoid accepting a header hash
for a file modified after compilation began. `RealBTarget::completionTime` is committed separately, only at successful
scheduler completion; cancelled and unchanged-output compilations therefore leave it untouched.

The resume path is used for dependencies requested dynamically by the compiler; its recorded update reason can come
from cached module dependencies or from static dependency evaluation. Every provider requested at runtime is added to
the consumer's dependency set before re-evaluation, including a provider that already completed. Therefore an updated
new provider keeps the consumer alive, while an unchanged provider cannot hide a different reason to rebuild. A null
`reasonForUpdate` deliberately disables cancellation: it means the target is stale because of its own inputs, because a
completed static dependency propagated the update, or that no direct dependency reason was recorded.

Cancellation is permitted only after the recorded reason becomes `UPDATE_NOT_NEEDED` and a full re-evaluation finds no
remaining reason to rebuild. This is the key correctness property of HMake's in-flight cutoff: speculative compilation
is discarded only after the same target-specific update logic that launched it now proves it unnecessary.

#### Compared with Ninja's `restat`

HMake's unchanged-output cutoff shares the goal of [Ninja's `restat`](https://ninja-build.org/manual.html#ref_rule):
avoid rebuilding reverse dependencies when a command ran but left its outputs unchanged. Ninja re-stats output
timestamps and can remove pending reverse dependencies when their modification times did not change. HMake exposes the
decision at the target-status level instead, allowing a target to use the proof appropriate to its output — for example,
a byte-for-byte comparison, a semantic code-generator result, or a declaration hash — and report that proof with one
state assignment.

The distinction is most visible for dynamic modules. [Ninja's `dyndep`](https://ninja-build.org/manual.html#dynamic_dependencies)
loads generated dependency information before the affected build statement proceeds. HMake can discover a dependency
through compiler IPC after the consumer compiler is already running and waiting. It then adds the live dependency,
re-evaluates the consumer, and can terminate only the speculative process that has become unnecessary. This handles a
broader in-flight execution case; it is not, by itself, a claim that HMake is universally faster than Ninja.

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

**Build an example:**

`hbuild` owns the complete take-off. It creates project metadata, compiles the generated
configure/build executables when necessary, configures, and then builds.

```bash
cd HMake/Examples/Example1
hbuild -B build
```

For Example 1, the resulting executable is `build/Release/app/app` on Linux
(`app.exe` on Windows).

### Start a project

Place an `hmake.cpp` beside your sources. This minimal specification creates one Release
configuration and one executable:

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

The lifecycle is deliberately small:

1. `buildSpecification()` creates configurations and assigns typed features.
2. `CALL_CONFIGURATION_SPECIFICATION` invokes `configurationSpecification()` for the active configuration.
3. The `getCpp*DSC()` factories declare compiled/linkable targets; add files and include paths through
   `getSourceTarget()`, and dependencies through `privateDeps()`, `publicDeps()`, or `interfaceDeps()`.

Feature assignments are evaluated left to right. Put broad presets before specific overrides, for example:

```cpp
getConfiguration("Debug").assign(ConfigType::DEBUG, Warnings::EXTRA, WarningsAsErrors::ON);
```

<details>
<summary> Step-by-Step Explanation </summary>

HMake installs a named default toolchain matching the compiler used to build HMake. Additional
toolchains live in `~/.hmake/toolchains.json` on Linux or `%LOCALAPPDATA%\HMake\toolchains.json`
on Windows, and optionally beside `hmake.cpp`. Select one with `hbuild --toolchain <name>` and print the fully
resolved registry with `hbuild --list-toolchains -B build` from the source directory or `hbuild --list-toolchains`
from the build directory.

`toolchains.json` is a top-level object keyed by unique toolchain names. A complete entry has the following form. A
derived entry can use `extends` and override only the fields it changes; its base must appear earlier.

```json
{
    "llvm-18": {
        "compiler": "/opt/llvm/bin/clang++",
        "linker": "/opt/llvm/bin/clang++",
        "archiver": "/opt/llvm/bin/llvm-ar",
        "family": "clang",
        "style": "gnu",
        "version": "18",
        "target": "x86_64-linux-gnu",
        "include-dirs": ["/opt/llvm/include/c++/v1", "/opt/llvm/lib/clang/18/include", "/usr/include"],
        "library-dirs": ["/opt/llvm/lib", "/usr/lib"],
        "bootstrap-arguments": []
    },
    "my-clang": {
        "extends": "llvm-18",
        "bootstrap-arguments": ["-fuse-ld=lld"]
    }
}
```

The built-in toolchain embeds the standard include and library directories detected while HMake is compiled. HMake
disables the compiler's default header search with `-nostdinc -nostdinc++` or `/X`, so a complete custom toolchain must
likewise provide its full standard include search path. Derived toolchains inherit these directory lists unless they
replace them.

Toolchain registries are treated as externally managed inputs. Editing `toolchains.json` does not cause automatic
reconfiguration, while every modification of the project's `cache.txt` does.

The project `cache.txt` stores the selected toolchain, default job count, and typed cache variables.
Empty lines and lines beginning with `#` are ignored. Every non-empty line must begin at column zero; leading whitespace
is invalid. The first two values are positional; later values use `name=value` syntax. The source directory is exactly
the nearest parent of the build directory that contains `hmake.cpp`.
Generated commands are
structured internally rather than stored as editable shell strings. Cache variables are edited in this file; `hbuild`
does not provide `-D` command-line overrides.

On each invocation, `hbuild` checks `configure`, `build`, `recompileNodes` (which always contains `hmake.cpp`),
`reconfigureNodes` (which always contains `cache.txt`), `nodes-cache.bin`, `config-cache.bin`, and `build-cache.bin`.
Compiler-discovered headers, HMake libraries and headers, compiler/linker binaries, toolchain definitions, and
bootstrap-command changes are not monitored automatically. `--recompile`, `--reconfigure`, and `--configure-only`
provide explicit control over the generated executables and configuration.

The hmake source filename selects the HMake API generation instead of storing a schema field in local caches.
The current library pair uses `hmake.cpp`; future generation-specific installations use names such as
`hmakev1.cpp` with their matching configure/build libraries.

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
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN), message{str}
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
    b->addDep<1>(a);
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
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN), message{str}
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

    b->addDep<0>(a);
    a->addDep<1>(b);
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
empty `message` parameter means the process exited; `*run.output` contains its full output.
When `isEventCompleted` returns `true`, it must select how processing continues: call `run.startRead()` to wait for more
output, call `run.writeReadExpected()` to reply and then wait for more output, or call neither to leave the child paused.

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
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN), message{str}
    {
    }

    void completeRoundOne() override
    {
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Cat1");
    OurTarget *b = new OurTarget("Cat2");
    OurTarget *c = new OurTarget("Cat3");
    a->addDep<0>(b);
    b->addDep<0>(c);
    c->addDep<0>(a);
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
    explicit OurTarget(const string &name_, const bool error_ = false)
        : BTarget(name_, false, BTargetType::UNKNOWN), name{name_}, error(error_)
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
    d->addDep<0>(e);
    h->addDep<0>(g);
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
    explicit OurTarget(const string &str, const unsigned short low_, const unsigned short high_)
        : BTarget(str, false, BTargetType::UNKNOWN, false, false, false, false), low(low_), high(high_)
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
    explicit OurTarget2(const string &str) : BTarget(str, false, BTargetType::UNKNOWN)
    {
        a = new OurTarget("a", 10, 40);
        b = new OurTarget("b", 50, 80);
        c = new OurTarget("c", 800, 1000);
    }

    bool isEventRegistered(Builder &builder) override
    {
        a->addDep<0>(c);
        b->addDep<0>(c);

        uint32_t insertionIndex;
        builder.readyBTargets.emplace(&c->realBTargets[0], insertionIndex);
        builder.readyBTargetsSizeGoal += 3;
        return false;
    }
};

void buildSpecification()
{
    OurTarget2 *target2 = new OurTarget2("target2");
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

1. You have to update the ```Builder::readyBTargetsSizeGoal``` variable with the
   additional number of times ```isEventRegistered``` will be called.
2. If any newly added targets do not have any dependency
   then it must be added in ```readyBTargets``` list like we added ```c``` target.
3. Besides new targets, we can also modify the dependencies of older targets.
   But these targets ```dependenciesSize``` should not be zero.
   Because if the target ```dependenciesSize``` becomes zero,
   it is added to the ```readyBTargets``` list.
   HMake does not allow removing or modifying elements in this list.

### Example 7 — Dynamic edges with cycle detection

<details>
<summary>hmake.cpp</summary>

```cpp
#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN){}
    bool isEventRegistered(Builder &builder) override
    {
        b->addDep<0>(c);
        c->addDep<0>(b);
        return false;
    }
};

void buildSpecification()
{
    b = new BTarget("b", false, BTargetType::UNKNOWN);
    c = new BTarget("c", false, BTargetType::UNKNOWN);
    OurTarget *target = new OurTarget("target");
    b->addDep<0>(target);
    c->addDep<0>(target);
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
        ++builder.readyBTargetsSizeGoal;
        // builder.readyBTargets.emplace(&a->realBTargets[0]);
        
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
    explicit Process(const string &name_) : BTarget(name_, false, BTargetType::UNKNOWN)
    {
    }
    static inline char cmd[] = "./a.out";

    bool isEventRegistered(Builder &builder) override
    {
        // Pass true so HMake keeps the child's stdin pipe open — this process
        // sends a message to the build-system and then waits for a reply before
        // continuing.
        run.startAsyncProcess(cmd, builder, this, /*haveWritePipe=*/true);
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
            out += *run.output;
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
            // Reply, then immediately arm the read for the child's next message.
            const string reply = "std\n";
            run.writeReadExpected(reply);
            firstReceived = true;
        }
        else
        {
            // No reply is needed for the final message, but the exit notification still needs a read armed.
            run.startRead();
        }

        return true; // keep listening; more messages or exit event may follow
    }
};

void buildSpecification()
{
    // Any BTarget must have application life-time.
    new Process("Process");
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
// HMake strips message from the child's normal stdout which is *run.output in
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
output (captured in `*run.output` and printed after exit), and the P2978 IPC protocol for structured messages that the
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

`getConfiguration()` creates a default `Configuration` named `Release` with `ConfigType::RELEASE`.
`CALL_CONFIGURATION_SPECIFICATION` ensures `configurationSpecification` is only invoked when `hbuild` is executed in the
build directory or a matching configuration subdirectory. This allows a multi-configuration project to build only the
active configuration without running the others.

`getCppExeDSC` returns a `DSC<CppTarget>` (Dependency Specification Container). `getSourceTarget()` returns the
`CppTarget` to which source files, include directories, and module files are attached.

Every `Configuration` creates a `stdCppTarget` by default, which carries the standard include directories from
the selected named toolchain. All targets created via `get*` functions receive this as a private dependency
automatically.

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

    // Change FILE1=true to FILE1=false in cache.txt, then run hbuild. HMake detects the
    // graph-affecting cache change, reconfigures, and selects file2.cpp.
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

`CacheVariable` supports `bool`, `int`, and `string` values. Names must match `[A-Za-z_][A-Za-z0-9_]*`; booleans use
`true` or `false`, integers use decimal syntax, and strings require outermost double quotes:

```text
SDK_ROOT="C:\Program Files\Microsoft Visual Studio"
MESSAGE="first\nsecond"
```

The outermost quotes delimit a string; backslashes and any interior quotes are literal. Thus `\n` above is two
characters. Call `decodeBackslashEscapes()` explicitly to decode `\\`, `\"`, `\n`, `\r`, `\t`, `\b`, `\f`, and
fixed-width `\xHH`. Actual CR, LF, and NUL characters cannot be represented in a cache value. A missing variable is
appended to `cache.txt` with its default value; an existing value with a different type is an error.

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
                              ? Node::getNode<PathType::NEITHER>("../Example4/Build/Release/Cat" + str, false, false)
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
tree. Pass `nullptr` at build time because the output-directory lookup is needed only while configuring.

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
        // config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    // module build of std.ixx provided with the msvc lib crashing with this commit. can be produced on developer
    // powershell. https://pastebin.com/38Q3FmWh
    //  it was working before but is failing with this commit. cc0371f2a4f95614c35601f898dde7745120e8d1.
    // getConfiguration("modules").assign(IsCppMod::YES, StdAsHeaderUnit::NO, CxxSTD::V_20);
    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
```

</details>

HMake has first-class support for C++20 modules and header units, including the ability to compile them without a prior
scanning step. During compilation, newly discovered module/hu dependencies can be prioritized in the build queue when
other units are already waiting on them (see **Builder** above). It is currently the only build system with this capability. The approach is described
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
    getConfiguration("static").assign(IsCppMod::YES, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
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
        : BTarget(std::move(name), false, BTargetType::UNKNOWN, buildExplicit, makeDirectory), message{std::move(str)}
    {
    }
    void completeRoundOne() override
    {
        if (selectiveBuild)
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
