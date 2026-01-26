#include "Configure.hpp"

#include <map>
#include <set>

using std::map;
using std::set;

void checkDirectory(const path &p, set<string> files, const string &endsWith)
{
    uint32_t total = files.size();
    vector<string> notFound;
    for (const auto &f : std::filesystem::recursive_directory_iterator(p))
    {
        if (f.is_regular_file() && f.path().extension() == endsWith)
        {
            if (string str = f.path().string(); !files.contains(str))
            {
                notFound.emplace_back(str);
            }
            else
            {
                files.erase(str);
            }
        }
    }

    printMessage(FORMAT("cmake total size {} for path {}\n", total, p.string()));
    printMessage(FORMAT("files in directory not in cmake {}\n", notFound.size()));
    for (const string &s : notFound)
    {
        printMessage(FORMAT("{}\n", s));
    }
    printMessage(FORMAT("files in cmake not in directory {}\n", files.size()));
    for (const string &s : files)
    {
        printMessage(FORMAT("{}\n", s));
    }
}

void writeFlagsCount(const vector<string> &lines)
{
    map<string, int> flagsCount;

    for (string l : lines)
    {
        if (l.contains("/usr/bin/c++ ") /*&& l.contains("-Wl")*/)
        {
            for (vector<string_view> flags = split(l, ' '); string_view f : flags)
            {
                const auto &[pos, ok] = flagsCount.emplace(f, 0);
                ++pos->second;
            }
        }
    }

    map<int, vector<string>> counts;
    for (auto &[flag, count] : flagsCount)
    {
        auto [pos, ok] = counts.emplace(count, vector<string>{});
        pos->second.emplace_back(flag);
    }

    for (auto &[count, flags] : counts)
    {
        for (string f : flags)
        {
            printMessage(FORMAT("{} {}\n", count, f));
        }
    }

    uint32_t count;

    /*vector<string> specificLine = split(string(lines[571]), ' ');
    count = 0;
    for (string s : specificLine)
    {
        if (s.ends_with('.o'))
        {
            ++count;
            printMessage(FORMAT("{}\n", s));
        }
    }
    printMessage(FORMAT("{}\n", count));*/
    /*count = 0;
       for (string l : lines)
       {
           if (l.contains("-DLLVM_BUILD_STATIC"))
           {
               if (l.contains("-c /home/hassan/Projects/llvm-project/clang/utils/TableGen/") ||
                   l.contains("-c /home/hassan/Projects/llvm-project/llvm/utils/TableGen/"))
               {
               }
               else
               {
                   printMessage(FORMAT("{}\n", l));
                   ++count;
               }
           }
       }
       printMessage(FORMAT("{}\n", count));*/

    /*count = 0;
    set<string> interestingFiles;
    for (string l : lines)
    {
        if (l.contains("-fvisibility=hidden"))
        {
            string f(l.begin() + l.find("-c ") + 3, l.end());
            interestingFiles.emplace(f);
            printMessage(FORMAT("{}\n", string(f)));
            ++count;
        }
    }
    printMessage(FORMAT("{}\n", count));

    checkDirectory(interestingFiles, "/home/hassan/Projects/llvm-project/llvm/lib/Target/X86");*/

    /*count = 0;
    for (string l : lines)
    {
        if (l.contains("/usr/bin/c++") && l.contains("-I/home/hassan/Projects/llvm-project/clang/include") &&
            !l.contains("-DCLANG_EXPORTS"))
        {
            string f(l.begin() + l.find("-c ") + 3, l.end());
            printMessage(FORMAT("{}\n", string(f)));
            ++count;
        }
    }
    printMessage(FORMAT("{}\n", count));*/

    // checkDirectory(interestingFiles, "/home/hassan/Projects/llvm-project/llvm/lib/Target/X86");

    count = 0;
    for (string f : lines)
    {
        if (f.contains("/usr/bin/cc") && !f.contains("-Wall"))
        {
            printMessage(FORMAT("{}\n", string(f)));
            ++count;
        }
    }
    printMessage(FORMAT("{}\n", count));
}

