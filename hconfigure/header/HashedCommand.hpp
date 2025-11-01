
#ifndef HMAKE_HashedCommand_HPP
#define HMAKE_HashedCommand_HPP

#include "PlatformSpecific.hpp"

using std::string;

class HashedCommand
{
    string_view command;

  public:
    void setCommand(string_view command_);
    string_view getCommand() const;
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
