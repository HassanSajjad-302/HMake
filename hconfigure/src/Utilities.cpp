
#ifdef USE_HEADER_UNITS
import "Utilities.hpp";
import "BuildSystemFunctions.hpp";
import <fstream>;
import <sstream>;
import <format>;
#else
#include "Utilities.hpp"
#include "BuildSystemFunctions.hpp"
#include <format>
#include <fstream>
#include <sstream>
#endif

using std::ifstream, std::format;

string addQuotes(const string &pathString)
{
    return "\"" + pathString + "\"";
}

string addEscapedQuotes(const string &pathString)
{
    const string str = R"(\")";
    return str + pathString + str;
}

string file_to_string(const string &file_name)
{
    ifstream file_stream{file_name};

    if (file_stream.fail())
    {
        // Error opening file.
        printErrorMessage(format("Error opening file {}\n", file_name));
        exit(EXIT_FAILURE);
    }

    std::ostringstream str_stream;
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        printErrorMessage(format("Error reading file {}\n", file_name));
        exit(EXIT_FAILURE);
    }

    return str_stream.str();
}

vector<string> split(string str, const string &token)
{
    vector<string> result;
    while (!str.empty())
    {
        std::string::size_type index = str.find(token);
        if (index != string::npos)
        {
            result.emplace_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.empty())
            {
                result.emplace_back(str);
            }
        }
        else
        {
            result.emplace_back(str);
            str = "";
        }
    }
    return result;
}
