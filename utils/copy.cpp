#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>

// Reads an entire file into a vector of lines.
// Returns false if file cannot be opened (instead of exiting).
static bool readLines(const std::string &path, std::vector<std::string> &out) {
    std::ifstream f(path);
    if (!f) {
        return false;
    }
    std::string line;
    while (std::getline(f, line))
        out.push_back(line);
    return true;
}

int main(int argc, char *argv[]) {

    const std::string readmePath  = "README.md";
    const std::string examplesDir = "Examples/";

    std::vector<std::string> readmeLines;
    if (!readLines(readmePath, readmeLines)) {
        std::cerr << "Error: cannot open file: " << readmePath << "\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Patterns
    // -----------------------------------------------------------------------
    // FIX 1: "## HMake Architecture" was not matching "## HMake Architecture Examples"
    const std::regex reArchSection(R"(^## HMake Architecture)");

    // FIX 2: "## C\+\+ Examples" stays correct, but confirm it matches the actual heading
    const std::regex reCppSection(R"(^## Examples: C\+\+)");

    const std::regex reAnyH2      (R"(^## )");

    // FIX 3: The old pattern was ### Example (\d+)\s*$ which required end-of-line right
    // after the digits. The headings now have trailing text like "— Minimal executable".
    // New pattern: match ### Example N optionally followed by anything (space, dash, text).
    const std::regex reExampleH3  (R"(^### Example (\d+)(?:\s|$))");

    // -----------------------------------------------------------------------
    // State machine
    // -----------------------------------------------------------------------
    enum class Section { NONE, ARCH, CPP };

    Section currentSection = Section::NONE;
    int     currentExample = -1;
    bool    inDetails      = false;   // inside <details><summary>...</summary>
    bool    inCodeBlock    = false;   // inside the ```cpp … ``` we are replacing
    // FIX 4: Track which file we are replacing (hmake.cpp or main.cpp)
    std::string currentSummaryFile;

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
                currentSummaryFile = "hmake.cpp";
                output.push_back(line);
                continue;
            }

            // FIX 4: Also detect <summary>main.cpp</summary> for multi-file examples
            if (currentExample != -1 &&
                line.find("<summary>main.cpp</summary>") != std::string::npos) {
                inDetails = true;
                currentSummaryFile = "main.cpp";
                output.push_back(line);
                continue;
            }

            // </details> resets the details flag
            if (line.find("</details>") != std::string::npos) {
                inDetails = false;
                currentSummaryFile.clear();
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
                    folder = examplesDir + "Example-A" + std::to_string(currentExample);
                else
                    folder = examplesDir + "Example"   + std::to_string(currentExample);

                // FIX 4: Use the detected filename (hmake.cpp or main.cpp)
                const std::string fileName = currentSummaryFile.empty() ? "hmake.cpp" : currentSummaryFile;
                const std::string srcPath  = folder + "/" + fileName;

                std::vector<std::string> srcLines;
                // FIX 5: Gracefully skip missing example directories instead of crashing
                if (!readLines(srcPath, srcLines)) {
                    std::cerr << "  Warning: source file not found, skipping: " << srcPath << "\n";
                    // Leave old content in place — the closing ``` will be kept by the
                    // inCodeBlock branch below (it stops skipping at the closing fence).
                    // But we already emitted the opening fence, so we must re-emit the
                    // old content. We do that by NOT setting inCodeBlock so the lines
                    // pass through normally.
                    inCodeBlock = false;
                } else {
                    std::cout << "  Updating Example " << currentExample
                              << " (" << (currentSection == Section::ARCH ? "Architecture" : "C++")
                              << ") [" << fileName << "] from: " << srcPath << "\n";

                    for (const std::string &sl : srcLines)
                        output.push_back(sl);
                    // Old content will be skipped in the inCodeBlock branch below.
                }

                continue;
            }

            // Default: copy the line as-is
            output.push_back(line);

        } else {
            // ── We are inside a code block being replaced ──────────────────
            // Skip old lines until we hit the closing fence
            if (line == "```") {
                inCodeBlock = false;
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