void matchDirectoryWithOutput(const vector<string> &lines, const string &directory, const string &endsWith)
{
    const string dirAbsolute =
        (std::filesystem::current_path() / "../../llvm-project" / path(directory)).lexically_normal().string();
    vector<string> interestingFiles;
    for (string l : lines)
    {
        if (l.ends_with(endsWith))
        {
            interestingFiles.emplace_back(l);
        }
    }

    set<string> pruned;

    for (string l : interestingFiles)
    {
        string s = " -c " + dirAbsolute;
        if (l.contains(s))
        {
            string f(l.begin() + l.find("-c ") + 3, l.end());
            pruned.emplace(f);
        }
    }

    checkDirectory(dirAbsolute, pruned, endsWith);
}

void compareObjectFiles(string targetName, const set<string> &ninjaObjectFiles, set<string> hbuildObjectFiles)
{
    for (const string &l : ninjaObjectFiles)
    {
        if (!hbuildObjectFiles.contains(l))
        {
            printErrorMessage(
                FORMAT("Ninja Object File {} not found in hbuild object files for target {}\n", l, targetName));
        }
        hbuildObjectFiles.erase(l);
    }

    for (string s : hbuildObjectFiles)
    {
        printErrorMessage(FORMAT("hbuild object-file {} not found in target {}\n", s, targetName));
    }
}

void analyzeObjectFiles(string targetName, string ninjaLine, string hbuildLine)
{
    set<string> ninjaObjectFileLines;
    for (vector<string_view> brokenNinjaCommand = split(ninjaLine, ' '); string_view s : brokenNinjaCommand)
    {
        if (s.ends_with(".o"))
        {
            ninjaObjectFileLines.emplace(s);
        }
    }

    set<string> ninjaObjectFiles;
    for (string s : ninjaObjectFileLines)
    {
        if (string stemName = path(s).stem().stem(); !ninjaObjectFiles.emplace(stemName).second)
        {
            printErrorMessage("Emplace failure means that there are 2 object-files of the same name. how to deal with "
                              "this situation?\n");
        }
    }

    vector<string> hbuildObjectFileLines;
    for (vector<string_view> brokenHbuildCommand = split(hbuildLine, ' '); string_view s : brokenHbuildCommand)
    {
        if (s.ends_with(".o\""))
        {
            hbuildObjectFileLines.emplace_back(s);
        }
    }

    set<string> hbuildObjectFiles;
    for (string s : hbuildObjectFileLines)
    {
        if (string stemName = path(s).stem().stem(); !hbuildObjectFiles.emplace(stemName).second)
        {
            printErrorMessage("Emplace failure means that there are 2 object-files of the same name. how to deal with "
                              "this situation?\n");
        }
    }

    compareObjectFiles(targetName, ninjaObjectFiles, hbuildObjectFiles);
}

void analyzeNinjaAndHbuildArchiveLines(const vector<string> &ninjaArchiveLines,
                                       const vector<string> &hbuildArchiveLines)
{
    vector<std::pair<string, string>> staticLibs;
    string ninjaArchivePre = "cmake -E rm -f lib/";
    for (string l : ninjaArchiveLines)
    {
        string archiveStringPre = "cmake -E rm -f lib/";
        const uint32_t pos = l.find(archiveStringPre) + archiveStringPre.size();
        string str{l.begin() + pos, l.begin() + l.find("&&", pos) - 1};
        // printMessage(FORMAT("{}", str));
        if (str == "libclangCodeGen.a")
        {
            continue;
        }
        staticLibs.emplace_back(str, l);
    }

    for (auto [libName, ninjaLine] : staticLibs)
    {
        string found;
        for (string hbuildLine : hbuildArchiveLines)
        {
            if (hbuildLine.contains(libName))
            {
                if (!found.empty())
                {
                    printErrorMessage(FORMAT("Library {} found twice in hbuildArchiveLines\n", libName));
                }
                found = hbuildLine;
            }
        }

        if (found.empty())
        {
            printErrorMessage(FORMAT("Library {} not found in hbuildArchiveLines\n", libName));
        }
        else
        {
            printMessage(FORMAT("found {}\n", libName));
            analyzeObjectFiles(libName, ninjaLine, found);
        }
    }
}

