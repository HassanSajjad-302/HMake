#include "Configure.hpp"

struct Process : BTarget
{
    static constexpr const char *cmd = "./a.out";

    bool isEventRegistered(Builder &builder) override
    {
        // Pass true so HMake keeps the child's stdin pipe open — this process
        // sends a message to the build-system and then waits for a reply before
        // continuing.
        run.startAsyncProcess(cmd, builder, this, /*keepStdinOpen=*/true);
        // launched async process. would have returned false otherwise (e.g. a module-file was already updated).
        // HMake will call isEventCompleted when process exits or there is message for build-system.
        return true;
    }

    bool firstReceived = false;

    bool isEventCompleted(Builder &builder, const string_view message) override
    {
        if (message.empty())
        {
            // Empty message means the child process has exited. exitStatus reflects the exitStatus of child process.
            const bool ok = run.exitStatus == EXIT_SUCCESS;

            string out = getColorCode(ok ? ColorIndex::light_green : ColorIndex::red);
            out += ok ? "./a.out finished successfully:\n" : "./a.out failed:\n";
            out += getColorCode(ColorIndex::reset);
            out += run.output;
            printMessage(out);

            return false; // stop waiting; we are done with this target
        }

        // The child sent an IPC message to the build-system (distinguished from ordinary stdout by the delimiter
        // protocol).  Print it, then on the first message reply with the module name it requested.

        string out = getColorCode(ColorIndex::orange);
        out += "./a.out → build-system message:\n";
        out += getColorCode(ColorIndex::reset);
        out += string{message};
        printMessage(out);

        if (!firstReceived)
        {
            // Reply: send the module name the child asked for.
            const string reply = "std\n";
            if (write(run.writePipe, reply.data(), reply.size()) == -1)
            {
                printMessage("Warning: failed to write to child stdin\n");
            }
            firstReceived = true;
        }

        return true; // keep listening; more messages or exit event may follow
    }
};

void buildSpecification()
{
    // Any BTarget must have application life-time.
    new Process();
}

MAIN_FUNCTION