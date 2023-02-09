#include <utility>

#include "Configure.hpp"
#include "iostream"

using std::filesystem::directory_iterator, std::filesystem::exists;

class cxxflag : string
{
};

// In Boost, except Math no library is using pch
// And except math, regex and json no library is using obj either.

int main()
{
    Variant variant("Checking");

    variant.privateIncludes.emplace_back(Node::getNodeFromString(".", false));
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

    CxxFlags W_NO_LONG_LONG{"-Wno-long-long"};
    CxxFlags W_NO_VARIADIC_MACROS{"-Wno-variadic-macros"};
    CxxFlags PEDANTIC{"-pedantic"};
    CxxFlags F_DIAGNOSTICS_SHOW_OPTION{"-fdiagnostics-show-option"};
    CxxFlags W_EXTRA{"-Wextra"};
    CxxFlags W_PEDANTIC{"-pedantic"};
    CxxFlags W_UNUSED_FUNCTIONS{"-Wunused-function"};
    CxxFlags W_NO_UNUSED_PARAMETER{"-Wno-unused-parameter"};
    CxxFlags F_PERMISSIVE{"-fpermissive"};
    CxxFlags W_NO_DELETE_NO_VIRTUAL_DTOR{"-Wno-delete-non-virtual-dtor"};
    LinkFlags ENABLE_AUTO_IMPORT{"--enable-auto-import"};

    // Variables used in Boost Thread
    Define BOOST_THREAD_BUILD_LIB{"BOOST_THREAD_BUILD_LIB", "1"};
    Define BOOST_THREAD_BUILD_DLL{"BOOST_THREAD_BUILD_DLL", "1"};
    Define WIN32_LEAN_AND_MEAN_{"WIN32_LEAN_AND_MEAN"};
    Define BOOST_USE_WINDOWS_H_{"BOOST_USE_WINDOWS_H"};

    string chrono = "chrono";
    string atomic = "atomic";
    string thread = "boost_thread";
    Library &libChrono = variant.findLibrary(chrono);
    Library &libAtomic = variant.findLibrary(atomic);
    Library &libThread = variant.findLibrary(thread);

    // TODO: chrono
    // TODO: default-build properties of are not handled yet.
    // TODO: sources not supplied

    // TODO
    // AND and OR funcitons removed. Will define GAND, GOR in global namespace with mutex locked mechanism for setting
    // the global targets that work for both CppSourceTarget and LinkAndArchiveTarget
    libChrono.setTargetForAndOr()
        .M_LEFT_OR(TargetOS::FREEBSD, TS::PGI, LinkFlags{"-lrt"})
        .SINGLE(TargetOS::LINUX_, LinkFlags{"-lrt -lpthread"})
        .SINGLE(TS::SUN, Define{"__typeof__", "__typeof__"})
        .ASSIGN(Warnings::ALL)
        .M_LEFT_OR(TS::GCC, TS::DARWIN, TS::PATHSCALE, TS::CLANG, W_NO_LONG_LONG)
        .M_LEFT_OR(TS::GCC_4, TS::GCC_5, TS::DARWIN_4, TS::DARWIN_5, TS::CLANG, W_NO_VARIADIC_MACROS)
        .M_LEFT_OR(TS::DARWIN, TS::CLANG, Warnings::PEDANTIC)
        .SINGLE(TS::PATHSCALE, PEDANTIC)
        .ASSIGN(AND(TS::GCC_4_4_0, TargetOS::WINDOWS) || AND(TS::GCC_4_5_0, TargetOS::WINDOWS) ||
                    AND(TS::GCC_4_6_0, TargetOS::WINDOWS) || AND(TS::GCC_4_6_3, TargetOS::WINDOWS) ||
                    AND(TS::GCC_4_7_0, TargetOS::WINDOWS) || AND(TS::GCC_4_8_0, TargetOS::WINDOWS),
                F_DIAGNOSTICS_SHOW_OPTION)
        .SINGLE(TS::MSVC, CxxFlags{"/wd4512"})
        .SINGLE(TS::INTEL, CxxFlags{"-wd193,304,383,444,593,981,1418,2415"});

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
    libChrono.SINGLE_I(Threading::SINGLE, Define{"BOOST_CHRONO_THREAD_DISABLED"})
        .SINGLE_I(TS::VACPP, Define{"BOOST_TYPEOF_EMULATION"})
        .SINGLE_I(TS::SUN, Define{"__typeof__", "__typeof__"})
        .SINGLE_I(Link::SHARED, Define{"BOOST_CHRONO_DYN_LINK", "1"})
        .SINGLE_I(Link::STATIC, Define("BOOST_CHRONO_STATIC_LINK", "1"))
        .M_LEFT_OR_I(TS::GCC_3_4_4, TS::GCC_4_3_4, ENABLE_AUTO_IMPORT)
        .ASSIGN_I(AND(TS::GCC_4_4_0, TargetOS::WINDOWS) || AND(TS::GCC_4_5_0, TargetOS::WINDOWS), ENABLE_AUTO_IMPORT);

    // BOOST atomic starts here
    // libAtomic.ASSIGN()

    // BOOST thread starts here
    Executable &hasAtiomicFlagLockFree = variant.addExecutable("has_atomic_flag_lockfree");

    // TODO: thread
    // TODO: executable hasAtomicFlagLockFree sources not supplied.
    // TODO: Libraries naming not handled yet. These use <tag> property
    libThread.SINGLE(Link::SHARED, W_NO_LONG_LONG);
    libThread.setTargetForAndOr()
        .ASSIGN(Threading::MULTI)
        .SINGLE(Link::STATIC, BOOST_THREAD_BUILD_LIB)
        .SINGLE(Link::SHARED, BOOST_THREAD_BUILD_DLL)
        .ASSIGN(Warnings::ALL)
        .ASSIGN(OR(TS::GCC, TS::DARWIN, TS::CLANG), W_NO_LONG_LONG, W_EXTRA, W_UNUSED_FUNCTIONS, W_NO_UNUSED_PARAMETER)
        .M_LEFT_OR(TS::GCC, TS::DARWIN, PEDANTIC)
        .SINGLE(TS::DARWIN, F_PERMISSIVE)
        .SINGLE(TS::CLANG, Warnings::ON)
        .M_LEFT_OR(TS::GCC_4, TS::GCC_5, TS::DARWIN_4, TS::DARWIN_5, TS::CLANG, W_NO_VARIADIC_MACROS)
        .M_LEFT_OR(TS::DARWIN_4_6_2, TS::DARWIN_4_7_0, TS::CLANG_3_0, W_NO_DELETE_NO_VIRTUAL_DTOR)
        .SINGLE(TS::INTEL, CxxFlags{"-wd193,304,383,444,593,981,1418,2415"})
        .SINGLE(TS::MSVC, CxxFlags{"/wd4100 /wd4512 /wd6246"})
        .M_RIGHT(TargetOS::WINDOWS, WIN32_LEAN_AND_MEAN_, BOOST_USE_WINDOWS_H_);

    // Note: Some of the remarks from the Intel compiler are disabled
    // remark #193: zero used for undefined preprocessing identifier "XXX"
    // remark #304: access control not specified ("public" by default)
    // remark #593: variable "XXX" was set but never used
    // remark #1418: external function definition with no prior declaration
    // remark #2415: variable "XXX" of static storage duration was declared but never referenced

    libThread.SINGLE_I(Link::STATIC, BOOST_THREAD_BUILD_LIB).SINGLE_I(Link::SHARED, BOOST_THREAD_BUILD_DLL);

    path PTW32_INCLUDE;
    path PTW32_LIB;
    if (!(PTW32_INCLUDE.empty() && !PTW32_LIB.empty()))
    {
        string libName;
        if (libThread.toolSet.ts == TS::MSVC)
        {
            libName = "pthreadVC2.lib";
        }
        if (libThread.toolSet.ts == TS::GCC)
        {
            libName = "libpthreadGC2.a";
        }
    }
}
