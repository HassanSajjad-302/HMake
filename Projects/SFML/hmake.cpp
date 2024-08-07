
// This successfully compiles SFML with C++20 header units. Except header units from prebuilt libraries vulkan, glad and
// minimp3, and, from directories as src/Win32/, all header units were successfully compiled. Adding  a header-unit from
// one of these causes compilation error. Not investigating for now.

#include "Configure.hpp"

template <typename... T>
void initializeTargets(TargetType targetType, DSC<CppSourceTarget> *target, const bool win32, T... targets)
{
    CppSourceTarget &t = target->getSourceTarget();
    string str = t.name.substr(0, t.name.size() - 4); // Removing -cpp from the name
    t.PRIVATE_INCLUDES("src")
        .PUBLIC_INCLUDES("include")
        .PRIVATE_HU_DIRECTORIES("src/SFML/" + str)
        .PUBLIC_HU_DIRECTORIES("include/SFML/" + str);

    if (win32)
    {
        t.MODULE_DIRECTORIES_RG("src/SFML/" + str + "/Win32", ".*cpp")
            .PRIVATE_HU_DIRECTORIES("src/SFML/" + str + "/Win32");
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
        initializeTargets(targetType, targets...);
    }
}

void configurationSpecification(Configuration &configuration)
{
    configuration.ASSIGN(Define{"SFML_SYSTEM_WINDOWS"});
    configuration.archiving = true;

    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    const string winStaticLibsDir = srcDir + "extlibs/libs-msvc-universal/x64/";

    DSC<CppSourceTarget> &openal32 = configuration.GetCppStaticDSC_P("openal32", winStaticLibsDir);
    openal32.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/AL");

    DSC<CppSourceTarget> &flac = configuration.GetCppStaticDSC_P("flac", winStaticLibsDir);
    flac.getSourceTarget().PUBLIC_COMPILE_DEFINITION("FLAC__NO_DLL").PUBLIC_HU_INCLUDES("extlibs/headers/FLAC");

    DSC<CppSourceTarget> &freetype = configuration.GetCppStaticDSC_P("freetype", winStaticLibsDir);

    DSC<CppSourceTarget> &glad = configuration.GetCppObjectDSC("glad");
    glad.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/glad/include");

    DSC<CppSourceTarget> &minimp3 = configuration.GetCppObjectDSC("minimp3");
    minimp3.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/minimp3");

    DSC<CppSourceTarget> &ogg = configuration.GetCppStaticDSC_P("ogg", winStaticLibsDir);
    ogg.getSourceTarget()
        .PUBLIC_HU_DIRECTORIES("extlibs/headers/ogg")
        .PRIVATE_INCLUDES("extlibs/headers")
        .INTERFACE_INCLUDES("extlibs/headers");

    DSC<CppSourceTarget> &stb_image = configuration.GetCppObjectDSC("stb_image");
    stb_image.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/stb_image");

    DSC<CppSourceTarget> &vorbis = configuration.GetCppStaticDSC_P("vorbis", winStaticLibsDir).PRIVATE_LIBRARIES(&ogg);
    vorbis.getSourceTarget()
        .INTERFACE_COMPILE_DEFINITION("OV_EXCLUDE_STATIC_CALLBACKS")
        .PUBLIC_HU_INCLUDES("extlibs/headers/vorbis");

    DSC<CppSourceTarget> &vorbisenc = configuration.GetCppStaticDSC_P("vorbisenc", winStaticLibsDir);

    DSC<CppSourceTarget> &vorbisfile = configuration.GetCppStaticDSC_P("vorbisfile", winStaticLibsDir);

    DSC<CppSourceTarget> &vulkan = configuration.GetCppObjectDSC("vulkan");
    vulkan.getSourceTarget().PUBLIC_HU_INCLUDES("extlibs/headers/vulkan");

    DSC<CppSourceTarget> &system =
        configuration.GetCppTargetDSC("system", configuration.targetType).PRIVATE_LIBRARIES(&stdhu);
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES_RG("src/SFML/system", ".*cpp")
        .MODULE_DIRECTORIES_RG("src/SFML/System/Win32", ".*cpp")
        .PUBLIC_HU_DIRECTORIES("src/SFML/System");

    DSC<CppSourceTarget> &network = configuration.GetCppTargetDSC("network", configuration.targetType)
                                        .PUBLIC_LIBRARIES(&system)
                                        .PRIVATE_LIBRARIES(&stdhu, &glad);
    network.getSourceTarget().MODULE_DIRECTORIES_RG("src/SFML/network", ".*cpp");

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
        .PRIVATE_HU_DIRECTORIES("src/SFML/Window")
        .PRIVATE_HU_DIRECTORIES("src/SFML/Window/Win32")
        .PUBLIC_HU_DIRECTORIES("include/SFML/Window");

    // SFML_USE_DRM = false
    // OpenGL library is set by two variables OPENGL_INCLUDE_DIR and OPEN_gl_LIBRARY
    // Where are these variables set.
    // winmm and gdi32 are being linked

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

    /*DSC<CppSourceTarget> &audio =
        configuration.GetCppTargetDSC("audio", configuration.targetType)
            .PUBLIC_LIBRARIES(&system)
            .PRIVATE_LIBRARIES(&stdhu, &openal32, &vorbis, &vorbisenc, &vorbisfile, &flac, &minimp3);

    CppSourceTarget &audioCpp = audio.getSourceTarget();

    audioCpp.MODULE_DIRECTORIES_RG("src/SFML/audio", ".*cpp").PRIVATE_INCLUDES("extlibs/headers");*/

    initializeTargets(configuration.targetType, &graphics, false, &system, true, &network, true);

    for (auto &k : std::filesystem::directory_iterator(srcDir + "src/SFML/Graphics"))
    {
        if (k.path().extension().string() == ".cpp")
        {
            if (k.path().string().contains("Font"))
            {
                graphicsCpp.SOURCE_FILES(k.path().string());
            }
            else
            {
                graphicsCpp.MODULE_FILES(k.path().string());
            }
        }
    }

    configuration.archiving = false;

    DSC<CppSourceTarget> &example = configuration.GetCppExeDSC("Example").PRIVATE_LIBRARIES(&graphics, &stdhu);
    example.getSourceTarget().MODULE_FILES("examples/win32/Win32.cpp");
    if (configuration.name == "hu")
    {
        example.getSourceTarget().PRIVATE_COMPILE_DEFINITION("USE_HEADER_IMPORT");
    }
    if (configuration.targetType != TargetType::LIBRARY_SHARED)
    {
        window.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_STATIC");
        example.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_STATIC");
    }
    else
    {
        window.getSourceTarget().PRIVATE_COMPILE_DEFINITION("SFML_WINDOW_EXPORTS");
    }

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

    for (LinkOrArchiveTarget *loat : configuration.linkOrArchiveTargets)
    {
        if (loat->OR(TargetType::EXECUTABLE, TargetType::LIBRARY_SHARED))
        {
            loat->requirementLinkerFlags +=
                "advapi32.lib gdi32.lib opengl32.lib user32.lib winmm.lib winmm.Lib ws2_32.lib";
        }
    }
}

