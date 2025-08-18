
#ifndef HMAKE_HashedCommand_HPP
#define HMAKE_HashedCommand_HPP
#ifdef USE_HEADER_UNITS
import <PlatformSpecific.hpp>;
#else
#include "PlatformSpecific.hpp"
#endif

using std::string;

class HashedCommand
{
    string command;

  public:
    void setCommand(string command_);
    string getCommand() const;
#ifdef USE_COMMAND_HASH
    uint64_t getHash() const;
#else
    string_view getHash() const;
#endif

#ifdef USE_COMMAND_HASH
  private:
    uint64_t hash = 0;
#endif
};

#endif // HMAKE_HashedCommand_HPP
