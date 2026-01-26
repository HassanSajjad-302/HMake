#include "Configure.hpp"
#include <fstream>

// lineBegin should be ```cpp line while lineEnd should be the line before ```.
void replaceLinesWithContent(const uint64_t lineBegin, const uint64_t lineEnd, const string &exampleName)
{
    const string readMe = fileToString("README.md");
    vector<string_view> lines = split(readMe, '\n');

    const path p = "Examples/" + exampleName + "/hmake.cpp";
    if (!exists(p))
    {
        printErrorMessage(FORMAT("path {} not found\n", p.string()));
    }

    const string example = fileToString(p.string());
    const vector<string_view> exampleFileLines = split(example, '\n');

    const vector<string> secondPartOutputReadme{lines.begin() + lineEnd, lines.end()};
    lines.erase(lines.begin() + lineBegin, lines.end());
    string output;

    for (const string_view &str : lines)
    {
        output += string(str) + '\n';
    }

    for (const string_view &str : exampleFileLines)
    {
        output += string(str) + '\n';
    }

    for (const string &str : secondPartOutputReadme)
    {
        output += string(str) + '\n';
    }

    std::ofstream("README.md") << output;
}

int main()
{
    // lineBegin should be ```cpp line while lineEnd should be the line before ```.
    replaceLinesWithContent(1151, 1173, "Example10");
}