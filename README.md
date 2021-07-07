# HMake

HMake is the first of its kind AFAIK as it uses C++ for project build specification. It uses C++ and Json. It's inspired
by CMake but also very different from CMake. Currently, it is incomplete in many regards. Its status is prototype
currently. Currently, it's available only for linux but you can try it on other platforms. That's described in later
examples.

As compared to CMake, there is one extra step involved during project compilation.

## Steps For Running HMake

Use git clone --recursive to download hmake.

I developed on ubuntu with gcc-10.2, though project should successfully compile on any linux distro with g++ installed.

Compile the project using CMake and Make. It will produce three binaries hhelper, hbuild, UnbuiltTarget and one static
library hconfigure. You will use hhelper, hbuild and hconfigure to configure and build your projects.

Create the hmake.cpp file in source directory of your project. You will use hconfigure library. hconfigure library has
two files Configure.hpp and Configure.cpp. So you need to include Configure.hpp.

Note: If you want to use intellisense while writing hmake.cpp, either you can use UnbuiltTarget. For details check the
definition of UnbuiltTarget in CMakeLists.txt. Or you can include Configure.hpp and nlohmann/include/Json.hpp files in
your source directory.

Compile this hmake.cpp with hconfigure library and produce the binary named configure in your buildDir.

Create cache.hmake file in your buildDir. This provides basic information such as compilerPath and linkerPath to build
system. This file aslo holds cache variables(Described Later). Certain variables with right values should be defined in
it.

Run the configure binary.

Run the hbuild in current buildDir to build your project.

## Steps For Running HMake Easily

So, you only need hbuild and hconfigure to build your project. But to help you in the build process use following steps.

Create symbolic links for hhelper and hbuild binaries in /usr/local/bin/, so you don't have trouble running these
commands in other directories.

Create buildDir in the sourceDir.

Run hhelper in it. This will create the cache.hmake with right values

Run hhelper again. This will compile your hmake.cpp file and will also execute it.

Run hbuild to build the project.

So you see as compared to CMake

    cmake ../
    make

It is:

    hhelper
    hhelper
    hbuild

## How It Works

When you run configure binary compiled from hamke.cpp in buildlDir, it generates the json files having .hmake extension.
These files have information about how to build the project. hbuild then uses these files to build the project. It is
like CMake where you first generate the build system and then use that build system to build project. In HMake you
generate json files and then run hbuild in directory where these json files are present and hbuild will build the
project.

## Advantages Compared To Other Build Systems

Debugging the build system is easier. Because there is no lexing involved, it is plain C++, it is easy to Debug. Either
hconfigure is generating wrong hmake(json) files, or hbuild is not building correctly.

Obtaining info on how hbuild will build is easy. hmake files are easily readable. Some knowledge of hbuild is still
required though

Build System was easy to develop and is easy to extend. You can easily extend it or write a new one using the same
approach.

Plugins can be written to make it work with current IDE's. Target files have information about the include directories
which can be easily parsed.

## Speed

HMake will be **"THE FASTEST"**(Currently, can't confirm). Faster one of ninja or tup build algorithm will be
implemented in hbuild. Currently, hmake builds one file a time and doesn't cache builds either.

## Basic HMake File

    #include "Configure.hpp"

    int main() {
    Cache::initializeCache();
    Project project;
    ProjectVariant variant{};
    
    Executable app("app", variant);
    app.sourceFiles.emplace_back("main.cpp");
    
    variant.executables.push_back(app);
    project.projectVariants.push_back(variant);
    project.configure();
    }

## Examples

There are 8 examples in repo. Please see them.

To run any example, make a buildDir inside srcDir. And then run hhelper twice in that directory and hbuild once.

## Package Manager

HMake can build hmake packages. These packages have many variants from same source code. An Example is given in Example
7 and 8. Targets in these packages can specify their dependencies and consumer requirements. These packages are intended
for other developers but not for other users. Support for generating user focused packages such as msi and deb will also
be added. These packages are not best for developers because libraries in them can't specify their dependencies. Neither
is there mechanism for selecting a specific variant of that package.

If hmake gets popularity, I intend to make a package manager too. Which will download required hmake packages and their
dependencies. Can't confirm this though.

## Flags

Compiler Flags and linker flags depends on lot of factors. Such as compiler family, compiler version, library type,
target architecture, target os, target os version, configuration.

In hmake final compiler flags are sum of compilerFlagDependencies that have trickled up because of PUBLIC dependencyType
or those in flags member variable. flags member variable is initialized with the flags member variable of variant that
is passed while flag member variable of variant is initialized with global flag inline variable.

CompilerFlags class has a mechanism for specifying flags which allows either to be fully specific about flags or be less
specific. e.g.

`flags.compilerFlags[CompilerFamily::GCC][Version{10,2,0}][ConfigType::DEBUG] = "-g"`

Or you can be less specific e.g.

`flags.compilerFlags[ConfigType::RELEASE] = "-O3 -DNDEBUG"`

In the later case these flags are for all compilers and all versions. You have to register the compiler family in the
compilerFamilies in CompilerFlags class.

So, when cross-compilation and cross-platform support will be added, this technique can be helpful.

## License

No Liscense

## Support

If you like my project, Please support me.

My btc address:  bc1qnklt02vke3jl4a8e355pnuqsdftsr00ml07naf

If you do decide, do message me. I will send you gratification. If you don't like bitcoin and want to support, I will
setup some other channel soon.