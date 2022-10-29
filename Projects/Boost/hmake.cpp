#include <utility>

#include "Configure.hpp"
#include "iostream"

using std::filesystem::directory_iterator, std::filesystem::exists;

class cxxflag : string
{
};

int main()
{
    // Very Important Variables
    // Uarget_OS targetOs;
    Cache::initializeCache();
    Project project;
    ProjectVariant variant{project};

    variant.privateIncludes.emplace_back(".");
    variant.privateCompileDefinitions.emplace_back("BOOST_ALL_NO_LIB", "1");

    for (const auto &it : directory_iterator(srcDir / path("libs")))
    {
        if (it.is_directory() &&
            (exists(it.path() / path("build/Jamfile")) || exists(it.path() / path("build/Jamfile.v2"))))
        {
            variant.addLibrary(it.path().filename().string());
            std::cout << it.path().filename();
        }
    }

    cxxflags W_NO_LONG_LONG{"-Wno-long-long"};
    cxxflags W_NO_VARIADIC_MACROS{"-Wno-variadic-macros"};
    cxxflags PEDANTIC{"-pedantic"};
    cxxflags F_DIAGNOSTICS_SHOW_OPTION{"-fdiagnostics-show-option"};

    linkflags ENABLE_AUTO_IMPORT{"--enable-auto-import"};

    string chrono = "chrono";
    string thread = "boost_thread";
    Library &libChrono = variant.findLibrary(chrono);
    Library &libThread = variant.findLibrary(thread);

    libChrono.setTargetForAndOr()
        .M_LEFT_OR(TargetOS::FREEBSD, ToolSet{TS::PGI}, linkflags{"-lrt"})
        .SINGLE(TargetOS::LINUX_, linkflags{"-lrt -lpthread"})
        .SINGLE(ToolSet{TS::SUN}, define{"__typeof__", "__typeof__"})
        .ASSIGN(Warnings::ALL)
        .M_LEFT_OR(ToolSet{TS::GCC}, ToolSet{TS::DARWIN}, ToolSet{TS::PATHSCALE}, ToolSet{TS::CLANG}, W_NO_LONG_LONG)
        .M_LEFT_OR(ToolSet{TS::GCC_4}, ToolSet{TS::GCC_5}, ToolSet{TS::DARWIN_4}, ToolSet{TS::DARWIN_5},
                   ToolSet{TS::CLANG}, W_NO_VARIADIC_MACROS)
        .M_LEFT_OR(ToolSet{TS::DARWIN}, ToolSet{TS::CLANG}, Warnings::PEDANTIC)
        .SINGLE(ToolSet{TS::PATHSCALE}, PEDANTIC)
        .ASSIGN(AND(ToolSet{TS::GCC_4_4_0}, TargetOS::WINDOWS) || AND(ToolSet{TS::GCC_4_5_0}, TargetOS::WINDOWS) ||
                    AND(ToolSet{TS::GCC_4_6_0}, TargetOS::WINDOWS) || AND(ToolSet{TS::GCC_4_6_3}, TargetOS::WINDOWS) ||
                    AND(ToolSet{TS::GCC_4_7_0}, TargetOS::WINDOWS) || AND(ToolSet{TS::GCC_4_8_0}, TargetOS::WINDOWS),
                F_DIAGNOSTICS_SHOW_OPTION)
        .SINGLE(ToolSet{TS::MSVC}, "/wd4512")
        .SINGLE(ToolSet{TS::INTEL}, cxxflags{"-wd193,304,383,444,593,981,1418,2415"});

    // Note: Some of the remarks from the Intel compiler are disabled
    // remark #193: zero used for undefined preprocessing identifier "XXX"
    // remark #304: access control not specified ("public" by default)
    // remark #383: value copied to temporary, reference to temporary used
    // remark #444: destructor for base class "XXX" (declared at line YYY") is not virtual
    // remark #593: variable "XXX" was set but never used
    // remark #981: operands are evaluated in unspecified order
    // remark #1418: external function definition with no prior declaration
    // remark #2415: variable "XXX" of static storage duration was declared but never referenced

    // usage-requirements conditions are producer-based and not consumer-based.
    libChrono.SINGLE_I(Threading::SINGLE, define{"BOOST_CHRONO_THREAD_DISABLED"})
        .SINGLE_I(ToolSet{TS::VACPP}, define{"BOOST_TYPEOF_EMULATION"})
        .SINGLE_I(ToolSet{TS::SUN}, define{"__typeof__", "__typeof__"})
        .SINGLE_I(Link::SHARED, define{"BOOST_CHRONO_DYN_LINK", "1"})
        .SINGLE_I(Link::STATIC, define("BOOST_CHRONO_STATIC_LINK", "1"))
        .M_LEFT_OR_I(ToolSet{TS::GCC_3_4_4}, ToolSet{TS::GCC_4_3_4}, ENABLE_AUTO_IMPORT)
        .ASSIGN_I(AND(ToolSet{TS::GCC_4_4_0}, TargetOS::WINDOWS) || AND(ToolSet{TS::GCC_4_5_0}, TargetOS::WINDOWS),
                  ENABLE_AUTO_IMPORT);

    // TODO: chrono
    // TODO: default-build properties of are not handled yet.
    // TODO: sources not supplied

    // BOOST thread starts here
    Executable *hasAtiomicFlagLockFree = variant.addExecutable("has_atomic_flag_lockfree");

    // TODO: thread
    // TODO: executable hasAtomicFlagLockFree sources not supplied.
}
