# Decentralized Unreal Engine integration

This directory contains the scanner that connects decentralized Unreal Engine
`*.hmake.hpp` specifications to HMake. It is the current UE integration; it does
not depend on the former UBT-export parser.

## Structure

- `scanner.py` scans the UE checkout for `*.hmake.hpp` files and generates the
  checkout-root `hmake.cpp`.
- `build_commands.json` is a configuration-keyed table of compiler, linker, and
  archiver command prefixes extracted from a successful UBT bootstrap build.
- `hconfigure/header/ue.hpp` declares the UE-oriented configuration, target,
  registry, dependency, path, and prebuilt-library APIs.
- `hconfigure/src/ue.cpp` implements lazy specification selection and graph
  construction.
- Each UE module or target owns its specification beside its source or target file.

The scanner registers discovered files globally once. Each `UeConfiguration`
then has its own lazily evaluated target graph, build commands, and PLOAT objects.
A module is not configured merely because the scanner found it; it is configured
only when requested as a graph root or reached through a dependency.

## Generate hmake.cpp

Run the scanner manually whenever a `*.hmake.hpp` file is added, removed, or
renamed:

```sh
python3 /home/hassan/Projects/HMake/Projects/UE5/scanner.py \
  --ue-root /home/hassan/Projects/UnrealEngine
```

By default this writes `/home/hassan/Projects/UnrealEngine/hmake.cpp`. Use
`--output` to select another file.

## File naming and selection

Supported names are:

- `Core.module.hmake.hpp`: base module specification.
- `Core.group.Unix.module.hmake.hpp`: platform-group specialization.
- `Core.platform.Linux.module.hmake.hpp`: exact-platform specialization.
- `zlib.prebuilt.hmake.hpp`: source-less external/prebuilt module specification.
- `UnrealServer.target.hmake.hpp`: top-level target specification.

HMake always applies the base function. It then applies the exact-platform
function when one exists. Otherwise it applies the single matching platform-group
function. Multiple matching group functions without an exact-platform function
are rejected, matching UBT's `RulesAssembly` selection rule. Unlike C# class
inheritance, an HMake exact-platform function does not implicitly execute a group
function; shared specialization logic should be factored into a normal helper or
stated explicitly.

Every file defines the same minimal entry point:

```cpp
void specify(UeConfiguration &configuration)
{
    auto &module = configuration.currentTarget();
    module.publicDeps("Core").privateDeps("Projects");

    module.getSourceTarget()
        .publicCompileDefines("WITH_EXAMPLE", "1");
}
```

## Module directory discovery

There is no source-directory call in a module specification, matching UBT's
`ModuleRules` API. The backend derives module directories from the selected base
and platform `*.module.hmake.hpp` files, corresponding to
`ModuleRules.GetAllModuleDirectories()`. It then:

- recursively discovers `.cpp`, `.c`, `.cc`, and `.cxx` sources;
- excludes unsupported platform directories and subtrees containing
  `.ubtignore`;
- exposes `Classes` and `Public` as public include/header inputs;
- exposes `Private` only while compiling the module; and
- propagates `Internal` for the current engine-only graph. UBT limits `Internal`
  visibility by rules scope, so project/plugin support must add the corresponding
  scope check.

`UeCppTarget::bAddDefaultIncludePaths` maps to
`ModuleRules.bAddDefaultIncludePaths` and defaults to true. Disabling it suppresses
only conventional include discovery; source discovery remains automatic, as it
does in `UEBuildModuleCPP.FindInputFiles()`.

The exceptional `conditionalAddModuleDirectory()` operation maps to
`ModuleRules.ConditionalAddModuleDirectory()`. It conditionally contributes
another source root and its conventional include directories. It is not needed
for the ordinary directory containing a module specification.

Unusual ModuleRules paths call HMake's `publicIncludesSource()` and
`privateIncludesSource()` APIs directly. While a `specify()` function runs,
HMake resolves its relative paths from the directory containing that
`*.hmake.hpp` file. There is no UE path-resolution or source-directory wrapper.

The scanner uses ordinary suffix comparisons, not a regular-expression engine.
The accepted source extensions are a closed, tiny set, so direct comparisons have
less C++ compile-time work, less runtime setup, and no third-party dependency.
ISPC and platform-specific Objective-C/Swift/resource sources are outside this
first-stage Linux build.

