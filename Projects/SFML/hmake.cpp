
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    string winStaticLibsDir = srcDir + "extlibs/libs-msvc-universal/";

    DSCPrebuilt<CPT> &flac = GetCPTTargetDSC("flac", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    flac.getSourceTarget().INTERFACE_INCLUDES("extlibs/headers/FLAC/").INTERFACE_COMPILE_DEFINITION("FLAC__NO_DLL");

    DSCPrebuilt<CPT> &freetype = GetCPTTargetDSC("freetype", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &ogg = GetCPTTargetDSC("ogg", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &openal32 = GetCPTTargetDSC("openal32", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    openal32.getSourceTarget().INTERFACE_INCLUDES("extlibs/headers/AL/");

    DSCPrebuilt<CPT> &vorbis = GetCPTTargetDSC("vorbis", winStaticLibsDir, TargetType::LIBRARY_STATIC);
    vorbis.getSourceTarget()
        .INTERFACE_INCLUDES("extlibs/headers/vorbis")
        .INTERFACE_COMPILE_DEFINITION("OV_EXCLUDE_STATIC_CALLBACKS");

    DSCPrebuilt<CPT> &vorbisenc = GetCPTTargetDSC("vorbisenc", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSCPrebuilt<CPT> &vorbisfile = GetCPTTargetDSC("vorbisfile", winStaticLibsDir, TargetType::LIBRARY_STATIC);

    DSC<CppSourceTarget> &system = GetCppSharedDSC("system");
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES("src/SFML/System/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/System/");
    systemCpp.MODULE_DIRECTORIES("src/SFML/System/Win32/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/System/Win32/");
    // winmm -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &network = GetCppSharedDSC("network");
    CppSourceTarget &networkCpp = network.getSourceTarget();
    networkCpp.MODULE_DIRECTORIES("src/SFML/Network/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Network/");
    networkCpp.MODULE_DIRECTORIES("src/SFML/Network/Win32/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Network/Win32/");
    // ws2_32  -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &window = GetCppSharedDSC("window").PUBLIC_LIBRARIES(&system);

    // Sources are specified considering SFML_OPENGL_ES = true; Otherwise two files shouldn't be specified EGLCheck.pp
    // and EglContext.cpp. Better if these files are moved to a new directory, so MODULE_DIRECTORIES or
    // SOURCE_DIRECTORIES could be conveniently used.
    CppSourceTarget &windowCpp = window.getSourceTarget();
    windowCpp.MODULE_DIRECTORIES("src/SFML/Window/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Window/");
    windowCpp.MODULE_DIRECTORIES("src/SFML/Window/Win32/", ".*cpp")
        .PRIVATE_HU_INCLUDES("src/SFML/Window/Win32/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_HU_INCLUDES("extlibs/headers/vulkan/");

    // Few files are assigned based on whether the OpenGL or EGL is used. if these could be moved to different
    // directories, then DIRECTORIES( functions could be used.
    windowCpp.MODULE_FILES("src/SFML/Window/Win32/CursorImpl.cpp", "src/SFML/Window/Win32/ClipboardImpl.cpp",
                           "src/SFML/Window/Win32/InputImpl.cpp", "src/SFML/Window/Win32/JoystickImpl.cpp",
                           "src/SFML/Window/Win32/SensorImpl.cpp", "src/SFML/Window/Win32/VideoModeImpl.cpp",
                           "src/SFML/Window/Win32/VulkanImplWin32.cpp", "src/SFML/Window/Win32/WindowImplWin32.cpp",
                           "src/SFML/Window/Clipboard.cpp", "src/SFML/Window/Context.cpp", "src/SFML/Window/Cursor.cpp",
                           "src/SFML/Window/GlContext.cpp", "src/SFML/Window/GlResource.cpp",
                           "src/SFML/Window/Joystick.cpp", "src/SFML/Window/JoystickManager.cpp",
                           "src/SFML/Window/Keyboard.cpp", "src/SFML/Window/Mouse.cpp", "src/SFML/Window/Touch.cpp",
                           "src/SFML/Window/Sensor.cpp", "src/SFML/Window/SensorManager.cpp",
                           "src/SFML/Window/VideoMode.cpp", "src/SFML/Window/Vulkan.cpp", "src/SFML/Window/Window.cpp",
                           "src/SFML/Window/WindowBase.cpp", "src/SFML/Window/WindowImpl.cpp");

    // SFML_USE_DRM = false
    // OpenGL library is set by two variables OPENGL_INCLUDE_DIR and OPEN_gl_LIBRARY
    // Where are these variables set.
    // winmm and gdi32 are being linked

    DSC<CppSourceTarget> &graphics = GetCppSharedDSC("graphics").PUBLIC_LIBRARIES(&window);

    CppSourceTarget &graphicsCpp = graphics.getSourceTarget();
    graphicsCpp.MODULE_DIRECTORIES("src/SFML/Graphics/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Graphics/");
    graphicsCpp.PRIVATE_HU_INCLUDES("extlibs/headers/stb_image/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/freetype2")
        .PRIVATE_COMPILE_DEFINITION("STBI_FAILURE_USERMSG");
    // legacy_stdio_definitions.lib   --> Not declared or mentioned anywhere else
    // Freetype

    DSC<CppSourceTarget> &audio =
        GetCppSharedDSC("audio").PUBLIC_LIBRARIES(&system).PRIVATE_LIBRARIES(&openal32, &vorbis, &flac);
    CppSourceTarget &audioCpp = audio.getSourceTarget();
    audioCpp.MODULE_DIRECTORIES("src/SFML/Audio/", ".*cpp")
        .PRIVATE_HU_INCLUDES("src/SFML/Audio/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/minimp3");

    DSC<CppSourceTarget> &main_ = GetCppStaticDSC("main");
    main_.getSourceTarget().MODULE_FILES("src/SFML/Main/MainWin32.cpp");

    for (const CppSourceTarget &cppSourceTarget : targets<CppSourceTarget>)
    {
        const_cast<CppSourceTarget &>(cppSourceTarget).PUBLIC_INCLUDES("include/");
    }
}

void buildSpecification()
{
    Configuration &configuration = GetConfiguration("Debug");
    configuration.ASSIGN(TreatModuleAsSource::YES);
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