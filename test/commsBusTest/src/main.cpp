#include "lgi/common/Lgi.h"
#include "lgi/common/CommsBus.h"

struct PrintfLog : public LStream
{
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
    {
        printf("%.*s", (int)Size, (const char*)Ptr);
        return Size;
    }
};

int main(int args, const char **arg)
{
    PrintfLog log;
    OsAppArguments appArgs(args, arg);
    LApp app(appArgs, "commsBusTest");
    LCommsBus bus(&log);
  	LCancel cancel;

    auto ideEp  = "lgi.ide.test";
    auto respEp = "lgi.test.response";
    bus.Listen(respEp, [&](auto data)
        {
        	printf("Got response: %s\n", data.Get());
        	cancel.Cancel();
        });

    bus.SendMsg(ideEp, respEp);

    // The comms bus runs in a thread, so this just needs to not exit...
    while (!cancel.IsCancelled())
        LSleep(50);
        
    return 0;
}