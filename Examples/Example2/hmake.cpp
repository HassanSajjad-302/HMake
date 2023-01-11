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

    Variant variantRelase("Release");
    Library cat("Cat", variantRelase);
    cat.SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    Executable animal("Animal", variantRelase);
    animal.SOURCE_FILES("main.cpp").PUBLIC_LIBRARIES(&cat);
    configureOrBuild();
    // I suggest you see the directory structure of your buildDir.
}