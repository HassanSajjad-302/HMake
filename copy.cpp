#include "Configure.hpp"

// lineBegin should be ```cpp line while lineEnd should be the line before ```.
void replaceLinesWithContent(const uint64_t lineBegin, const uint64_t lineEnd, const string &exampleName)
{
    vector<string> lines = split(fileToPString("README.md"), "\n");
    path p = "Examples/" + exampleName + "/hmake.cpp";
    if (!exists(p))
    {
        printErrorMessage(FORMAT("path {} not found\n", p.string()));
    }
    const vector<string> exampleFileLines = split(fileToPString(p.string()), "\n");

    const vector<string> secondPartOutputReadme{lines.begin() + lineEnd, lines.end()};
    lines.erase(lines.begin() + lineBegin, lines.end());
    string output;
    for (const string &str : lines)
    {
        output += str + '\n';
    }
    for (const string &str : exampleFileLines)
    {
        output += str + '\n';
    }
    for (const string &str : secondPartOutputReadme)
    {
        output += str + '\n';
    }

    std::ofstream("README.md") << output;
}

int main()
{
    // lineBegin should be ```cpp line while lineEnd should be the line before ```.
    replaceLinesWithContent(1178, 1328, "Example11");
}