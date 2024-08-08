
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <stack>;
import <utility>;
#else
#include "LinkOrArchiveTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <stack>
#include <utility>
#endif

using std::ofstream, std::filesystem::create_directories, std::ifstream, std::stack, std::lock_guard, std::mutex;

bool operator<(const LinkOrArchiveTarget<> &lhs, const LinkOrArchiveTarget<> &rhs)
{
    return lhs.targetSubDir < rhs.targetSubDir;
}
