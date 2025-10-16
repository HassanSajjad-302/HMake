#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    string s = "ball_pit/modules/";
    config.getCppExeDSC("app")
        .getSourceTarget()
        .publicHeaderFiles("ball_pit/3rd_party/olcPixelGameEngine/olcPixelGameEngine.h", "olcPixelGameEngine.h")
        .interfaceFiles(s + "bridges/pge-bridge.ixx", "Bridges.PGE", s + "physics/physics.ixx", "Physics",
                        s + "physics/physics-ball.ixx", "Physics.Ball", s + "physics/physics-engine.ixx",
                        "Physics.Engine", s + "physics/physics-utils.ixx", "Physics.Utils", s + "physics/quad-tree.ixx",
                        "Physics.QuadTree", s + "util/basic-types.ixx", "Util.BasicTypes", s + "util/enum-utils.ixx",
                        "Util.EnumUtils", s + "util/random-generator.ixx", "Util.RandomGenerator",
                        s + "util/stopwatch.ixx", "Util.Stopwatch", s + "util/util.ixx", "Util", s + "world/world.ixx",
                        "World")
        .rModuleDirs("ball_pit/src");
}

void buildSpecification()
{
    getConfiguration().assign(TreatModuleAsSource::NO);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION