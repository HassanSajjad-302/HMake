
#ifndef HMAKE_OS_HPP
#define HMAKE_OS_HPP

#include <cstdint>

enum class OS : uint8_t
{
    AIX,
    CYGWIN,
    LINUX,
    MACOSX,
    NT,
    VMS,
};

#endif // HMAKE_OS_HPP
