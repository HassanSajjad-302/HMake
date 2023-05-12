
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    string winStaticLibsDir = srcDir + "extlibs/libs-msvc-universal/x64/";

    DSCPrebuilt<CPT> &flac = configuration.GetCPTTargetDSC("flac", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    flac.getSourceTarget().INTERFACE_INCLUDES("extlibs/headers/FLAC/").INTERFACE_COMPILE_DEFINITION("FLAC__NO_DLL");

    DSCPrebuilt<CPT> &freetype =
        configuration.GetCPTTargetDSC("freetype", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &ogg = configuration.GetCPTTargetDSC("ogg", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &openal32 =
        configuration.GetCPTTargetDSC("openal32", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    openal32.getSourceTarget().INTERFACE_INCLUDES("extlibs/headers/AL/");

    DSCPrebuilt<CPT> &vorbis = configuration.GetCPTTargetDSC("vorbis", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    vorbis.getSourceTarget()
        .INTERFACE_INCLUDES("extlibs/headers/")
        .INTERFACE_COMPILE_DEFINITION("OV_EXCLUDE_STATIC_CALLBACKS");

    DSCPrebuilt<CPT> &vorbisenc =
        configuration.GetCPTTargetDSC("vorbisenc", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &vorbisfile =
        configuration.GetCPTTargetDSC("vorbisfile", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToHUIncludes();

    DSC<CppSourceTarget> &system = configuration.GetCppSharedDSC("system");
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES_RG("src/SFML/System/", ".*cpp").PRIVATE_INCLUDES("src/");
    systemCpp.MODULE_DIRECTORIES_RG("src/SFML/System/Win32/", ".*cpp")
        .PRIVATE_INCLUDES("src/")
        .PRIVATE_COMPILE_DEFINITION("SFML_SYSTEM_EXPORTS");
    system.linkOrArchiveTarget->requirementLinkerFlags += "winmm.lib ";
    // winmm -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &network = configuration.GetCppSharedDSC("network").PUBLIC_LIBRARIES(&system);
    CppSourceTarget &networkCpp = network.getSourceTarget();
    networkCpp.MODULE_DIRECTORIES_RG("src/SFML/Network/", ".*cpp").PRIVATE_INCLUDES("src/");
    networkCpp.MODULE_DIRECTORIES_RG("src/SFML/Network/Win32/", ".*cpp")
        .PRIVATE_INCLUDES("src/")
        .PRIVATE_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_COMPILE_DEFINITION("SFML_NETWORK_EXPORTS");
    network.linkOrArchiveTarget->requirementLinkerFlags += "ws2_32.lib ";
    // ws2_32  -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &window = configuration.GetCppSharedDSC("window").PUBLIC_LIBRARIES(&system);

    // Sources are specified considering SFML_OPENGL_ES = true; Otherwise two files shouldn't be specified EGLCheck.pp
    // and EglContext.cpp. Better if these files are moved to a new directory, so MODULE_DIRECTORIES_RG or
    // SOURCE_DIRECTORIES_RG could be conveniently used.
    CppSourceTarget &windowCpp = window.getSourceTarget();
    windowCpp.MODULE_DIRECTORIES_RG("src/SFML/Window/", ".*cpp").PRIVATE_INCLUDES("src/");
    windowCpp.MODULE_DIRECTORIES_RG("src/SFML/Window/Win32/", ".*cpp")
        .PRIVATE_INCLUDES("src/")
        .PRIVATE_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_INCLUDES("extlibs/headers/vulkan/")
        .PRIVATE_COMPILE_DEFINITION("SFML_WINDOW_EXPORTS");

    // Few files are assigned based on whether the OpenGL or EGL is used. if these could be moved to different
    // directories, then DIRECTORIES_RG( functions could be used.
    windowCpp.MODULE_FILES(
        "src/SFML/Window/Win32/CursorImpl.cpp", "src/SFML/Window/Win32/ClipboardImpl.cpp",
        "src/SFML/Window/Win32/InputImpl.cpp", "src/SFML/Window/Win32/JoystickImpl.cpp",
        "src/SFML/Window/Win32/SensorImpl.cpp", "src/SFML/Window/Win32/VideoModeImpl.cpp",
        "src/SFML/Window/Win32/VulkanImplWin32.cpp", "src/SFML/Window/Win32/WindowImplWin32.cpp",
        "src/SFML/Window/Clipboard.cpp", "src/SFML/Window/Context.cpp", "src/SFML/Window/Cursor.cpp",
        "src/SFML/Window/GlContext.cpp", "src/SFML/Window/GlResource.cpp", "src/SFML/Window/Joystick.cpp",
        "src/SFML/Window/JoystickManager.cpp", "src/SFML/Window/Keyboard.cpp", "src/SFML/Window/Mouse.cpp",
        "src/SFML/Window/Touch.cpp", "src/SFML/Window/Sensor.cpp", "src/SFML/Window/SensorManager.cpp",
        "src/SFML/Window/VideoMode.cpp", "src/SFML/Window/Vulkan.cpp", "src/SFML/Window/Window.cpp",
        "src/SFML/Window/WindowBase.cpp", "src/SFML/Window/WindowImpl.cpp", "src/SFML/Window/Win32/WglContext.cpp");

    // SFML_USE_DRM = false
    // OpenGL library is set by two variables OPENGL_INCLUDE_DIR and OPEN_gl_LIBRARY
    // Where are these variables set.
    // winmm and gdi32 are being linked
    window.linkOrArchiveTarget->requirementLinkerFlags += "winmm.Lib gdi32.lib user32.lib advapi32.lib opengl32.lib ";

    DSC<CppSourceTarget> &graphics =
        configuration.GetCppSharedDSC("graphics").PUBLIC_LIBRARIES(&window).PRIVATE_LIBRARIES(&freetype);

    CppSourceTarget &graphicsCpp = graphics.getSourceTarget();
    graphicsCpp.requirementIncludes.clear();
    graphicsCpp.MODULE_DIRECTORIES_RG("src/SFML/Graphics/", ".*cpp").PRIVATE_INCLUDES("src/");
    graphicsCpp.PRIVATE_INCLUDES("extlibs/headers/stb_image/")
        .PRIVATE_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_INCLUDES("extlibs/headers/freetype2")
        .PRIVATE_COMPILE_DEFINITION("STBI_FAILURE_USERMSG")
        .PRIVATE_COMPILE_DEFINITION("SFML_GRAPHICS_EXPORTS");
    // legacy_stdio_definitions.lib   --> Not declared or mentioned anywhere else
    // Freetype

    for (const string &str : toolsCache.vsTools[cache.selectedCompilerArrayIndex].includeDirectories)
    {
        InclNode::emplaceInList(graphicsCpp.requirementIncludes, Node::getNodeFromString(str, false), true, true);
    }

    DSC<CppSourceTarget> &audio = configuration.GetCppSharedDSC("audio").PUBLIC_LIBRARIES(&system).PRIVATE_LIBRARIES(
        &openal32, &ogg, &vorbis, &flac);
    CppSourceTarget &audioCpp = audio.getSourceTarget();
    audioCpp.MODULE_DIRECTORIES_RG("src/SFML/Audio/", ".*cpp")
        .PRIVATE_INCLUDES("src/")
        .PRIVATE_INCLUDES("extlibs/headers/minimp3")
        .PRIVATE_COMPILE_DEFINITION("SFML_AUDIO_EXPORTS");

    DSC<CppSourceTarget> &main_ = configuration.GetCppStaticDSC("main");
    main_.getSourceTarget().MODULE_FILES("src/SFML/Main/MainWin32.cpp").PRIVATE_INCLUDES("src/");

    for (const CppSourceTarget &cppSourceTarget : targets<CppSourceTarget>)
    {
        const_cast<CppSourceTarget &>(cppSourceTarget).PUBLIC_INCLUDES("include/");
    }

    configuration.setModuleScope(&main_.getSourceTarget());
}

void buildSpecification()
{
    // srcDir = path( "../../SFML/").lexically_normal().generic_string();
    Configuration &configuration = GetConfiguration("Debug");
    configuration.ASSIGN(TreatModuleAsSource::YES, CxxSTD::V_17);
    configurationSpecification(configuration);

    // SFML Window
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif