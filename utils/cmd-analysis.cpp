#include "Configure.hpp"

#include <map>
#include <set>

using std::map;
using std::set;

static bool hasErrors = false;

void checkDirectory(const path &p, set<string> files, const string &endsWith)
{
    if (!std::filesystem::exists(p))
    {
        printErrorMessageNoReturn(FORMAT("Directory does not exist: {}\n", p.string()));
        hasErrors = true;
        return;
    }
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
    const string dirAbsolute = std::filesystem::absolute(directory).lexically_normal().string();
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

void compareObjectFiles(string targetName, const set<string> &ninjaObjectFiles, set<string> hbuildObjectFiles,
                        const bool warningForMissingInNinja)
{
    for (const string &l : ninjaObjectFiles)
    {
        if (!hbuildObjectFiles.contains(l))
        {
            printErrorMessageNoReturn(
                FORMAT("Ninja Object File {} not found in hbuild object files for target {}\n", l, targetName));
            hasErrors = true;
        }
        hbuildObjectFiles.erase(l);
    }

    for (string s : hbuildObjectFiles)
    {
        if (warningForMissingInNinja)
        {
            // Used for Executables as due to hu dependencies an exe can have more deps in hbuild than in Ninja.
            printMessage(FORMAT("hbuild object-file {} not found in target {}\n", s, targetName));
        }
        else
        {
            printErrorMessageNoReturn(FORMAT("hbuild object-file {} not found in target {}\n", s, targetName));
            hasErrors = true;
        }
    }
}

set<string> getDuplicateObjFiles()
{
    set<string> duplicateObjectFiles;

    // following are of clangCodeGen target
    duplicateObjectFiles.emplace("AArch64");
    duplicateObjectFiles.emplace("ARC");
    duplicateObjectFiles.emplace("AMDGPU");
    duplicateObjectFiles.emplace("ARM");
    duplicateObjectFiles.emplace("AVR");
    duplicateObjectFiles.emplace("BPF");
    duplicateObjectFiles.emplace("CSKY");
    duplicateObjectFiles.emplace("DirectX");
    duplicateObjectFiles.emplace("Hexagon");
    duplicateObjectFiles.emplace("Lanai");
    duplicateObjectFiles.emplace("LoongArch");
    duplicateObjectFiles.emplace("M68k");
    duplicateObjectFiles.emplace("Mips");
    duplicateObjectFiles.emplace("MSP430");
    duplicateObjectFiles.emplace("NVPTX");
    duplicateObjectFiles.emplace("PPC");
    duplicateObjectFiles.emplace("RISCV");
    duplicateObjectFiles.emplace("Sparc");
    duplicateObjectFiles.emplace("SPIR");
    duplicateObjectFiles.emplace("SystemZ");
    duplicateObjectFiles.emplace("TCE");
    duplicateObjectFiles.emplace("VE");
    duplicateObjectFiles.emplace("WebAssembly");
    duplicateObjectFiles.emplace("X86");
    duplicateObjectFiles.emplace("XCore");

    // following of clangDriver target
    duplicateObjectFiles.emplace("AMDGPU");

    // following of llvm-tblgen target
    duplicateObjectFiles.emplace("Types");

    return duplicateObjectFiles;
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

    set<string> duplicateObjectFiles = getDuplicateObjFiles();
    set<string> ninjaObjectFiles;

    uint32_t count = 0;
    for (string s : ninjaObjectFileLines)
    {
        if (string stemName = path(s).stem().stem(); !ninjaObjectFiles.emplace(stemName).second)
        {
            if (duplicateObjectFiles.contains(stemName))
            {
                continue;
            }
            printErrorMessageNoReturn(
                FORMAT("There are 2 object-files with same name {} in ninjaObjectFileLinex in target {}\n", stemName,
                       targetName));
            hasErrors = true;
        }
    }

    vector<string> hbuildObjectFileLines;
    for (vector<string_view> brokenHbuildCommand = split(hbuildLine, ' '); string_view s : brokenHbuildCommand)
    {
        if (s.ends_with(".o\"") || s.ends_with(".o"))
        {
            string cleanStr(s);
            if (cleanStr.front() == '"')
                cleanStr.erase(cleanStr.begin());
            if (cleanStr.back() == '"')
                cleanStr.pop_back();
            hbuildObjectFileLines.emplace_back(std::move(cleanStr));
        }
    }

    set<string> hbuildObjectFiles;
    for (string s : hbuildObjectFileLines)
    {
        if (string stemName = path(s).stem().stem(); !hbuildObjectFiles.emplace(stemName).second)
        {
            if (duplicateObjectFiles.contains(stemName))
            {
                continue;
            }
            printErrorMessageNoReturn(FORMAT("There are 2 object-files with same name {} in hbuildObjectFiles in target {}\n",
                                     stemName, targetName));
            hasErrors = true;
        }
    }

    compareObjectFiles(targetName, ninjaObjectFiles, hbuildObjectFiles, false);
}

void analyzeNinjaAndHbuildArchiveLines(const vector<string> &ninjaArchiveLines,
                                       const vector<string> &hbuildArchiveLines)
{
    vector<std::pair<string, string>> staticLibs;
    string ninjaArchivePre = "cmake -E rm -f lib/";
    for (string l : ninjaArchiveLines)
    {
        string archiveStringPre = "cmake -E rm -f lib/";
        const auto pos = l.find(archiveStringPre);
        if (pos == string::npos)
        {
            continue;
        }
        const uint32_t nameStart = pos + archiveStringPre.size();
        const auto endPos = l.find("&&", nameStart);
        if (endPos == string::npos)
        {
            continue;
        }
        string str{l.begin() + nameStart, l.begin() + endPos - 1};
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
                    printErrorMessageNoReturn(FORMAT("Library {} found twice in hbuildArchiveLines\n", libName));
                    hasErrors = true;
                }
                found = hbuildLine;
            }
        }

        if (found.empty())
        {
            printErrorMessageNoReturn(FORMAT("Library {} not found in hbuildArchiveLines\n", libName));
            hasErrors = true;
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
    const auto getStaticLibs = [](const string &line) {
        set<string> libraries;
        for (string_view s : split(line, ' '))
        {
            if (s.starts_with("-l\""))
            {
                string libName(s.begin() + 3, s.size() - 4);
                libraries.emplace("lib" + libName + ".a");
            }
            else if (s.starts_with("-l"))
            {
                string libName(s.begin() + 2, s.end());
                libraries.emplace("lib" + libName + ".a");
            }
            else if (s.ends_with(".a"))
            {
                libraries.emplace(path(s).filename());
            }
        }
        return libraries;
    };

    compareObjectFiles(targetName, getStaticLibs(ninjaLine), getStaticLibs(hbuildLine), true);
}

