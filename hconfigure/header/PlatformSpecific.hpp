
#ifndef HMAKE_PLATFORMSPECIFIC_HPP
#define HMAKE_PLATFORMSPECIFIC_HPP

#include "fmt/format.h"
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

using fmt::format, std::string, std::filesystem::path, std::wstring, std::unique_ptr, std::make_unique, std::vector,
    std::span, std::string_view;

// There is nothing platform-specific in this file. It is just another BuildSystemFunctions.hpp file. Some functions go
// there, some go here.

#define FORMAT(formatStr, ...) fmt::format(formatStr, __VA_ARGS__)

vector<char> readBufferFromFile(const string &fileName);
vector<char> readBufferFromCompressedFile(const string &fileName);

string getThreadId();
inline vector<string> threadIds;

void readConfigCache();
void readBuildCache();

void writeConfigBuffer(vector<char> &buffer);
void writeBuildBuffer(vector<char> &buffer);

// While decompressing lz4 file, we allocate following + 1 the buffer size.
// So, we have compressed filee * bufferMultiplier times the space.
// Also, while storing we check that the original file size / compresseed file size
// is not equal to or greater than bufferMultiplier. Hence validating our assumption.
void writeBufferToCompressedFile(const string &fileName, const vector<char> &fileBuffer);
bool compareStringsFromEnd(string_view lhs, string_view rhs);
void lowerCaseOnWindows(char *ptr, uint64_t size);
string getNormalizedPath(path filePath);
bool childInParentPathNormalized(string_view parent, string_view child);

// Enum with sequential indices for array lookup
enum class ColorIndex : uint16_t
{
    reset = 0,
    alice_blue = 1,
    antique_white = 2,
    aqua = 3,
    aquamarine = 4,
    azure = 5,
    beige = 6,
    bisque = 7,
    black = 8,
    blanched_almond = 9,
    blue = 10,
    blue_violet = 11,
    brown = 12,
    burly_wood = 13,
    cadet_blue = 14,
    chartreuse = 15,
    chocolate = 16,
    coral = 17,
    cornflower_blue = 18,
    cornsilk = 19,
    crimson = 20,
    cyan = 21,
    dark_blue = 22,
    dark_cyan = 23,
    dark_golden_rod = 24,
    dark_gray = 25,
    dark_green = 26,
    dark_khaki = 27,
    dark_magenta = 28,
    dark_olive_green = 29,
    dark_orange = 30,
    dark_orchid = 31,
    dark_red = 32,
    dark_salmon = 33,
    dark_sea_green = 34,
    dark_slate_blue = 35,
    dark_slate_gray = 36,
    dark_turquoise = 37,
    dark_violet = 38,
    deep_pink = 39,
    deep_sky_blue = 40,
    dim_gray = 41,
    dodger_blue = 42,
    fire_brick = 43,
    floral_white = 44,
    forest_green = 45,
    fuchsia = 46,
    gainsboro = 47,
    ghost_white = 48,
    gold = 49,
    golden_rod = 50,
    gray = 51,
    green = 52,
    green_yellow = 53,
    honey_dew = 54,
    hot_pink = 55,
    indian_red = 56,
    indigo = 57,
    ivory = 58,
    khaki = 59,
    lavender = 60,
    lavender_blush = 61,
    lawn_green = 62,
    lemon_chiffon = 63,
    light_blue = 64,
    light_coral = 65,
    light_cyan = 66,
    light_golden_rod_yellow = 67,
    light_gray = 68,
    light_green = 69,
    light_pink = 70,
    light_salmon = 71,
    light_sea_green = 72,
    light_sky_blue = 73,
    light_slate_gray = 74,
    light_steel_blue = 75,
    light_yellow = 76,
    lime = 77,
    lime_green = 78,
    linen = 79,
    magenta = 80,
    maroon = 81,
    medium_aquamarine = 82,
    medium_blue = 83,
    medium_orchid = 84,
    medium_purple = 85,
    medium_sea_green = 86,
    medium_slate_blue = 87,
    medium_spring_green = 88,
    medium_turquoise = 89,
    medium_violet_red = 90,
    midnight_blue = 91,
    mint_cream = 92,
    misty_rose = 93,
    moccasin = 94,
    navajo_white = 95,
    navy = 96,
    old_lace = 97,
    olive = 98,
    olive_drab = 99,
    orange = 100,
    orange_red = 101,
    orchid = 102,
    pale_golden_rod = 103,
    pale_green = 104,
    pale_turquoise = 105,
    pale_violet_red = 106,
    papaya_whip = 107,
    peach_puff = 108,
    peru = 109,
    pink = 110,
    plum = 111,
    powder_blue = 112,
    purple = 113,
    rebecca_purple = 114,
    red = 115,
    rosy_brown = 116,
    royal_blue = 117,
    saddle_brown = 118,
    salmon = 119,
    sandy_brown = 120,
    sea_green = 121,
    sea_shell = 122,
    sienna = 123,
    silver = 124,
    sky_blue = 125,
    slate_blue = 126,
    slate_gray = 127,
    snow = 128,
    spring_green = 129,
    steel_blue = 130,
    tan = 131,
    teal = 132,
    thistle = 133,
    tomato = 134,
    turquoise = 135,
    violet = 136,
    wheat = 137,
    white = 138,
    white_smoke = 139,
    yellow = 140,
    yellow_green = 141
};

