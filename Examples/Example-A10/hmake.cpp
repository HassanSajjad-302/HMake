#include "Configure.hpp"

struct Process : BTarget
{
    explicit Process(const string &name_) : BTarget(name_, false, BTargetType::UNKNOWN)
    {
    }
    static inline char cmd[] = "./a.out";

    bool isEventRegistered(Builder &builder) override
    {
        // Pass true so HMake keeps the child's stdin pipe open — this process
        // sends a message to the build-system and then waits for a reply before
        // continuing.
        run.startAsyncProcess(cmd, builder, this, /*haveWritePipe=*/true);
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
            out += *run.output;
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
            // Reply, then immediately arm the read for the child's next message.
            const string reply = "std\n";
            run.writeReadExpected(reply);
            firstReceived = true;
        }
        else
        {
            // No reply is needed for the final message, but the exit notification still needs a read armed.
            run.startRead();
        }

        return true; // keep listening; more messages or exit event may follow
    }
};

void buildSpecification()
{
    // Any BTarget must have application life-time.
    new Process("Process");
}

MAIN_FUNCTION