void analyzeNinjaAndHbuildExecutableLines(const vector<string> &ninjaExeLines, const vector<string> &hbuildExeLines)
{
    vector<std::pair<string, string>> executables;
    string ninjaArchivePre = "cmake -E rm -f lib/";
    for (string l : ninjaExeLines)
    {
        string exeStringPre = " bin/";
        const auto pos = l.find(exeStringPre);
        if (pos == string::npos)
        {
            continue;
        }
        const uint32_t nameStart = pos + exeStringPre.size();
        const auto endPos = l.find(' ', nameStart);
        string str = (endPos == string::npos) ? string(l.begin() + nameStart, l.end())
                                              : string(l.begin() + nameStart, l.begin() + endPos);
        executables.emplace_back(str, l);
    }

    for (auto [libName, ninjaLine] : executables)
    {
        string found;
        const string searchName = "/" + libName + "\"";
        for (string hbuildLine : hbuildExeLines)
        {
            if (hbuildLine.contains(searchName))
            {
                if (!found.empty())
                {
                    printErrorMessageNoReturn(FORMAT("Executable {} found twice in hbuildExecutableLines\n", libName));
                    hasErrors = true;
                }
                found = hbuildLine;
            }
        }

        if (found.empty())
        {
            printErrorMessageNoReturn(FORMAT("Executable {} not found in hbuildExecutableLines\n", libName));
            hasErrors = true;
        }
        else
        {
            printMessage(FORMAT("found {}\n", libName));
            // Not analyzed because cmake links LLVMTableGen as object files while we link it as a library.
            if (libName != "llvm-min-tblgen")
            {
                analyzeObjectFiles(libName, ninjaLine, found);
                analyzeStaticLibs(libName, ninjaLine, found);
            }
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
    // writeFlagsCount(lines);
    path llvmDir = std::filesystem::current_path();
    if (!std::filesystem::exists(llvmDir / "clang/lib/CodeGen"))
    {
        llvmDir = llvmDir / "../../llvm-project";
    }
    matchDirectoryWithOutput(lines, (llvmDir / "clang/lib/CodeGen/").lexically_normal().string(), ".cpp");

    vector<string> ninjaArchiveLines;
    for (string l : lines)
    {
        if (l.contains("cmake -E rm -f lib/"))
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
        if (l.contains("-ar\"") || l.contains("/ar\"") || l.contains(" rcs ") || l.contains(".a.tmp"))
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

    if (hasErrors)
    {
        printErrorMessageNoReturn("CmdAnalysis detected discrepancies between Ninja and HBuild dry-runs.\n");
        return 1;
    }
    return 0;
}

// -DLLVM_BUILD_STATIC is used for source-files of llvm/utils/TableGen/*, clang/utils/TableGen/* and
// clang/tools/driver/* source-files.

// Except this file /home/hassan/Projects/llvm-project/llvm/lib/Target/X86/MCA/X86CustomBehaviour.cpp
// all else files from /llvm/lib/Target/X86 are being compiled and are being compiled with -fvisibility=hidden.

// All of the clang is being built with the -DCLANG_EXPORTS except my IPC2978 and Driver.
