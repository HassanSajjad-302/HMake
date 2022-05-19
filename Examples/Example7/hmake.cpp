#include "Configure.hpp"

int main()
{
    Cache::initializeCache();

    PackageVariant variantRelease;

    Library dog("Dog", variantRelease);
    dog.sourceFiles.emplace_back("Dog/src/Dog.cpp");
    dog.includeDirectoryDependencies.push_back(
        IDD{.includeDirectory = Directory("Dog/header"), .dependencyType = DependencyType::PUBLIC});
#ifdef _WIN32
    PLibrary catRelease("../Example2/Build/0/Cat/Cat.lib", LibraryType::STATIC);
    PLibrary catDebug("../Example2/Build/1/Cat/Cat.lib", LibraryType::STATIC);
#elif
    PLibrary catRelease("../Example2/Build/0/Cat/libCat.a", LibraryType::STATIC);
    PLibrary catDebug("../Example2/Build/1/Cat/libCat.a", LibraryType::STATIC);
#endif
    catRelease.includeDirectoryDependencies.emplace_back("../Example2/Cat/header/");
    dog.libraryDependencies.emplace_back(catRelease, DependencyType::PUBLIC);
    variantRelease.libraries.push_back(dog);

    catDebug.includeDirectoryDependencies.emplace_back("../Example2/Cat/header/");

    dog.libraryDependencies.clear();

    // Following is how to make a hmake package. PackageVariant is like ProjectVariant. Libraries are added in it.
    // And then variants are added in Package. However, there is one difference, We also specify the json with
    // every variant. That json should be unique. Consumer will select a target that is in some variant. That
    // target will also have a hmake file associated with it. Which will describe what other include directories,
    // compiler flags, linker flags, compile options and library dependencies, the consumer of that target
    // will be built with. All the library dependencies should be present in the same variant. When a library is
    //  added in a variant, its public dependencies are also added in that variant. If a library is PLibrary then it
    // is added just like a normal library. However, if it is PPLibrary(Packaged Prebuilt Library(Introduced later),
    // then you have the option to either just refer it(i.e. the package name, the variant of the package and the target
    // name is added as dependency(library is not copied) in the package or include that library just like
    // other libraries in the variant(library is copied in package).

    // Consumption of packages is shown in next example. This json mechanism provides the consumer to produce
    // different binaries having different consumption requirements. While the consumer can select the one it
    // needs from many variants of the package.
    // e.g. For this package following will be present in hmake package on it's installed location in cpackage.hmake.
    /*
    "VARIANTS": [
    {
      "CONFIGURATION": "DEBUG",
          "INDEX": "0"
    },
    {
      "CONFIGURATION": "RELEASE",
          "INDEX": "1"
    }
    ]
    */
    // Now the consumer will specify either of the two jsons and that variant will be selected. And binaries in
    // that variant will be used.
    // A good practical json could be
    /*
    {
      "CONFIGURATION": "RELEASE",
      "LIBRARY_TYPE" : "STATIC",
      "HOST_ARCH" : amd64
      "HOST_OS" : LINUX
      "TARGET_ARCH" : ARMv8-a
      "TARGET_OS" : ANDROID
      "LIBRARY_FOO_USES_LIBRARY_BAR" : false
      "LIBRARY_FOO_USES_LIBRARY_CAT" : true
          "INDEX": "1"
    }
   */

    // Note: producer will have to write json by hand and consumer will also need to fully specify them.
    PackageVariant variatDebug;
    variatDebug.configurationType = ConfigType::DEBUG;

    dog.assignDifferentVariant(variatDebug);
    dog.libraryDependencies.emplace_back(catDebug, DependencyType::PUBLIC);

    variatDebug.libraries.push_back(dog);

    Package package("Pets");
    variatDebug.uniqueJson["CONFIGURATION"] = "DEBUG";
    variantRelease.uniqueJson["CONFIGURATION"] = "RELEASE";
    package.packageVariants.push_back(variatDebug);
    package.packageVariants.push_back(variantRelease);

    // This will produce the package files in Cache::configureDirectory.path/"Package". That is why a target
    // with Package name is not allowed. You will have to run hbuild in Package directory to build the package.
    // Package will be copied by default. If you just want to build it. Set PACKAGE_COPY to false in cache.hmake
    // or set Cache::copyPackage to false before this line.
    // Package class also has a member bool cacheCommonIncludeDirs which is defaulted to true. If set to false
    // each target in every variant will have it's set of includeDirs copied in include. If set to true
    // common includes are copied just once and that too in Commons/ directory in package's copy path. That
    // saves space
    package.configure();

    // Package Consumption is shown in next example. Package is built in Package directory in
    // Cache::configDirectory.path I recommend you see it's contents in this example.
}