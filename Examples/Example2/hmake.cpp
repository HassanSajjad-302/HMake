#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    /*    vector<Variant *> variants;
        Variant variantRelease("Release");
        Variant variantDebug("DEBUG");
        variants.emplace_back(&variantDebug);
        variants.emplace_back(&variantRelease);

        for (Variant *variant : variants)
        {
            Library cat("Cat", *variant);
            cat.SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

            Executable animal("Animal", *variant);
            animal.SOURCE_FILES("main.cpp").PUBLIC_LIBRARIES(&cat);
            variant->configure();
        }*/

    cache.libraryType = TargetType::LIBRARY_SHARED;
    LinkOrArchiveTarget *target = GetCppLib("Cat")
                                      .cppSourceTarget->SOURCE_FILES("Cat/src/Cat.cpp")
                                      .PUBLIC_INCLUDES("Cat/header")
                                      .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllexport)")
                                      .linkOrArchiveTarget;
    GetCppExe("Animal")
        .cppSourceTarget->SOURCE_FILES("main.cpp")
        .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllimport)")
        .linkOrArchiveTarget->publicLibs.emplace(target);
    configureOrBuild();
    // I suggest you see the directory structure of your buildDir.
}