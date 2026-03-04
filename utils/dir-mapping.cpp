#include "BuildSystemFunctions.hpp"

#include "fstream"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using std::filesystem::current_path;
using namespace std;
namespace fs = filesystem;

// Function to populate the dirToFiles map
void populateRecursiveDirectoryMap(const string &rootPath, map<string, vector<string>> &dirToFiles)
{
    // Iterate through all entries in the directory tree
    for (const auto &entry : fs::recursive_directory_iterator(rootPath))
    {
        if (entry.is_regular_file())
        {
            if (string ext = entry.path().extension().string(); ext == ".h" || ext == ".inc" || ext == ".def")
            {
                // Get the parent directory path
                string dirPath = entry.path().parent_path().string();
                string fileName = entry.path().filename().string();

                // Get reference to the vector for this directory

                // Check if filename already exists (ensure uniqueness)
                if (auto &files = dirToFiles[dirPath]; ranges::find(files, fileName) == files.end())
                {
                    files.push_back(fileName);
                }
            }
        }
    }
}

bool replace(std::string &str, const std::string &from, const std::string &to)
{
    const size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
    {
        return false;
    }
    str.replace(start_pos, from.length(), to);
    return true;
}

void replaceFileTextsRecursive(const string &rootPath, const vector<string> &fileNames, const string &prepend,
                               const string &append)
{
    if (!filesystem::exists(rootPath))
    {
        printErrorMessage(FORMAT("path {} does not exists\n", rootPath));
    }

    vector<string> filePaths;

    for (const auto &entry : fs::recursive_directory_iterator(rootPath))
    {
        if (entry.is_regular_file())
        {
            if (string ext = entry.path().extension().string();
                ext == ".h" || ext == ".inc" || ext == ".def" || ext == ".cpp")
            {
                filePaths.emplace_back(entry.path().string());
            }
        }
    }

    vector<string> fileTexts;
    fileTexts.reserve(filePaths.size());
    for (const string &filePath : filePaths)
    {
        fileTexts.emplace_back(fileToString(filePath));
    }

    bool fileChanged = false;
    for (const string &name : fileNames)
    {
        string from = "#include \"" + name + '\"';
        string to = "#include \"" + prepend;
        to += name + append + '\"';

        for (uint32_t i = 0; i < fileTexts.size(); ++i)
        {
            if (filePaths[i].ends_with("ELF.h"))
            {
                bool breakpoint = true;
            }
            string &fileText = fileTexts[i];
            while (replace(fileText, from, to))
            {
                fileChanged = true;
                printMessage(FORMAT("Replaced {} to {} in file {}\n", from, to, filePaths[i]));
            }
        }
    }

    if (!fileChanged)
    {
        return;
    }

    fflush(stdout);
    for (uint32_t i = 0; i < filePaths.size(); ++i)
    {
        std::ofstream(filePaths[i]) << fileTexts[i];
    }
}

void replaceFileTexts(const string &rootPath, const vector<string> &fileNames, const string &prepend,
                      const string &append)
{
    if (!filesystem::exists(rootPath))
    {
        printErrorMessage(FORMAT("path {} does not exists\n", rootPath));
    }
    vector<string> filePaths;

    for (const auto &entry : fs::directory_iterator(rootPath))
    {
        if (entry.is_regular_file())
        {
            if (string ext = entry.path().extension().string();
                ext == ".h" || ext == ".inc" || ext == ".def" || ext == ".cpp")
            {
                filePaths.emplace_back(entry.path().string());
            }
        }
    }

    vector<string> fileTexts;
    fileTexts.reserve(filePaths.size());
    for (const string &filePath : filePaths)
    {
        fileTexts.emplace_back(fileToString(filePath));
    }

    bool fileChanged = false;
    for (const string &name : fileNames)
    {
        for (uint32_t i = 0; i < fileTexts.size(); ++i)
        {
            string from = "#include \"" + name + '\"';
            string to = "#include \"" + prepend;
            to += name + append + '\"';
            string &fileText = fileTexts[i];
            while (replace(fileText, from, to))
            {
                fileChanged = true;
                printMessage(FORMAT("Replaced {} to {} in file {}\n", from, to, filePaths[i]));
            }
        }
    }

    if (!fileChanged)
    {
        return;
    }

    fflush(stdout);
    for (uint32_t i = 0; i < filePaths.size(); ++i)
    {
        std::ofstream(filePaths[i]) << fileTexts[i];
    }
}