void analyzeStaticLibs(string targetName, string ninjaLine, string hbuildLine)
{
    set<string> ninjaStaticLibs;
    for (vector<string_view> brokenNinjaCommand = split(ninjaLine, ' '); string_view s : brokenNinjaCommand)
    {
        if (s.ends_with(".a"))
        {
            if (string libName = path(s).filename(); !ninjaStaticLibs.emplace(libName).second)
            {
                /*printErrorMessage(
                    "Emplace failure means that there are 2 object-files of the same name. how to deal with "
                    "this situation?\n");*/
            }
        }
    }

    set<string> hbuildStaticLibs;
    for (vector<string_view> brokenHbuildCommand = split(hbuildLine, ' '); string_view s : brokenHbuildCommand)
    {
        if (s.starts_with("-l\""))
        {
            string libName(s.begin() + 3, s.size() - 4);
            hbuildStaticLibs.emplace("lib" + libName + ".a");
        }
    }

    compareObjectFiles(targetName, ninjaStaticLibs, hbuildStaticLibs);
}

void analyzeNinjaAndHbuildExecutableLines(const vector<string> &ninjaExeLines, const vector<string> &hbuildExeLines)
{
    vector<std::pair<string, string>> executables;
    string ninjaArchivePre = "cmake -E rm -f lib/";
    for (string l : ninjaExeLines)
    {
        string exeStringPre = " bin/";
        const uint32_t pos = l.find(exeStringPre) + exeStringPre.size();
        string str{l.begin() + pos, l.begin() + l.find(' ', pos)};
        // printMessage(FORMAT("{}", str));
        executables.emplace_back(str, l);
    }

    for (auto [libName, ninjaLine] : executables)
    {
        string found;
        for (string hbuildLine : hbuildExeLines)
        {
            if (hbuildLine.contains(libName))
            {
                if (!found.empty())
                {
                    printErrorMessage(FORMAT("Library {} found twice in hbuildArchiveLines\n", libName));
                }
                found = hbuildLine;
            }
        }

        if (found.empty())
        {
            printErrorMessage(FORMAT("Library {} not found in hbuildArchiveLines\n", libName));
        }
        else
        {
            printMessage(FORMAT("found {}\n", libName));
            analyzeObjectFiles(libName, ninjaLine, found);
            analyzeStaticLibs(libName, ninjaLine, found);
        }
    }
}

int main()
{
    const string ninjaOutput = fileToString("output.txt");
    const vector<string_view> lines2 = split(ninjaOutput, '\n');
    vector<string> lines;
    for (string_view l : lines2)
    {
        lines.emplace_back(l);
    }
    writeFlagsCount(lines);
    matchDirectoryWithOutput(lines, "clang/lib/CodeGen/", ".cpp");

    vector<string> ninjaArchiveLines;
    for (string l : lines)
    {
        if (l.contains("cmake -E rm -f"))
        {
            ninjaArchiveLines.emplace_back(l);
        }
    }

    const string hbuildOutput = fileToString("output2.txt");
    const vector<string_view> hbuildLines2 = split(hbuildOutput, '\n');
    vector<string> hbuildLines;
    for (string_view l : hbuildLines2)
    {
        hbuildLines.emplace_back(l);
    }
    vector<string> hbuildArchiveLines;
    for (string l : hbuildLines)
    {
        if (l.contains("/usr/bin/ar"))
        {
            hbuildArchiveLines.emplace_back(l);
        }
    }

    analyzeNinjaAndHbuildArchiveLines(ninjaArchiveLines, hbuildArchiveLines);
    vector<string> ninjaExecutableLines;
    for (string l : lines)
    {
        if (l.contains(" -ldl "))
        {
            ninjaExecutableLines.emplace_back(l);
        }
    }

    vector<string> hbuildExecutableLines;
    for (string l : hbuildLines)
    {
        if (l.contains(" -ldl "))
        {
            hbuildExecutableLines.emplace_back(l);
        }
    }
    analyzeNinjaAndHbuildExecutableLines(ninjaExecutableLines, hbuildExecutableLines);
}

// command linking llvm-tblgen-min has all source-files from utils/llvm/TableGen/Basic/* +
// utils/llvm/TableGen/llvm-tblgen-min.cpp command linking llvm-tblgen has all source-files from utils/llvm/TableGen
// except utils/llvm/TableGen/llvm-tblgen-min.cpp

// -DLLVM_BUILD_STATIC is used for source-files of llvm/utils/TableGen/*, clang/utils/TableGen/* and
// clang/tools/driver/* source-files.

// Except this file /home/hassan/Projects/llvm-project/llvm/lib/Target/X86/MCA/X86CustomBehaviour.cpp
// all else files from /llvm/lib/Target/X86 are being compiled and are being compiled with -fvisibility=hidden.

// All of the clang is being built with the -DCLANG_EXPORTS except my IPC2978 and Driver.
