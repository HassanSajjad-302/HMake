
#include "Configure.hpp"

template <typename... T> void initializeTargets(DSC<CppSourceTarget> &target, T &...targets)
{
    CppSourceTarget &t = target.getSourceTarget();
    string str = t.name.substr(0, t.name.size() - 4); // Removing -cpp from the name
    t.MODULE_DIRECTORIES_RG("src/SFML/" + str + "/", ".*cpp")
        .PRIVATE_INCLUDES("src/")
        .PUBLIC_INCLUDES("include/")
        .HU_DIRECTORIES("include/SFML/" + str + "/");

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &configuration)
{
    configuration.archiving = true;

    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().setModuleScope().assignStandardIncludesToHUIncludes();

    configuration.moduleScope = stdhu.getSourceTargetPointer();

    string winStaticLibsDir = srcDir + "extlibs/libs-msvc-universal/x64/";

    DSC<CPT> &flac = configuration.GetCPTStaticDSC_P("flac", winStaticLibsDir);
    flac.getSourceTarget().INTERFACE_COMPILE_DEFINITION("FLAC__NO_DLL");

    DSC<CPT> &freetype = configuration.GetCPTStaticDSC_P("freetype", winStaticLibsDir);

    DSC<CPT> &openal32 = configuration.GetCPTStaticDSC_P("openal32", winStaticLibsDir);

    DSC<CPT> &ogg = configuration.GetCPTStaticDSC_P("ogg", winStaticLibsDir);

    DSC<CPT> &vorbis = configuration.GetCPTStaticDSC_P("vorbis", winStaticLibsDir);
    vorbis.getSourceTarget().INTERFACE_COMPILE_DEFINITION("OV_EXCLUDE_STATIC_CALLBACKS");

    DSC<CPT> &vorbisenc = configuration.GetCPTStaticDSC_P("vorbisenc", winStaticLibsDir);

    DSC<CPT> &vorbisfile = configuration.GetCPTStaticDSC_P("vorbisfile", winStaticLibsDir);

    DSC<CppSourceTarget> &system = configuration.GetCppSharedDSC("system").PRIVATE_LIBRARIES(&stdhu);
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES_RG("src/SFML/System/Win32/", ".*cpp")
        .PRIVATE_COMPILE_DEFINITION("SFML_SYSTEM_EXPORTS");
    system.getLinkOrArchiveTarget().requirementLinkerFlags += "winmm.lib ";
    // winmm -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &network =
        configuration.GetCppSharedDSC("network").PUBLIC_LIBRARIES(&system).PRIVATE_LIBRARIES(&stdhu);
    CppSourceTarget &networkCpp = network.getSourceTarget();
    networkCpp.MODULE_DIRECTORIES_RG("src/SFML/Network/Win32/", ".*cpp")
        .PRIVATE_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_COMPILE_DEFINITION("SFML_NETWORK_EXPORTS");
    network.getLinkOrArchiveTarget().requirementLinkerFlags += "ws2_32.lib ";
    // ws2_32  -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &window =
        configuration.GetCppSharedDSC("window").PUBLIC_LIBRARIES(&system).PRIVATE_LIBRARIES(&stdhu);

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
    window.getLinkOrArchiveTarget().requirementLinkerFlags +=
        "winmm.Lib gdi32.lib user32.lib advapi32.lib opengl32.lib ";

    DSC<CppSourceTarget> &graphics =
        configuration.GetCppSharedDSC("graphics").PUBLIC_LIBRARIES(&window).PRIVATE_LIBRARIES(&freetype, &stdhu);

    CppSourceTarget &graphicsCpp = graphics.getSourceTarget();
    graphicsCpp.requirementIncludes.clear();
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
        &stdhu, &openal32, &ogg, &vorbis, &flac, &vorbisenc, &vorbisfile);
    CppSourceTarget &audioCpp = audio.getSourceTarget();
    audioCpp.PRIVATE_INCLUDES("extlibs/headers/minimp3")
        .PRIVATE_INCLUDES("extlibs/headers/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/FLAC/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/AL/")
        .PRIVATE_COMPILE_DEFINITION("SFML_AUDIO_EXPORTS");

    DSC<CppSourceTarget> &main_ = configuration.GetCppStaticDSC("main");
    main_.getSourceTarget()
        .MODULE_FILES("src/SFML/Main/MainWin32.cpp")
        .PRIVATE_INCLUDES("src/")
        .PUBLIC_INCLUDES("include/");

    initializeTargets(graphics, audio, system, network);

    configuration.archiving = false;

    DSC<CppSourceTarget> &example = configuration.GetCppExeDSC("Example").PRIVATE_LIBRARIES(&graphics);
    example.getSourceTarget().MODULE_FILES("examples/win32/Win32.cpp");
    example.getLinkOrArchiveTarget().requirementLinkerFlags += "user32.lib ";

    GetRoundZeroUpdateBTarget(
        [&](Builder &builder, unsigned short round, BTarget &bTarget) {
            if (bTarget.getRealBTarget(0).exitStatus == EXIT_SUCCESS)
            {
                create_directory(path(example.prebuiltLinkOrArchiveTarget->getActualOutputPath()).parent_path() /
                                 "resources");
                std::filesystem::copy(srcDir + "examples/win32/resources/",
                                      path(example.prebuiltLinkOrArchiveTarget->getActualOutputPath()).parent_path() /
                                          "resources",
                                      std::filesystem::copy_options::update_existing);
            }
        },
        example.getLinkOrArchiveTarget());
}

void buildSpecification()
{
    Configuration &configuration = GetConfiguration("Debug");
    configuration.ASSIGN(TreatModuleAsSource::NO, TranslateInclude::YES, CxxSTD::V_LATEST);
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