#include "Configure.hpp"

int main(int argc, char **argv)
{
    // Following line will initialize the Cache class static members from cache.hmake. That's why if cache file does not
    // exist library does not work properly. When you run hhelper for first time cache.hmake file is created. This file
    // has list of all the compilers and linkers detected by hmake and the array indices defaulted to zero which
    // specifies which of them is
    //  the default. This also has the default LibraryType to use and default configuration to build. You can change
    //  these values before running hhelper second time. Currently, only static libraries supported. If you want to
    //  experiment
    // with library on other platforms, you will have to change the hhelper code to generate the right cache.hmake which
    // has right values for compiler and linker paths and family. Then you will have to add some code in
    // Flags::defaultFlags() function which will specify flags for that compiler and linker. This is explained later.
    // This way you can run it on other platforms. Currently, hhelper code is very basic and does not actually detect
    // anything. In the future, when it's ready then e.g. your use case is to only allow gcc and no other compiler. You
    // can write the following code snippet. So that in case more than one compiler are detected, use gcc. If gcc is not
    // detected then exit.
    /*
    bool compilerFound = false;
    for(int i=0;i<Cache::compilerArray.size(); ++i){
      if(Cache::compilerArray[i].compilerFamily == CompilerFamily::GCC){
        compilerFound = true;
        Cache::selectedCompilerArrayIndex = i;
      }
    }
    if(!compilerFound){
      cerr << "Compiler Not detected" << endl;
      exit(EXIT_FAILURE);
    }*/
    setBoolsAndSetRunDir(argc, argv);

    // There should be only one project variable. Project class has name and version members but because those are not
    // used currently, constructor does not require them. Later, I will show example of Package. There should also be
    // only one Package if you are using it.

    // ProjectVariant and PackageVariant are derived from Variant Class. A project can have as many as needed. Variant
    // is needed when your use case is such that e.g. you want your library compiled with debug and release flags to
    // exist both at a same time because you may use any at any time. So, you can make two different variants. Set their
    // configurationType member variable to different values. Add them in Project class member variable projectVariants
    // and configure the project. Now, you will have project with two different variants with different
    // configurationType. In order to do this in cmake, you need to configure the project twice. Here there is native
    // support. More about variants in Example.
    // Note that we passed it the above variant object. So that it will initialize its defaults from that variant
    // object which will initialize its defaults from Cache class static members. Executable and Library are derived
    // from Target class. First argument is targetName. targetName for every target should be different in every
    // variant. Note that it is different from outputName which is the name of output compiled. outputName can be
    // assigned for the following Target as: app.outputName = "HelloApp";

    // If you don't provide full path then relative path is converted into full path like following
    // Cache::sourceDirectory.path/path i.e. your path is taken relative to sourceDirectory.
    GetCppExe("app").cppSourceTarget->SOURCE_FILES("main.cpp");
    configureOrBuild();
    // This will be your last line. If you are doing packaging, then package.configure may be your last, where package
    // is object of Package.

    // Here is what you buildDir will look like
    // In buildDir, for every variant that you add in project, there is a directory created which has all the
    // targets included in that variant. So, there is directory 0 which is variant directory. In this variant
    // directory there is directory app. Notice app was targetName given in Executable Constructor. In this
    // app directory there is file app.hmake. This file has all the info to build the associated target.

    // hbuild should be run in directory having one of the following files.

    //{target}.hmake
    // This target will be built. Whenever a target is built it's LibraryDependencies are also built.
    // If it is a module then source is collected from all the module targets in the variant and all the source is
    // built.
    // LibraryDependencies are discussed later.

    // projectVariant.hmake
    // This variant will be built. Libraries and Executables added in the variant will be built.

    // project.hmake
    // The variants added in project will be built

    // package.hmake
    // The variants added in package will be built and also copied. That will be discussed later.

    // So, when package.configure() is called it generates all hmake files.
}
