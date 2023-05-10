
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{

    DSC<CppSourceTarget> &system = GetCppSharedDSC("system");
    CppSourceTarget &systemCpp = system.getSourceTarget();
    systemCpp.MODULE_DIRECTORIES("src/SFML/System/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/System/");
    systemCpp.MODULE_DIRECTORIES("src/SFML/System/Win32/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/System/Win32/");
    // winmm

    DSC<CppSourceTarget> &network = GetCppSharedDSC("network");
    CppSourceTarget &networkCpp = network.getSourceTarget();
    networkCpp.MODULE_DIRECTORIES("src/SFML/Network/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Network/");
    networkCpp.MODULE_DIRECTORIES("src/SFML/Network/Win32/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Network/Win32/");
    // ws2_32

    DSC<CppSourceTarget> &window = GetCppSharedDSC("window").PUBLIC_LIBRARIES(&system);

    // Sources are specified considering SFML_OPENGL_ES = true; Otherwise two files shouldn't be specified EGLCheck.pp
    // and EglContext.cpp. Better if these files are moved to a new directory, so MODULE_DIRECTORIES or
    // SOURCE_DIRECTORIES could be conveniently used.
    CppSourceTarget &windowCpp = window.getSourceTarget();
    windowCpp.MODULE_DIRECTORIES("src/SFML/Window/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Window/");
    windowCpp.MODULE_DIRECTORIES("src/SFML/Window/Win32/", ".*cpp")
        .PRIVATE_HU_INCLUDES("src/SFML/Window/Win32/")
        .PRIVATE_HU_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_HU_INCLUDES("/extlibs/headers/vulkan/");
    // SFML_USE_DRM = false
    // OpenGL library is set by two variables OPENGL_INCLUDE_DIR and OPEN_gl_LIBRARY
    // Where are these variables set.
    // winmm and gdi32 are being linked

    DSC<CppSourceTarget> &graphics = GetCppSharedDSC("graphics").PUBLIC_LIBRARIES(&window);

    CppSourceTarget &graphicsCpp = graphics.getSourceTarget();
    graphicsCpp.MODULE_DIRECTORIES("src/SFML/Graphics/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Graphics/");
    graphicsCpp.PRIVATE_HU_INCLUDES("extlibs/headers/stb_image/")
        .PRIVATE_HU_INCLUDES("/extlibs/headers/freetype2")
        .PRIVATE_INCLUDES("extlibs/headers/glad/include")
        .PRIVATE_COMPILE_DEFINITION("STBI_FAILURE_USERMSG");
    // legacy_stdio_definitions.lib
    // Freetype

    DSC<CppSourceTarget> &audio = GetCppSharedDSC("audio");
    CppSourceTarget &audioCpp = audio.getSourceTarget();
    audioCpp.MODULE_DIRECTORIES("src/SFML/Audio/", ".*cpp").PRIVATE_HU_INCLUDES("src/SFML/Audio/");

    DSC<CppSourceTarget> &main_ = GetCppStaticDSC("main");
    main_.getSourceTarget().MODULE_FILES("src/SFML/Main/MainWin32.cpp");
}

void buildSpecification()
{
    DSC<CppSourceTarget> &asio = GetCppObjectDSC("asio");
    asio.getSourceTarget()
        .PUBLIC_HU_INCLUDES("asio/asio/include")
        .PUBLIC_COMPILE_DEFINITION("ASIO_STANDALONE")
        .assignStandardIncludesToHUIncludes();

    DSC<CppSourceTarget> &spdlog = GetCppObjectDSC("spdlog");
    spdlog.getSourceTarget().PUBLIC_HU_INCLUDES("spdlog/include/").setModuleScope(asio.getSourceTargetPointer());

    DSC<CppSourceTarget> &app = GetCppExeDSC("app").PRIVATE_LIBRARIES(&spdlog, &asio);
    app.getSourceTarget()
        .MODULE_DIRECTORIES("GetAway/src/", ".*")
        .MODULE_FILES("GetAway/main.cpp")
        .PUBLIC_HU_INCLUDES("GetAway/header/")
        .PRIVATE_COMPILE_DEFINITION("-D_WIN32_WINNT", "0x0A00")
        .ASSIGN(TranslateInclude::YES)
        .setModuleScope(asio.getSourceTargetPointer());

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