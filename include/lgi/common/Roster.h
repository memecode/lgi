#pragma once

// Manage processes
class LgiClass LRoster
{
public:
	#if WINDOWS
	typedef DWORD PID;
	#else
	typedef int PID;
	#endif

	struct AppInfo
	{
		LString name;
	};

	LRoster();
	~LRoster();

	bool GetAppList(LArray<PID> &processes, LError *err = nullptr) const;

	bool GetRunningAppInfo(PID process, AppInfo *info, LError *err = nullptr) const;
	bool Terminate(PID process, LError *err = nullptr) const;
};