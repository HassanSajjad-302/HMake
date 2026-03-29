#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

// ---------------------------------------------------------------------------
// P2978 IPC protocol helpers
//
// HMake distinguishes build-system messages from ordinary stdout by checking for
// a fixed 32-byte delimiter after each message.  The format of one message is:
//
//   <payload bytes>  <uint32 payload-length (LE)>  <32-byte delimiter>
//
// HMake strips message from the child's normal stdout which is run.output in
// the build-system
// ---------------------------------------------------------------------------

namespace ipc
{

// The delimiter must match the one compiled into HMake exactly.
inline constexpr char delimiter[] =
    "DELIMITER"
    "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
    "DELIMITER"; // 32 bytes total

static void writeAll(int fd, const char *buf, std::size_t len)
{
    std::size_t written = 0;
    while (written < len)
    {
        const ssize_t n = ::write(fd, buf + written, len - written);
        if (n == -1)
        {
            if (errno == EINTR)
                continue; // interrupted by signal — retry
            std::perror("ipc::writeAll");
            std::exit(EXIT_FAILURE);
        }
        written += static_cast<std::size_t>(n);
    }
}

// Send a single IPC message to the build-system.
// The payload is an arbitrary string; the build-system receives it verbatim
// inside isEventCompleted(builder, message).
void send(const std::string &payload)
{
    // Frame layout: payload | uint32 length | delimiter
    const auto len = static_cast<uint32_t>(payload.size());

    std::string frame;
    frame.reserve(payload.size() + sizeof(uint32_t) + sizeof(delimiter) - 1);
    frame.append(payload);
    frame.append(reinterpret_cast<const char *>(&len), sizeof(len)); // little-endian on x86/ARM
    frame.append(delimiter, sizeof(delimiter) - 1);                  // exclude null terminator

    writeAll(STDOUT_FILENO, frame.data(), frame.size());
}

} // namespace ipc

int main()
{
    // Tell the build-system which module we need.  HMake will call
    // isEventCompleted() with this string and can write a reply to our stdin.
    ipc::send("First message to build-system: this module depends on 'std'. Please provide it.\n");

    // Wait for the build-system's reply (the module name).
    std::string module;
    if (!(std::cin >> module))
    {
        std::cerr << "main: failed to read module name from build-system\n";
        return EXIT_FAILURE;
    }

    std::cout << "Hello World\n";
    std::cout << "Module received: " << module << "\nYey\n";

    // Optionally send a second message — demonstrates multiple IPC rounds.
    ipc::send("Final message to build-system: compilation finished.\n");

    return EXIT_SUCCESS;
}