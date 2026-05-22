#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

static std::string fileToString(const std::string_view fileName)
{
    std::string fileBuffer;
    FILE *fp;

#ifdef WIN32
    fopen_s(&fp, fileName.data(), "rb");
#else
    fp = fopen(fileName.data(), "r");
#endif

    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileBuffer.resize(filesize);
    const uint64_t readLength = fread(fileBuffer.data(), 1, filesize, fp);
    fclose(fp);
    return fileBuffer;
}

using namespace std;
int main(const int argc, char **argv)
{
    if (argc != 4)
    {
        exit(-1);
    }

    // first argument is the macroName
    const string macroName = argv[1];
    // we read this file to get the value of the macro
    const string macroValueInputFile = argv[2];
    const string macroValue = fileToString(macroValueInputFile);
    const string fileContent = "#define " + macroName + " " + macroValue + "\n";
    ofstream(argv[3]) << fileContent;
}
