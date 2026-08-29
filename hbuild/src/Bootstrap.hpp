#ifndef HMAKE_HBUILD_BOOTSTRAP_HPP
#define HMAKE_HBUILD_BOOTSTRAP_HPP

/// Parses the public hbuild command line, performs the required bootstrap/configure
/// take-off, and finally dispatches the generated build executable.
int runBootstrap(int argc, char **argv);

#endif
