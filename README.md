# HMake

HMake is a build system that uses C++ for project build specifications. It uses C++ and JSON. Currently,
it is incomplete in many regards. Its status is prototype while bugs, omissions, and limitations are
expected. I developed it on Windows 11 with MSVC 19.32.31332 and Ubuntu 22.04 LTS with
GCC 11.2.0. You can try it on other platforms, but you may have to do some extra
steps/initializations.

## Steps For Running HMake

Use git clone --recursive to download HMake.

updateSourceNodeBTarget the project using CMake and Make(Please Note HMake can self-compile itself but
binaries and installers are currently not provided).
It will produce three binaries hhelper, hbuild, UnbuiltTarget, and one static
library hconfigure. You will use hhelper, hbuild, and hconfigure to configure
and build your projects.

Create the hmake.cpp file in the source directory of your project.
You will use hconfigure library. hconfigure library has
two files Configure.hpp and Configure.cpp. So you need to include Configure.hpp.

Note: If you want to use IntelliSense while writing hmake.cpp,
either you can use UnbuiltTarget. Modify the definition of UnbuiltTarget
in CMakeLists.txt and point towards your hmake.cpp.

updateSourceNodeBTarget this hmake.cpp with hconfigure library and produce the binary named configure in your buildDir.

Create cache.hmake file in your buildDir. This provides basic information such as compilerPath and linkerPath to build
system. This file also holds cache variables(Described Later). Certain variables with the right values should be defined
in
this file.

Run the configure binary.

Run the hbuild in the current buildDir to build your project.

## Steps For Running HMake Easily

So, you only need hbuild and hconfigure to build your project. But to help you in the build process, use the following
steps.

Create symbolic links for hhelper and hbuild binaries in /usr/local/bin/ or on Windows
add the build directory in your system Environment Variables Path, so you don't have trouble running these
commands in other directories. On Windows, you will have to run with .exe extensions.

Create buildDir in the sourceDir.

Run hhelper in it. This will create the cache.hmake with the right values

Run hhelper again. This will compile your hmake.cpp file and will also execute it.

Run hbuild to build the project.

So to run HMake Project:

    hhelper
    hhelper
    hbuild

## How It Works

When you run configure binary compiled from hamke.cpp in buildlDir, it generates the JSON files having .hmake extension.
These files have information about how to build the project. hbuild then uses these files to build the project. It is
like CMake where you first generate the build system and then use that build system to build project. In HMake you
generate JSON files and then run hbuild in the directory where these JSON files are present and hbuild will build the
project.

## Advantages Compared To Other Build Systems

Debugging the build system is easier. Because there is no lexing involved, it is plain C++, it is easy to Debug. Either
hconfigure is generating wrong hmake(json) files, or hbuild is not building correctly.

Obtaining info on how hbuild will build is easy. hmake files are very readable. Some knowledge of hbuild is still
required though

HMake uses Ninja-like algorithm to build your project.

Because the project files generated are JSON files, they can be easily integrated with IDEs and other tools.

## Basic HMake File

    int main()
    {
        Cache::initializeCache();
        Project project;
        ProjectVariant variant{};
        Executable app("app", variant);
        ADD_SRC_FILES_TO_TARGET(app, "main.cpp");
        ADD_EXECUTABLES_TO_VARIANT(variant, app);
        project.projectVariants.push_back(variant);
        project.configure();
    }

## Examples

There are 10 examples in the repo. Please see them. 9 and 10 are Module Examples.
The best Examples are hmake.cpp of HMake itself and Example 10.

Except for Example 10, to run any example, make a buildDir inside srcDir. And then run hhelper twice in that
directory and hbuild once. Instructions for Example10 are given in Example10/READMDE.md

## Package Manager

HMake can build HMake packages. These packages have many variants
from the same source code. An example is given in Example
7 and 8. Targets in these packages can specify their dependencies and consumer
requirements. These packages are intended
for other developers but not for other users. Support for generating
user-focused packages such as MSI and deb will also
be added. These packages are not best for developers because libraries
in them can't specify their dependencies. Neither
is there a mechanism for selecting a specific variant of that package.

A website is planned which will host HMake packages.

## Flags

Compiler Flags and linker flags depend on a lot of factors. Such as compiler family, compiler version, library type,
target architecture, target os, target os version, configuration.

In HMake, final compiler flags are the sum of compilerFlagDependencies that have trickled up because of PUBLIC
dependencyType
or those in flags member variable. flags member variable is initialized with the flags member variable of the variant
that
is passed while the flag member variable of the variant is initialized with the global flag inline variable.

CompilerFlags class has a mechanism for specifying flags which allows either to be fully specific about flags or be less
specific. e.g.

`flags.compilerFlags[BTFamily::GCC][Version{10,2,0}][ConfigType::DEBUG] = "-g"`

Or you can be less specific e.g.

`flags.compilerFlags[ConfigType::RELEASE] = "-O3 -DNDEBUG"`

In the latter case, these flags are for all compilers and all versions. You have to register the compiler family in the
compilerFamilies in CompilerFlags class.

So, when cross-compilation and cross-platform support will be added, this technique can be helpful.

## Features

1) Modules Support.
2) Multithreaded Cached Compilation.
3) Colored And Pretty-Printed Output.
4) Settings are provided to control color and other aspects of hbuild like numberOfThreads. settings.json is
   created in your buildDir.
