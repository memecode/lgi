#include <WinSock2.h>
#include <windows.h>
#include <psapi.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Roster.h"

LRoster::LRoster()
{
}

LRoster::~LRoster()
{
}

bool LRoster::GetRunningAppInfo(PID process, AppInfo *info, LError *err) const
{
    auto hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process );
    if (!hProcess)
    {
        if (err)
            *err = GetLastError();
        return false;
    }

    HMODULE hMod;
    DWORD needed;
    TCHAR szProcessName[MAX_PATH] = TEXT("");
    auto status = EnumProcessModules(hProcess, &hMod, sizeof(hMod), &needed);
    if (status)
    {
        GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) );
        info->name = szProcessName;
        LAssert(info->name);
    }

    CloseHandle(hProcess);
    return status;
}

bool LRoster::Terminate(PID process, LError *err) const
{
    auto hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process);
    if (!hProcess)
    {
        if (err)
            *err = GetLastError();
        return false;
    }

    bool result = TerminateProcess(hProcess, -1);
    if (!result && err)
        *err = GetLastError();

    CloseHandle(hProcess);
    return result;
}

bool LRoster::GetAppList(LArray<LRoster::PID> &processes, LError *err) const
{
    DWORD handles[1024], needed = 0;
    if (!EnumProcesses(handles, sizeof(handles), &needed))
    {
        if (err)
            *err = GetLastError();
        return false;
    }

    auto count = needed / sizeof(DWORD);
    for (int i=0; i<count; i++)
        if (handles[i])
            processes.Add(handles[i]);

	return true;
}
