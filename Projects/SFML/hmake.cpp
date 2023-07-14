
#include "Configure.hpp"

template <typename... T>
void initializeTargets(TargetType targetType, DSC<CppSourceTarget> &target, bool win32, T &&...targets)
{
    CppSourceTarget &t = target.getSourceTarget();
    string str = t.name.substr(0, t.name.size() - 4); // Removing -cpp from the name
    t.MODULE_DIRECTORIES_RG("src/SFML/" + str, ".*cpp")
        .PRIVATE_INCLUDES("src")
        .PUBLIC_INCLUDES("include")
        .HU_DIRECTORIES("src/SFML/" + str)
        .HU_DIRECTORIES("include/SFML/" + str);

    if (win32)
    {
        t.MODULE_DIRECTORIES_RG("src/SFML/" + str + "/Win32", ".*cpp").HU_DIRECTORIES("src/SFML/" + str + "/Win32");
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        transform(str.begin(), str.end(), str.begin(), ::toupper);
        str += "_EXPORTS";
        t.PRIVATE_COMPILE_DEFINITION("SFML_" + str);
    }
    else
    {
        t.PRIVATE_COMPILE_DEFINITION("SFML_STATIC");
    }

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targetType, std::forward<T>(targets)...);
    }
}

void configurationSpecification(Configuration &configuration)
{
    configuration.archiving = true;

    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().setModuleScope().assignStandardIncludesToHUIncludes();

    configuration.moduleScope = stdhu.getSourceTargetPointer();

    string winStaticLibsDir = srcDir + "extlibs/libs-msvc-universal/x64/";

    DSC<CppSourceTarget, true> &openal32 = configuration.GetCppStaticDSC_P("openal32", winStaticLibsDir);
    openal32.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/AL");

    DSC<CppSourceTarget, true> &flac = configuration.GetCppStaticDSC_P("flac", winStaticLibsDir);
    flac.getSourceTarget().PUBLIC_COMPILE_DEFINITION("FLAC__NO_DLL").PUBLIC_HU_INCLUDES("extlibs/headers/FLAC");

    DSC<CppSourceTarget, true> &freetype = configuration.GetCppStaticDSC_P("freetype", winStaticLibsDir);

    DSC<CppSourceTarget, false> &glad = configuration.GetCppObjectDSC("glad");
    glad.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/glad/include");

    DSC<CppSourceTarget, false> &minimp3 = configuration.GetCppObjectDSC("minimp3");
    minimp3.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/minimp3");

    DSC<CppSourceTarget, true> &ogg = configuration.GetCppStaticDSC_P("ogg", winStaticLibsDir);
    ogg.getSourceTarget()
        .HU_DIRECTORIES("extlibs/headers/ogg")
        .PRIVATE_INCLUDES("extlibs/headers")
        .INTERFACE_INCLUDES("extlibs/headers");

    DSC<CppSourceTarget, false> &stb_image = configuration.GetCppObjectDSC("stb_image");
    stb_image.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/stb_image");

    DSC<CppSourceTarget, true> &vorbis =
        configuration.GetCppStaticDSC_P("vorbis", winStaticLibsDir).PRIVATE_LIBRARIES(&ogg);
    vorbis.getSourceTarget()
        .INTERFACE_COMPILE_DEFINITION("OV_EXCLUDE_STATIC_CALLBACKS")
        .PUBLIC_HU_INCLUDES("extlibs/headers/vorbis");

    DSC<CppSourceTarget, true> &vorbisenc = configuration.GetCppStaticDSC_P("vorbisenc", winStaticLibsDir);

    DSC<CppSourceTarget, true> &vorbisfile = configuration.GetCppStaticDSC_P("vorbisfile", winStaticLibsDir);

    DSC<CppSourceTarget, false> &vulkan = configuration.GetCppObjectDSC("vulkan");
    vulkan.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/vulkan");

    DSC<CppSourceTarget> &system =
        configuration.GetCppTargetDSC("system", configuration.targetType).PRIVATE_LIBRARIES(&stdhu);
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES_RG("src/SFML/System/Win32", ".*cpp");
    if (configuration.targetType != TargetType::LIBRARY_OBJECT)
    {
        system.getLinkOrArchiveTarget().requirementLinkerFlags += "winmm.lib ";
    }
    // winmm -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &network = configuration.GetCppTargetDSC("network", configuration.targetType)
                                        .PUBLIC_LIBRARIES(&system)
                                        .PRIVATE_LIBRARIES(&stdhu, &glad);
    CppSourceTarget &networkCpp = network.getSourceTarget();
    networkCpp.MODULE_DIRECTORIES_RG("src/SFML/Network/Win32", ".*cpp");
    if (configuration.targetType != TargetType::LIBRARY_OBJECT)
    {
        network.getLinkOrArchiveTarget().requirementLinkerFlags += "ws2_32.lib ";
    }
    // ws2_32  -> Not declared or mentioned anywhere else

    DSC<CppSourceTarget> &window = configuration.GetCppTargetDSC("window", configuration.targetType)
                                       .PUBLIC_LIBRARIES(&system)
                                       .PRIVATE_LIBRARIES(&stdhu, &glad, &vulkan);

    // Sources are specified considering SFML_OPENGL_ES = true; Otherwise two files shouldn't be specified EGLCheck.pp
    // and EglContext.cpp. Better if these files are moved to a new directory, so MODULE_DIRECTORIES_RG or
    // SOURCE_DIRECTORIES_RG could be conveniently used.
    CppSourceTarget &windowCpp = window.getSourceTarget();
    windowCpp.MODULE_DIRECTORIES_RG("src/SFML/Window", ".*cpp").PRIVATE_INCLUDES("src");
    windowCpp.MODULE_DIRECTORIES_RG("src/SFML/Window/Win32", ".*cpp").PRIVATE_INCLUDES("src");

    // Few files are assigned based on whether the OpenGL or EGL is used. if these could be moved to different
    // directories, then DIRECTORIES_RG( functions could be used.
    windowCpp
        .MODULE_FILES("src/SFML/Window/Win32/CursorImpl.cpp", "src/SFML/Window/Win32/ClipboardImpl.cpp",
                      "src/SFML/Window/Win32/InputImpl.cpp", "src/SFML/Window/Win32/JoystickImpl.cpp",
                      "src/SFML/Window/Win32/SensorImpl.cpp", "src/SFML/Window/Win32/VideoModeImpl.cpp",
                      "src/SFML/Window/Win32/VulkanImplWin32.cpp", "src/SFML/Window/Win32/WindowImplWin32.cpp",
                      "src/SFML/Window/Clipboard.cpp", "src/SFML/Window/Context.cpp", "src/SFML/Window/Cursor.cpp",
                      "src/SFML/Window/GlContext.cpp", "src/SFML/Window/GlResource.cpp", "src/SFML/Window/Joystick.cpp",
                      "src/SFML/Window/JoystickManager.cpp", "src/SFML/Window/Keyboard.cpp",
                      "src/SFML/Window/Mouse.cpp", "src/SFML/Window/Touch.cpp", "src/SFML/Window/Sensor.cpp",
                      "src/SFML/Window/SensorManager.cpp", "src/SFML/Window/VideoMode.cpp",
                      "src/SFML/Window/Vulkan.cpp", "src/SFML/Window/Window.cpp", "src/SFML/Window/WindowBase.cpp",
                      "src/SFML/Window/WindowImpl.cpp", "src/SFML/Window/Win32/WglContext.cpp")
        .HU_DIRECTORIES("src/SFML/Window")
        .HU_DIRECTORIES("src/SFML/Window/Win32")
        .HU_DIRECTORIES("include/SFML/Window");

    // SFML_USE_DRM = false
    // OpenGL library is set by two variables OPENGL_INCLUDE_DIR and OPEN_gl_LIBRARY
    // Where are these variables set.
    // winmm and gdi32 are being linked
    if (configuration.targetType != TargetType::LIBRARY_OBJECT)
    {
        window.getLinkOrArchiveTarget().requirementLinkerFlags +=
            "winmm.Lib gdi32.lib user32.lib advapi32.lib opengl32.lib ";
    }

    DSC<CppSourceTarget> &graphics = configuration.GetCppTargetDSC("graphics", configuration.targetType)
                                         .PUBLIC_LIBRARIES(&window)
                                         .PRIVATE_LIBRARIES(&stdhu, &freetype, &glad, &stb_image);

    CppSourceTarget &graphicsCpp = graphics.getSourceTarget();
    graphicsCpp.requirementIncludes.clear();
    graphicsCpp.PRIVATE_HU_INCLUDES("extlibs/headers/freetype2").PRIVATE_COMPILE_DEFINITION("STBI_FAILURE_USERMSG");
    // legacy_stdio_definitions.lib   --> Not declared or mentioned anywhere else
    // Freetype

    for (const string &str : toolsCache.vsTools[cache.selectedCompilerArrayIndex].includeDirectories)
    {
        InclNode::emplaceInList(graphicsCpp.requirementIncludes, Node::getNodeFromNonNormalizedPath(str, false), true,
                                true);
    }

    DSC<CppSourceTarget> &audio =
        configuration.GetCppTargetDSC("audio", configuration.targetType)
            .PUBLIC_LIBRARIES(&system)
            .PRIVATE_LIBRARIES(&stdhu, &openal32, &vorbis, &vorbisenc, &vorbisfile, &flac, &minimp3);

    CppSourceTarget &audioCpp = audio.getSourceTarget();
    audioCpp.PRIVATE_INCLUDES("extlibs/headers");

    initializeTargets(configuration.targetType, graphics, false, audio, false, system, true, network, true);

    graphics.getSourceTarget().moduleSourceFileDependencies.erase(SMFile(
        graphics.getSourceTargetPointer(), Node::getNodeFromNonNormalizedPath("src/SFML/Graphics/Font.cpp", true)));
    graphics.getSourceTarget().SOURCE_FILES("src/SFML/Graphics/Font.cpp");

    configuration.archiving = false;

    DSC<CppSourceTarget> &example = configuration.GetCppExeDSC("Example").PRIVATE_LIBRARIES(&graphics);
    example.getSourceTarget().MODULE_FILES("examples/win32/Win32.cpp");
    if (configuration.targetType == TargetType::LIBRARY_STATIC)
    {
        window.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_STATIC");
        example.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_STATIC");
    }
    else
    {
        window.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_WINDOW_EXPORTS");
    }
    example.getLinkOrArchiveTarget().requirementLinkerFlags += "user32.lib ";
    example.getLinkOrArchiveTarget().requirementLinkerFlags +=
        "winmm.Lib gdi32.lib user32.lib advapi32.lib opengl32.lib ";
    GetRoundZeroUpdateBTarget(
        [&](Builder &builder, BTarget &bTarget) {
            if (bTarget.realBTargets[0].exitStatus == EXIT_SUCCESS)
            {
                create_directory(path(example.getLinkOrArchiveTarget().getActualOutputPath()).parent_path() /
                                 "resources");
                std::filesystem::copy(srcDir + "examples/win32/resources",
                                      path(example.getLinkOrArchiveTarget().getActualOutputPath()).parent_path() /
                                          "resources",
                                      std::filesystem::copy_options::update_existing);
            }
        },
        example.getLinkOrArchiveTarget());
}

void buildSpecification()
{
    Configuration &modules = GetConfiguration("modules");
    modules.linkerFeatures.linker =
        Linker{BTFamily::MSVC, Version{},
               path(R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\lld-link.exe)")};
    modules.ASSIGN(TreatModuleAsSource::NO, TargetType::LIBRARY_OBJECT, TranslateInclude::YES, CxxSTD::V_LATEST);

    Configuration &conventional = GetConfiguration("conventional");
    conventional.linkerFeatures.linker =
        Linker{BTFamily::MSVC, Version{},
               path(R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\lld-link.exe)")};
    conventional.ASSIGN(TreatModuleAsSource::YES, TargetType::LIBRARY_SHARED, CxxSTD::V_LATEST);

    if (equivalent(path(configureDir), std::filesystem::current_path()))
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            configurationSpecification(const_cast<Configuration &>(configuration));
        }
    }
    else
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            if (const_cast<Configuration &>(configuration).getSelectiveBuildChildDir())
            {
                configurationSpecification(const_cast<Configuration &>(configuration));
            }
        }
    }
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