void buildSpecification()
{
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    GetConfiguration("conventional").ASSIGN(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::YES);

    // Drop-in Replacement of header-files with header-units. Results in 2x incremental compilation speed-up of
    // Win32.cpp file. The caveat however is that I could not make shared libraries work with C++ 20 header-units. I
    // have a doubt that it is because of
    // https://developercommunity.visualstudio.com/t/Cannot-easily-link-C20-modules-as-DLLs/10202062?sort=votes&viewtype=all&page=3
    // Build succeeded with static libraries instead
    GetConfiguration("drop-in").ASSIGN(CxxSTD::V_LATEST, TargetType::LIBRARY_STATIC, TreatModuleAsSource::NO,
                                       TranslateInclude::YES);

    // In this configuration, USE_HEADER_IMPORT definition will be set for Example target because of which Win32.cpp
    // imports only 3 header units. If TranslateInclude::YES would had been provided it would had imported 150 instead.

    // That is because if e.g. header-file main.cpp includes A.hpp which includes B.hpp. And TranslateInclude::YES
    // is used, then, B.hpp will be compiled first, then A.hpp, and main.cpp will depend on both A.hpp and B.hpp. But,
    // if TranslateInclude::YES is not used and main.cpp explicitly imports A.hpp which includes B.hpp, then, A.hpp will
    // be compiled with B.hpp contents and main.cpp will only depend on A.hpp.

    // This configuration was 3.5x faster which shows that importing aggregate header-unit is better than
    // importing multiple smaller header-units.
    GetConfiguration("hu").ASSIGN(CxxSTD::V_LATEST, TargetType::LIBRARY_STATIC, TreatModuleAsSource::NO);

    // Run this benchmark.exe, this will execute hbuild in Example-cpp in the thrice configurations 10 times and prints
    // the average time. Please note that hbuild is executed in Example-cpp dir, so the source-file is recompiled but is
    // not relinked. Otherwise, results could had been different as 2 configurations link major libraries statically,
    // while one links them dynamically.
    GetCppExeDSC("benchmark").getSourceTarget().SOURCE_FILES("benchmark.cpp");

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION