#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

// Reads an entire file into a vector of lines.
static std::vector<std::string> readLines(const std::string &path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Error: cannot open file: " << path << "\n";
        std::exit(1);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line))
        lines.push_back(line);
    return lines;
}

int main(int argc, char *argv[]) {

    const std::string readmePath  = "README.md";
    const std::string examplesDir = "Examples/";

    std::vector<std::string> readmeLines = readLines(readmePath);

    // -----------------------------------------------------------------------
    // Patterns
    // -----------------------------------------------------------------------
    const std::regex reArchSection(R"(^## HMake Architecture\s*$)");
    const std::regex reCppSection (R"(^## C\+\+ Examples\s*$)");
    const std::regex reAnyH2      (R"(^## )");
    const std::regex reExampleH3  (R"(^### Example (\d+)\s*$)");

    // -----------------------------------------------------------------------
    // State machine
    // -----------------------------------------------------------------------
    enum class Section { NONE, ARCH, CPP };

    Section currentSection = Section::NONE;
    int     currentExample = -1;
    bool    inDetails      = false;   // inside <details><summary>hmake.cpp</summary>
    bool    inCodeBlock    = false;   // inside the ```cpp … ``` we are replacing

    std::vector<std::string> output;
    output.reserve(readmeLines.size());

    for (const std::string &line : readmeLines) {

        // ── Section tracking (only when we are not replacing code) ──────────
        if (!inCodeBlock) {

            // Detect our two big headings
            if (std::regex_search(line, reArchSection)) {
                currentSection = Section::ARCH;
                currentExample = -1;
                inDetails      = false;
                output.push_back(line);
                continue;
            }
            if (std::regex_search(line, reCppSection)) {
                currentSection = Section::CPP;
                currentExample = -1;
                inDetails      = false;
                output.push_back(line);
                continue;
            }
            // Any other H2 resets the section
            if (std::regex_search(line, reAnyH2)) {
                currentSection = Section::NONE;
                currentExample = -1;
                inDetails      = false;
                output.push_back(line);
                continue;
            }

            // Detect ### Example N inside one of our sections
            if (currentSection != Section::NONE) {
                std::smatch m;
                if (std::regex_search(line, m, reExampleH3)) {
                    currentExample = std::stoi(m[1].str());
                    inDetails      = false;
                    output.push_back(line);
                    continue;
                }
            }

            // Detect <summary>hmake.cpp</summary> (signals the details block)
            if (currentExample != -1 &&
                line.find("<summary>hmake.cpp</summary>") != std::string::npos) {
                inDetails = true;
                output.push_back(line);
                continue;
            }

            // </details> resets the details flag
            if (line.find("</details>") != std::string::npos) {
                inDetails = false;
                output.push_back(line);
                continue;
            }

            // Detect the opening ```cpp of the block we want to replace
            if (inDetails && line == "```cpp") {
                inCodeBlock = true;
                output.push_back(line);   // keep the fence itself

                // Build path to the source file
                std::string folder;
                if (currentSection == Section::ARCH)
                    folder = examplesDir + "/Example-A" + std::to_string(currentExample);
                else
                    folder = examplesDir + "/Example"   + std::to_string(currentExample);

                const std::string srcPath = folder + "/hmake.cpp";
                const std::vector<std::string> srcLines = readLines(srcPath);

                std::cout << "  Updating Example " << currentExample
                          << " (" << (currentSection == Section::ARCH ? "Architecture" : "C++")
                          << ") from: " << srcPath << "\n";

                for (const std::string &sl : srcLines)
                    output.push_back(sl);

                // Old content will be skipped (handled in the inCodeBlock branch below)
                continue;
            }

            // Default: copy the line as-is
            output.push_back(line);

        } else {
            // ── We are inside a code block being replaced ──────────────────
            // Skip old lines until we hit the closing fence
            if (line == "```") {
                inCodeBlock = false;
                inDetails   = false;
                output.push_back(line);   // keep the closing fence
            }
            // else: discard old content
        }
    }

    // -----------------------------------------------------------------------
    // Write the updated README back
    // -----------------------------------------------------------------------
    std::ofstream fout(readmePath);
    if (!fout) {
        std::cerr << "Error: cannot write to: " << readmePath << "\n";
        return 1;
    }
    for (std::size_t i = 0; i < output.size(); ++i) {
        fout << output[i];
        if (i + 1 < output.size())
            fout << '\n';
    }
    fout.close();

    std::cout << "README.md updated successfully.\n";
    return 0;
}