
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
	auto path = p / LString::Fmt("%i", process) / "exe";
	
	char full[MAX_PATH_LEN] = "#errNoLink";
	if (!LResolveShortcut(path, full, sizeof(full)))
	{
		if (err)
			err->Set(LErrorPathNotFound, LString::Fmt("%s not found", path.GetFull().Get()));
		return false;
	}

	info->name = full;
	return true;
}

bool LRoster::Terminate(PID process, LError *err) const
{
	return false;
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