## Existing UHT output

The first stage does not run UHT. `setGeneratedIncludeRoot()` points at an
existing UBT-generated `Inc` directory. When
`<root>/<Module>/UHT` exists, HMake adds its generated headers to the module's
include inputs and compiles its existing `*.gen.cpp` translation units. A missing
module UHT directory is simply ignored.

These are two separate jobs. `*.generated.h` is included by normal C++ and only
needs the generated directory on the include path. `*.gen.cpp` contains generated
definitions and registration code; it is a complete translation unit, so adding
its directory as an include directory cannot compile it. HMake therefore registers
every `*.gen.cpp` through `moduleFiles()`.

UE sources may contain `#include UE_INLINE_GENERATED_CPP_BY_NAME(Name)`. The
command table defines `HMAKE_COMPILE_GENERATED_CPP_SEPARATELY=1`, causing
`ObjectMacros.h` to map those directives to `HMakeEmptyGeneratedCpp.h`. This
prevents duplicate definitions while keeping the original UE source unchanged.
This is an intentional HMake adaptation; UBT normally removes inlined generated
files from its generated compile list and may unity-build the remainder.

## Configuration commands

A UE configuration may pass a `UeBuildCommands` value to
`setBuildCommands()`. `UeConfiguration::initialize()` first performs HMake's
normal initialization, then installs each non-empty C, C++, linker, and archiver
command template. HMake still appends the source, output, `-c`, `-o`, and graph
inputs.

`build_commands.json` is keyed by platform, architecture, UE build configuration,
and target type. It currently contains one `Linux/x64/Debug/Server` row. The C++
prefix came from the local successful UBT `BASE-COMMAND`; HMake owns `-c`, source,
dependency-file, and output arguments, so `-c` was removed from the stored prefix.
The C prefix uses UBT's equivalent flags with `-x c`. The linker prefix comes from
the successful `UnrealServer-Linux-Debug.link.rsp` portion before `-o`, and the
archiver is the matching UE SDK `llvm-ar`. The one HMake-only addition to the C
and C++ prefixes is `HMAKE_COMPILE_GENERATED_CPP_SEPARATELY=1`, which selects
the separate generated-code behavior described above.

More rows can be appended without changing the scanner or `UeConfiguration`.
Per-target command variation remains deferred: the initial HMake model requires
one compatible base C/C++ command per configuration. Also, HMake's current linker
template is a prefix; exact UBT arguments that must appear after all objects (for
example some system libraries and linker groups) still need to be represented by
prebuilt/link requirements or a future linker-suffix facility.

## Dependencies and external modules

The named dependency APIs map approximately as follows:

| HMake | Closest ModuleRules meaning |
| --- | --- |
| `publicDeps()` | public compile and link dependency |
| `privateDeps()` | private compile and link dependency |
| `publicCompileDeps()` | public include-path/compile-only module dependency |
| `privateCompileDeps()` | private include-path/compile-only module dependency |
| `publicLinkDeps()` | public link-only dependency |
| `privateLinkDeps()` | private link-only dependency |

A source-less external module can declare its include paths and definitions on its
`UeCppTarget`, then attach physical libraries through
`publicPrebuiltStaticLibrary()`, `privatePrebuiltStaticLibrary()`,
`publicPrebuiltSharedLibrary()`, or their interface forms. The current PLOAT
adapter accepts conventional `.a`, `.lib`, `.so`, `.dylib`, and `.dll`
filenames.

## Monolithic modules and circular dependencies

`UnrealServer-Linux-Debug` is modeled as a monolithic executable. Ordinary UE
modules are `UeCppTarget` object producers; they do not become intermediate static
archives. Once lazy dependency expansion reaches a fixed point, HMake attaches all
reachable module object producers and prebuilt libraries to the requested target's
executable PLOAT.

This matters for legacy UE circular module references. `getOrAddTarget()` returns
an already-configuring module so specification recursion terminates. If a named
compile dependency closes that DFS cycle, HMake retains the public/private compile
usage relationship but omits that one scheduling back-edge. It then computes the
usage-requirement closure before graph completion. As a result,
`RealBTarget::sortGraph()` still rejects real HMake action cycles, but does not see
an artificial cycle merely because two monolithic UE modules expose headers to
each other. There is no archive-order cycle because monolithic modules are linked
as objects.