vector<string> getFileNames(const string &rootPath)
{
    if (!filesystem::exists(rootPath))
    {
        printErrorMessage(FORMAT("path {} does not exists\n", rootPath));
    }

    vector<string> fileNames;
    // Iterate through all entries in the directory tree
    for (const auto &entry : fs::directory_iterator(rootPath))
    {
        if (entry.is_regular_file())
        {
            if (string ext = entry.path().extension().string(); ext == ".h" || ext == ".inc" || ext == ".def")
            {
                fileNames.emplace_back(entry.path().filename().string());
            }
        }
    }
    return fileNames;
}

vector<string> getFileNamesRecursive(const string &rootPath)
{
    if (!filesystem::exists(rootPath))
    {
        printErrorMessage(FORMAT("path {} does not exists\n", rootPath));
    }

    vector<string> fileNames;
    // Iterate through all entries in the directory tree
    for (const auto &entry : fs::recursive_directory_iterator(rootPath))
    {
        if (entry.is_regular_file())
        {
            if (string ext = entry.path().extension().string(); ext == ".h" || ext == ".inc" || ext == ".def")
            {
                fileNames.emplace_back(entry.path().filename().string());
            }
        }
    }
    return fileNames;
}

void replaceInSingleFile(const string &relativePath, const string &from, const string &to)
{
    const string filePath = current_path() / relativePath;
    string fileText = fileToString(filePath);
    if (fileText.empty())
    {
        printErrorMessage(FORMAT("Empty file {}.\n", filePath));
    }

    // Replace all occurrences in one pass
    size_t pos = 0;
    while ((pos = fileText.find(from, pos)) != string::npos)
    {
        fileText.replace(pos, from.length(), to);
        pos += to.length(); // Move past the replacement to avoid infinite loop
    }

    std::ofstream(filePath) << fileText;
}

