
#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "Package.hpp"
#include "Project.hpp"
void configure(const Library &library);
void configure(const Executable &ourExecutable);
void configure();
void configure(const Package &package);

#endif//HMAKE_CONFIGURE_HPP
