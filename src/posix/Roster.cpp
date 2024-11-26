#include <signal.h>

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
	if (!info)
		return false;

	LFile::Path p("/proc");
	auto folder = p / LString::Fmt("%i", process);
	
	char full[MAX_PATH_LEN] = "#errNoLink";
	auto resolve = LResolveShortcut(folder / "exe", full, sizeof(full));
	if (resolve)
	{
		// Full path to the original binary:
		info->name = full;
	}
	else
	{
		// Ok, maybe we don't have perms to 'exe', try the 'stat' file instead...
		// We might be able to get the process name (no path).
		auto statPath = (folder / "stat").GetFull();
		LFile stat(statPath);
		if (stat)
		{
			char content[512] = "";
			auto rd = read(stat.Handle(), content, sizeof(content));			
			auto parts = LString(content, rd).SplitDelimit();
			if (parts.Length() > 1)
				info->name = parts[1].Strip("()"); // process name without path
			else
				return false;
		}
		else
		{
			if (err)
				err->Set(LErrorPathNotFound, LString::Fmt("%s not found", folder.GetFull().Get()));
			return false;
		}
	}

	return true;
}

bool LRoster::Terminate(PID process, LError *err) const
{
	int r = kill(process, SIGTERM);
	if (r && err)
		*err = errno;
	return r == 0;
}

bool LRoster::GetAppList(LArray<LRoster::PID> &processes, LError *err) const
{
	LDirectory d;
	
	auto b = d.First("/proc");
	if (!b)
	{
		printf("%s:%i - first failed.\n", _FL);
		return false;
	}
	
	for (; b; b = d.Next())
	{
		auto nm = d.GetName();
		if (d.IsDir() && IsDigit(*nm))
		{
			auto pid = Atoi(nm);
			if (pid > 0)
			{
				processes.Add(pid);
			}
			else printf("%s:%i - atoi failed.\n", _FL);
		}
		// else printf("%s:%i - not numeric '%s'.\n", _FL, nm);
	}
	
	printf("%s:%i - LRoster::GetAppList done.\n", _FL);
	return true;
}