int main()
{
    {
        // For clang/ dirs
        map<string, vector<string>> dirToFileNames;

        populateRecursiveDirectoryMap(current_path() / "clang/include/clang", dirToFileNames);

        {
            const string s = current_path() / "clang/include/";
            for (const auto &[dir, fileNames] : dirToFileNames)
            {
                string prepend(dir.begin() + s.size(), dir.end());
                prepend += '/';
                replaceFileTexts(dir, fileNames, prepend, "");
            }
        }

        replaceFileTextsRecursive(current_path() / "clang/lib/Basic",
                                  getFileNames(current_path() / "clang/lib/Basic/Targets"), "Targets/", "");
    }

    // The map to populate. For llvm/ dirs
    map<string, vector<string>> dirToFileNames;

    populateRecursiveDirectoryMap(current_path() / "llvm/include", dirToFileNames);

    {
        const string s = current_path() / "llvm/include/";
        for (const auto &[dir, fileNames] : dirToFileNames)
        {
            string prepend(dir.begin() + s.size(), dir.end());
            prepend += '/';
            replaceFileTexts(dir, fileNames, prepend, "");
        }

        replaceInSingleFile("clang/lib/AST/ByteCode/InterpBuiltin.cpp", "../ExprConstShared.h", "ExprConstShared.h");
        replaceInSingleFile("clang/lib/AST/ByteCode/Interp.h", "../ExprConstShared.h", "ExprConstShared.h");

        replaceFileTextsRecursive(current_path() / "clang/lib/AST",
                                  getFileNames(current_path() / "clang/lib/AST/ByteCode"), "ByteCode/", "");

        replaceFileTextsRecursive(current_path() / "clang/lib/Driver",
                                  getFileNames(current_path() / "clang/lib/Driver/ToolChains/Arch"), "ToolChains/Arch/",
                                  "");

        replaceFileTextsRecursive(current_path() / "clang/lib/Driver",
                                  getFileNames(current_path() / "clang/lib/Driver/ToolChains"), "ToolChains/", "");

        replaceFileTextsRecursive(current_path() / "clang/lib/Driver",
                                  getFileNames(current_path() / "clang/lib/Driver/ToolChains"), "ToolChains/", "");

        vector<string> archFileNames = getFileNames(current_path() / "clang/lib/Driver/ToolChains/Arch/");
        {
            for (string &arch : archFileNames)
            {
                arch = "Arch/" + arch;
            }

            replaceFileTextsRecursive(current_path() / "clang/lib/Driver", archFileNames, "ToolChains/", "");
        }

        // NCC is a macro defined in ioctl-types.h
        replaceInSingleFile(current_path() / "clang/lib/Serialization/ASTWriter.cpp", "NCC", "NCC_");

        replaceInSingleFile(current_path() / "clang/lib/Driver/ToolChains/Arch/RISCV.cpp", "../Clang.h",
                            "ToolChains/Clang.h");

        replaceInSingleFile(current_path() / "clang/lib/Driver/ToolChains/Arch/LoongArch.cpp", "../Clang.h",
                            "ToolChains/Clang.h");

        replaceInSingleFile(current_path() / "clang/include/clang/ExtractAPI/Serialization/APISetVisitor.h",
                            "../APIRecords.inc", "clang/ExtractAPI/APIRecords.inc");

        replaceFileTextsRecursive(current_path() / "llvm/utils/TableGen/Common/GlobalISel",
                                  getFileNames(current_path() / "llvm/utils/TableGen/Common/GlobalISel"),
                                  "Common/GlobalISel/", "");
        replaceFileTextsRecursive(current_path() / "llvm/utils/TableGen/Common",
                                  getFileNames(current_path() / "llvm/utils/TableGen/Common"), "Common/", "");
    }

    const string str = current_path() / "llvm/include/llvm/Demangle";
    vector<string> &demangleNames = dirToFileNames.at(str);
    replaceFileTexts(current_path() / "llvm/lib/Demangle", demangleNames, "llvm/Demangle/", "");
    replaceFileTextsRecursive(current_path() / "llvm/lib/Support",
                              getFileNames(current_path() / "llvm/lib/Support/Unix"), "Unix/", "");

    vector<string> binaryFormatElfFiles = getFileNames(current_path() / "llvm/include/llvm/BinaryFormat/ELFRelocs");
    for (string &n : binaryFormatElfFiles)
    {
        n = "ELFRelocs/" + n;
    }
    replaceFileTextsRecursive(current_path() / "llvm/include/llvm/BinaryFormat", binaryFormatElfFiles,
                              "llvm/BinaryFormat/", "");

    replaceInSingleFile("llvm/lib/BinaryFormat/ELF.cpp", "ELFRelocs/x86_64.def",
                        "llvm/BinaryFormat/ELFRelocs/x86_64.def");

    replaceFileTextsRecursive(current_path() / "llvm/lib/CodeGen",
                              getFileNames(current_path() / "llvm/lib/CodeGen/LiveDebugValues"), "LiveDebugValues/",
                              "");

    replaceFileTextsRecursive(current_path() / "llvm/lib/Target/X86",
                              getFileNames(current_path() / "llvm/lib/Target/X86/MCTargetDesc"), "MCTargetDesc/", "");

    replaceFileTextsRecursive(current_path() / "llvm/lib/Target/X86",
                              getFileNames(current_path() / "llvm/lib/Target/X86/GISel"), "GISel/", "");

    string from = R"(#if !__has_include(GET_PASS_REGISTRY)
#error "must provide <Target>PassRegistry.def"
#endif)";

    string to = R"(#include GET_PASS_REGISTRY)";
    replaceInSingleFile("llvm/include/llvm/Passes/TargetPassRegistry.inc", from, to);

    replaceFileTextsRecursive(current_path() / "llvm/utils/TableGen",
                              getFileNames(current_path() / "llvm/utils/TableGen/Common"), "Common/", "");

    replaceFileTextsRecursive(current_path() / "llvm/utils/TableGen",
                              getFileNames(current_path() / "llvm/utils/TableGen/Basic"), "Basic/", "");
    return 0;
}