5) Using C++ Variadic Templates, functions are provided to write hmake.cpp files in a more concise manner.

## Planned Features

### High Priority

1) IDEs and Editors support and plugins. Because HMake use JSON, it can be easily integrated and this is the top-most
   priority.
2) Dynamic / Shared Libraries Support.
3) hhelper needs improvements in detecting the environment. Currently, it is just a stud.
   I will copy the code from CMake for this.
4) hbuild is slow especially for bigger projects. I haven't profiled it yet. But there are some missed
   optimizations in the code. E.g. for modules hbuild only builds them in a multithreaded manner but does not
   generate the SMFiles in a multithreaded manner. Maybe, include a modified pre-processor and save the costs
   of process setup and file read and file write for generating SMFiles. hbuild should start compiling a file as soon
   as it knows
   that a file needs compilation. To test hbuild for bigger projects simply select an example and place this
   line ```project.variants.emplace_back(variant);``` inside a loop iterating say 1000 times. It will create
   a project with 1000 variants with sufficient targets.
5) Improve hconfigure. hbuild is in a very good state but hconfigure needs more work
   atm as it does not detect circular dependencies(circular dependencies are detected
   by hbuild though), will not configure a dependency
   of dependency, will reconfigure a target if it is a dependency of 2 targets. Another
   shortcoming is that to specify IDD(Include Directory Dependency), CFD(updateSourceNodeBTarget
   Flag Dependency) or CDD(updateSourceNodeBTarget Definition Dependency) for a module target the
   interface is similar to a source target which means you have to specify an
   extra PUBLIC or PRIVATE while defining these which is unnecessary in the case of modules
   as they can't be a dependency of another target
6) JSON Schema Definition for all JSON files of the Project.
7) Installers Development Support.
8) Support for MacOS and Clang.
9) Link-Before Dependencies. These won't be linked with but only linked-before.
10) Some Command-Line Easiness. Like flags for building HMake
    project in 1 command instead of three, clean project, rebuild project, and configure and
    build project.
11) Environment mechanism for Linux, so users can opt out of the standard library.
12) Packaging does not support Modules Packaging, Header-Only libraries, and the packaging of
    prebuilt-libraries(basically an only repackaging of the already existing prebuilt libraries). An API for Resources
    will also be added. There is a common include-dir feature in HMake Packages which maybe integrated with this.
    A website for uploading and downloading HMake packages is to be developed too.
13) SourceDirectory will have an option for opting out of subdirectories or selecting only
    subdirectories that match a regex. SourceDirectory will also have an option for
    collecting all files at configure time to improve speeds at runtime. This may also
    be added as a cache-level setting.
14) string comparisons anywhere in the project should happen
    from the end as path strings are most likely to differ from the end.
15) Some compilers and linkers like [mold](https://github.com/rui314/mold) can use
    multiple cores for quicker task completion. HMake should provide a setting to
    invoke these with extra cores option if cores are available.
16) As per this guideline
    https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=msvc-170#remarks
    use /WX flag for enabling warning as an error.
17) Option for dependency printing like https://pdimov.github.io/boostdep-report/ in
    hbuild for both modules and libraries.
18) An option for printing the original compiler and linker commands with original output.
19) A test will be added against every error output to ensure that error happens
    on those conditions. A test will be added against all if-else scenarios that
    impact the linking and caching scenarios to ensure they are working as expected.
    Adding and Removing things will also be tested. One major test with up to 1k targets
    will also be added. Maybe use some OSS for this.
20) Source-Code pruned packages, especially for header-only libraries. Some libraries like
    asio has a lot of #ifdef macros based on OS, Processor Arch, Compiler, Language Version, and some other choices
    which makes it hard sometimes for a user only interested in specific values of these to more easily
    look at the code. HMake will include a tool to create variants in HMake packages containing pruned code
    based on the specific values of these macros.
21) To do whatever is being done in vcvarsall.bat in c++ so that configuration
    can be quicker on Windows.
22) A GUI based application for easy viewing of HMake projects. IDE plugins can
    do this stuff too.
23) An option for marking include-directory for excluding it from including any files of it in
    HEADER_DEPENDENCIES key of a source file in target-cache for optimization purposes. The environment
    includes are not included in HEADER_DEPENDENCIES atm with no option of changing it.
24) If a compile or link error occurs, the cache is reliably saved, but it is saved only for one target for
    which the error occurred, and then hbuild exits. It should save the cache for all targets. This appears to
    be a small edit.
25) Some good enough defaults for major compilers, architectures, and standard configurations.
    Basically one good flags Json file.

### Low Priority / Speculation

1) Android, IOS app compilation support, and other programming languages support. Also provide HMake in other
   programming languages.
2) CI support using Jenkins or maybe some custom solution using C++ web framework.
3) Some other libraries included with hconfigure and hbuild and a way to specify
   their usage at build-time maybe using some JSON RPC, e.g libraries for compression
   will be useful for hbuild and libraries for Prebuilt Library Analysis may be useful
   for hconfigure.
4) Subdir support like add_subdirectory(). Currently, I don't plan to add this, as
   I believe that a single file solution is the best solution. This way you don't have to
   go back-and-forth, but, if needs to be added, can add this using a macro that defines
   some other function instead of main() when the file is #include in the parent directory
   and that function does some stuff and provides data to the parent function.

## Support

Please donate for further development. And that at a good pace. Please contact here
if you want to donate hassan.sajjad069@gmail.com. Thanks.