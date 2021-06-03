

#ifndef HMAKE_INITIALIZE_HPP
#define HMAKE_INITIALIZE_HPP

#include "Version.hpp"
#include "string"

void initializeCache(int argc, const char **argv);
void initializeProject(const std::string &projectName, Version projectVersion);
void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion = Version{0, 0, 0});

#endif//HMAKE_INITIALIZE_HPP