This corresponds to UBT's legacy
`CircularlyReferencedDependentModules` treatment for the monolithic case. Modular
shared-library cycles are a different problem: UBT may create import libraries
separately, and HMake does not model that stage yet.

## First-stage ModuleRules coverage

This layer is not yet the full API exposed by `ModuleRules` and `TargetRules`.
The parts already represented for the Linux/x64 Debug non-unity experiment are:

- automatic module-directory source and conventional include discovery;
- `bAddDefaultIncludePaths` and conditional additional module directories;
- public/private compile-and-link dependencies and compile-only include-path
  dependencies;
- public/private definitions and ordinary include paths through `CppTarget`;
- physical prebuilt static/shared libraries;
- platform, platform-group, architecture, configuration, and target-type
  conditions;
- an existing-UHT-output consumer; and
- monolithic executable assembly with the current circular compile-dependency
  handling.

That is enough infrastructure to translate more modules, but it is not yet enough
to claim that an arbitrary `UnrealServer-Linux-Debug` rules graph will compile.
Important remaining rule surfaces include:

- per-module compile-environment differences such as RTTI, exceptions,
  optimization, warning policy, and language-standard overrides;
- system-library names, general linker options, ordered linker groups, delay-load
  declarations, and runtime library handling;
- exact UBT `Internal` include visibility once project/plugin rules scopes exist;
- dynamically loaded modules and other nonordinary dependency categories;
- platform source kinds beyond the initial C/C++ set;
- project/plugin descriptor evaluation; and
- receipt, staging, runtime-dependency, precompiled-module, and other packaging
  properties. Most of the last category is not required merely to compile and
  link the first server executable.

Unity and PCH APIs are intentionally absent from this stage. Existing UHT output
is consumed, and ISPC is intentionally disabled. Before attempting the complete
server build, every reachable module still needs a decentralized specification
covering the subset of its rules that affects this configuration.

## .uproject and .uplugin plan

The scanner should continue registering every `*.hmake.hpp` globally, independent
of any project. Descriptor handling belongs to each `UeConfiguration`, because the
enabled module set can change with platform, target type, configuration, project,
and target rules.

For a standard Linux Debug server configuration, descriptor evaluation should run
before `configureRequestedTargets()`:

1. If no `.uproject` is supplied, as with the current engine `UnrealServer` build,
   there are no project modules or project plugin overrides to add.
2. If a project is supplied, parse its `Modules` array and retain only descriptors
   compiled for Linux, Debug, the selected target name, and `Server`. This mirrors
   UBT's `ModuleDescriptor.IsCompiledInConfiguration()` filtering, including module
   type plus platform/target/configuration allow and deny lists.
3. Discover plugin descriptors in engine plugin roots, the project's `Plugins`
   directory, and additional plugin directories. Resolve the enabled set from
   target enable/disable lists, project plugin references, enabled-by-default
   policy, optional references, and recursive plugin dependencies.
4. Apply plugin-reference and plugin-descriptor platform/target filters. For each
   enabled plugin, filter its `.uplugin` `Modules` array with the same
   `IsCompiledInConfiguration` inputs used for project modules.
5. Add the surviving project and plugin module names as private roots of the
   requested target. `getOrAddTarget()` then lazily executes their decentralized
   specifications and reaches their ordinary dependencies.

Descriptor selection determines which modules enter the graph; it does not replace
their `*.hmake.hpp` build specifications. For initial parity, the safest bootstrap
is to compare the selected roots against one successful UBT makefile/receipt or
query export, then make the descriptor evaluator reproduce that set.

## Deferred work

- Implement the `.uproject`/`.uplugin` descriptor plan above. It corresponds to
  UBT's `SetupProjectModules()` and `SetupPlugins()`.
- Add explicit validation/diagnostics for legacy circular module declarations and
  implement modular shared-library import-library handling if modular UE targets
  are brought up.
- Generate command-table entries automatically from a clean UBT bootstrap build.
- Add native UHT generation only after the standard Debug server graph builds from
  existing generated output. ISPC remains disabled for this first stage.
