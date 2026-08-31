#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    auto makeApps = [&] {
        const string str = config.targetType == TargetType::LIBRARY_STATIC
                               ? "-Static"
                               : config.targetType == TargetType::LIBRARY_SHARED ? "-Shared" : "-Object";

        DSC<CppTarget> *cat;
        if (config.targetType == TargetType::LIBRARY_OBJECT)
        {
            cat = &config.getCppTargetDSC("Cat" + str, true, "CAT_EXPORT");
            cat->getSourceTarget()
                .sourceFiles("../Example4/Cat/src/Cat.cpp")
                .publicIncludes("../Example4/Cat/header");
        }
        else
        {
            Node *outputDir = bsMode == BSMode::CONFIGURE
                                  ? Node::getNode<PathType::NEITHER>("../Example4/Build/Release/Cat" + str, false, false)
                                  : nullptr;
            cat = &config.getCppTargetDSC_P("Cat" + str, outputDir, true, "CAT_EXPORT");
            cat->getSourceTarget().interfaceIncludes("../Example4/Cat/header");
        }

        DSC<CppTarget> &dog = config.getCppTargetDSC("Dog" + str, true, "DOG_EXPORT");
        dog.publicDeps(*cat).getSourceTarget().sourceFiles("Dog/src/Dog.cpp").publicIncludes("Dog/header");

        DSC<CppTarget> &dog2 = config.getCppTargetDSC("Dog2" + str, true, "DOG2_EXPORT");
        dog2.privateDeps(*cat).getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

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
    config.targetType = TargetType::LIBRARY_OBJECT;
    makeApps();

    // A PRIVATE library requirement declared by an outputless producer must reach the executable that eventually
    // consumes its objects.
    DSC<CppTarget> &mixedPrivateCat = config.getCppStaticDSC("Cat-MixedPrivate", true, "CAT_EXPORT");
    mixedPrivateCat.getSourceTarget()
        .sourceFiles("../Example4/Cat/src/Cat.cpp")
        .publicIncludes("../Example4/Cat/header");

    DSC<CppTarget> &mixedPrivateDog =
        config.getCppObjectDSC("Dog-MixedPrivate", true, "DOG2_EXPORT").privateDeps(mixedPrivateCat);
    mixedPrivateDog.getSourceTarget().sourceFiles("Dog2/src/Dog.cpp").publicIncludes("Dog2/header");

    DSC<CppTarget> &mixedPrivateApp = config.getCppExeDSC("App-MixedPrivate").privateDeps(mixedPrivateDog);
    mixedPrivateApp.getLOAT().setOutputName("app");
    mixedPrivateApp.getSourceTarget().sourceFiles("main2.cpp");

    // An INTERFACE library requirement is not consumed by the object target itself, but becomes required when a
    // downstream executable consumes that target's interface.
    DSC<CppTarget> &mixedInterfaceCat = config.getCppSharedDSC("Cat-MixedInterface", true, "CAT_EXPORT");
    mixedInterfaceCat.getSourceTarget()
        .sourceFiles("../Example4/Cat/src/Cat.cpp")
        .publicIncludes("../Example4/Cat/header");

    DSC<CppTarget> &mixedInterfaceBridge = config.getCppObjectDSC("Bridge-MixedInterface");
    mixedInterfaceBridge.interfaceDeps(mixedInterfaceCat);

    DSC<CppTarget> &mixedInterfaceApp = config.getCppExeDSC("App-MixedInterface").privateDeps(mixedInterfaceBridge);
    mixedInterfaceApp.getLOAT().setOutputName("app");
    mixedInterfaceApp.getSourceTarget().sourceFiles("../Example4/main.cpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