// Array of pre-computed ANSI color codes - O(1) lookup
inline const char *ColorCodes[] = {
    "\033[0m",                // reset
    "\033[38;2;240;248;255m", // alice_blue
    "\033[38;2;250;235;215m", // antique_white
    "\033[38;2;0;255;255m",   // aqua
    "\033[38;2;127;255;212m", // aquamarine
    "\033[38;2;240;255;255m", // azure
    "\033[38;2;245;245;220m", // beige
    "\033[38;2;255;228;196m", // bisque
    "\033[38;2;0;0;0m",       // black
    "\033[38;2;255;235;205m", // blanched_almond
    "\033[38;2;0;0;255m",     // blue
    "\033[38;2;138;43;226m",  // blue_violet
    "\033[38;2;165;42;42m",   // brown
    "\033[38;2;222;184;135m", // burly_wood
    "\033[38;2;95;158;160m",  // cadet_blue
    "\033[38;2;127;255;0m",   // chartreuse
    "\033[38;2;210;105;30m",  // chocolate
    "\033[38;2;255;127;80m",  // coral
    "\033[38;2;100;149;237m", // cornflower_blue
    "\033[38;2;255;248;220m", // cornsilk
    "\033[38;2;220;20;60m",   // crimson
    "\033[38;2;0;255;255m",   // cyan
    "\033[38;2;0;0;139m",     // dark_blue
    "\033[38;2;0;139;139m",   // dark_cyan
    "\033[38;2;184;134;11m",  // dark_golden_rod
    "\033[38;2;169;169;169m", // dark_gray
    "\033[38;2;0;100;0m",     // dark_green
    "\033[38;2;189;183;107m", // dark_khaki
    "\033[38;2;139;0;139m",   // dark_magenta
    "\033[38;2;85;107;47m",   // dark_olive_green
    "\033[38;2;255;140;0m",   // dark_orange
    "\033[38;2;153;50;204m",  // dark_orchid
    "\033[38;2;139;0;0m",     // dark_red
    "\033[38;2;233;150;122m", // dark_salmon
    "\033[38;2;143;188;143m", // dark_sea_green
    "\033[38;2;72;61;139m",   // dark_slate_blue
    "\033[38;2;47;79;79m",    // dark_slate_gray
    "\033[38;2;0;206;209m",   // dark_turquoise
    "\033[38;2;148;0;211m",   // dark_violet
    "\033[38;2;255;20;147m",  // deep_pink
    "\033[38;2;0;191;255m",   // deep_sky_blue
    "\033[38;2;105;105;105m", // dim_gray
    "\033[38;2;30;144;255m",  // dodger_blue
    "\033[38;2;178;34;34m",   // fire_brick
    "\033[38;2;255;250;240m", // floral_white
    "\033[38;2;34;139;34m",   // forest_green
    "\033[38;2;255;0;255m",   // fuchsia
    "\033[38;2;220;220;220m", // gainsboro
    "\033[38;2;248;248;255m", // ghost_white
    "\033[38;2;255;215;0m",   // gold
    "\033[38;2;218;165;32m",  // golden_rod
    "\033[38;2;128;128;128m", // gray
    "\033[38;2;0;128;0m",     // green
    "\033[38;2;173;255;47m",  // green_yellow
    "\033[38;2;240;255;240m", // honey_dew
    "\033[38;2;255;105;180m", // hot_pink
    "\033[38;2;205;92;92m",   // indian_red
    "\033[38;2;75;0;130m",    // indigo
    "\033[38;2;255;255;240m", // ivory
    "\033[38;2;240;230;140m", // khaki
    "\033[38;2;230;230;250m", // lavender
    "\033[38;2;255;240;245m", // lavender_blush
    "\033[38;2;124;252;0m",   // lawn_green
    "\033[38;2;255;250;205m", // lemon_chiffon
    "\033[38;2;173;216;230m", // light_blue
    "\033[38;2;240;128;128m", // light_coral
    "\033[38;2;224;255;255m", // light_cyan
    "\033[38;2;250;250;210m", // light_golden_rod_yellow
    "\033[38;2;211;211;211m", // light_gray
    "\033[38;2;144;238;144m", // light_green
    "\033[38;2;255;182;193m", // light_pink
    "\033[38;2;255;160;122m", // light_salmon
    "\033[38;2;32;178;170m",  // light_sea_green
    "\033[38;2;135;206;250m", // light_sky_blue
    "\033[38;2;119;136;153m", // light_slate_gray
    "\033[38;2;176;196;222m", // light_steel_blue
    "\033[38;2;255;255;224m", // light_yellow
    "\033[38;2;0;255;0m",     // lime
    "\033[38;2;50;205;50m",   // lime_green
    "\033[38;2;250;240;230m", // linen
    "\033[38;2;255;0;255m",   // magenta
    "\033[38;2;128;0;0m",     // maroon
    "\033[38;2;102;205;170m", // medium_aquamarine
    "\033[38;2;0;0;205m",     // medium_blue
    "\033[38;2;186;85;211m",  // medium_orchid
    "\033[38;2;147;112;219m", // medium_purple
    "\033[38;2;60;179;113m",  // medium_sea_green
    "\033[38;2;123;104;238m", // medium_slate_blue
    "\033[38;2;0;250;154m",   // medium_spring_green
    "\033[38;2;72;209;204m",  // medium_turquoise
    "\033[38;2;199;21;133m",  // medium_violet_red
    "\033[38;2;25;25;112m",   // midnight_blue
    "\033[38;2;245;255;250m", // mint_cream
    "\033[38;2;255;228;225m", // misty_rose
    "\033[38;2;255;228;181m", // moccasin
    "\033[38;2;255;222;173m", // navajo_white
    "\033[38;2;0;0;128m",     // navy
    "\033[38;2;253;245;230m", // old_lace
    "\033[38;2;128;128;0m",   // olive
    "\033[38;2;107;142;35m",  // olive_drab
    "\033[38;2;255;165;0m",   // orange
    "\033[38;2;255;69;0m",    // orange_red
    "\033[38;2;218;112;214m", // orchid
    "\033[38;2;238;232;170m", // pale_golden_rod
    "\033[38;2;152;251;152m", // pale_green
    "\033[38;2;175;238;238m", // pale_turquoise
    "\033[38;2;219;112;147m", // pale_violet_red
    "\033[38;2;255;239;213m", // papaya_whip
    "\033[38;2;255;218;185m", // peach_puff
    "\033[38;2;205;133;63m",  // peru
    "\033[38;2;255;192;203m", // pink
    "\033[38;2;221;160;221m", // plum
    "\033[38;2;176;224;230m", // powder_blue
    "\033[38;2;128;0;128m",   // purple
    "\033[38;2;102;51;153m",  // rebecca_purple
    "\033[38;2;255;0;0m",     // red
    "\033[38;2;188;143;143m", // rosy_brown
    "\033[38;2;65;105;225m",  // royal_blue
    "\033[38;2;139;69;19m",   // saddle_brown
    "\033[38;2;250;128;114m", // salmon
    "\033[38;2;244;164;96m",  // sandy_brown
    "\033[38;2;46;139;87m",   // sea_green
    "\033[38;2;255;245;238m", // sea_shell
    "\033[38;2;160;82;45m",   // sienna
    "\033[38;2;192;192;192m", // silver
    "\033[38;2;135;206;235m", // sky_blue
    "\033[38;2;106;90;205m",  // slate_blue
    "\033[38;2;112;128;144m", // slate_gray
    "\033[38;2;255;250;250m", // snow
    "\033[38;2;0;255;127m",   // spring_green
    "\033[38;2;70;130;180m",  // steel_blue
    "\033[38;2;210;180;140m", // tan
    "\033[38;2;0;128;128m",   // teal
    "\033[38;2;216;191;216m", // thistle
    "\033[38;2;255;99;71m",   // tomato
    "\033[38;2;64;224;208m",  // turquoise
    "\033[38;2;238;130;238m", // violet
    "\033[38;2;245;222;179m", // wheat
    "\033[38;2;255;255;255m", // white
    "\033[38;2;245;245;245m", // white_smoke
    "\033[38;2;255;255;0m",   // yellow
    "\033[38;2;154;205;50m"   // yellow_green
};

inline const char *getColorCode(ColorIndex c)
{
    return ColorCodes[static_cast<uint32_t>(c)];
}

#endif // HMAKE_PLATFORMSPECIFIC_HPP
