#include "lgi/common/Lgi.h"
#include "lgi/common/UdpTransport.h"

struct PrintfLog : public LStream
{
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
    {
        printf("%.*s", (int)Size, (const char*)Ptr);
        return Size;
    }
};

bool Test(uint32_t destIp, bool write)
{
    PrintfLog log;
    if (write)
        return LUdpTransport::UnitTest_Sender(destIp, &log);
    else
        return LUdpTransport::UnitTest_Receiver(&log);
}

int main(int args, const char **arg)
{
    OsAppArguments appArgs(args, arg);
    LApp app(appArgs, "commsBusTest");
    
    const char *val = nullptr;
    if (appArgs.Get("read"))
    {
        return !Test(0, false);
    }
    else if (appArgs.Get("write", &val))
    {
        if (val)
            return !Test(LIpToInt(val), true);
        
        printf("Error: missing IP value.\n");
    }
    else
    {
        printf("Usage: %s [-read][-write $ipAddr]\n", arg[0]);
    }

    return 0;
}