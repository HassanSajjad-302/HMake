/// \file
/// Declares compiler dependency-output parsing shared by ordinary compilation and bootstrap take-off.

#ifndef HMAKE_PARSEHEADERDEPS_HPP
#define HMAKE_PARSEHEADERDEPS_HPP

#include "gtl/include/gtl/phmap.hpp"
#include <string>
#include <string_view>

class Node;
struct Compiler;

/// Parses compiler dependencies and removes MSVC showIncludes records from output.
/// MSVC uses showIncludes when dependencyFile is empty and /sourceDependencies otherwise; GCC-family compilers
/// use Make dependency syntax. `workingDirectory` is an absolute normalized directory without a trailing separator;
/// relative paths are resolved against it and normalized lexically.
/// The compiled source is always excluded; configure-tree headers are excluded only when requested.
gtl::flat_hash_set<Node *> parseHeaderDeps(std::string &output, const Compiler &compiler, int exitStatus,
                                           const std::string &dependencyFile, std::string_view workingDirectory,
                                           const Node *compiledSource, bool excludeHeadersInConfigureNode);

#endif // HMAKE_PARSEHEADERDEPS_HPP
