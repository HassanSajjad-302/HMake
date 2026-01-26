
#ifndef HMAKE_HashedCommand_HPP
#define HMAKE_HashedCommand_HPP

#include "BuildSystemFunctions.hpp"

using std::string;

class HashedCommand
{
  public:
    void setCommand(string_view command_);
#ifdef USE_COMMAND_HASH
    uint64_t getHash() const;
#else
    string_view getHash() const;
#endif

#ifdef USE_COMMAND_HASH
  private:
    uint64_t hash = 0;
#else
  private:
    string_view command;
#endif
};

#endif // HMAKE_HashedCommand_HPP
