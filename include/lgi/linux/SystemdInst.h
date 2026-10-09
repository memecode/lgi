#pragma once

#include "lgi/common/LgiCommon.h"

class LSystemD_Install
{
    LString appName;
    LStream *log = nullptr;

public:
	LSystemD_Install(const char *AppName, LStream *Log = nullptr) :
        appName(AppName),
        log(Log)
	{
	}

	LString ServiceFile()
	{
		return LString("/etc/systemd/system/") + appName + ".service";
	}
	
	int Install(const char *Description)
	{
		LFile f;
		auto sf = ServiceFile();
		if (!f.Open(sf, O_WRITE))
		{
            if (log)
                log->Print("Error: can't open '%s' for writing, most like due to lack of permissions.\n", sf.Get());
            return -1;
		}
		f.SetSize(0);
		
		LFile::Path working(LSP_APP_INSTALL);
		auto exe = LGetExeFile();
		
		f.Print("[Unit]\n"
				"Description=%s.\n"
				"\n"
				"[Service]\n"
				"WorkingDirectory=%s\n"
				"ExecStart=%s\n"
				"Type=simple\n"
				"TimeoutStopSec=10\n"
				"Restart=on-failure\n"
				"RestartSec=5\n"
				"User=%s\n"
				"\n"
				"[Install]\n"
				"WantedBy=multi-user.target\n",
                Description,
				(working / "..").GetFull().Get(),
				exe.Get(),
				LCurrentUserName().Get());
		f.Close();
		
		// Set the permissions on the service file:
		auto chmod = LString::Fmt("chmod 644 %s", sf.Get());
		auto result = system(chmod);
		if (result)
		{
			if (log)
                log->Print("Error: failed to chmod '%s'.\n", sf.Get());
			return result;
		}
		
		// Enable the server:
		if (log)
            log->Print("Enabling server...\n");
		auto enable = LString::Fmt("systemctl enable %s", appName.Get());
		result = system(enable);
		if (result)
		{
			if (log)
                log->Print("Error: failed to enable service '%s': %s\n", appName.Get(), enable.Get());
			return result;
		}

		// Start the server:
		if (log)
            log->Print("Starting server...\n");
		auto start = LString::Fmt("systemctl start %s", appName.Get());
		result = system(start);
		if (result)
		{
			if (log)
                log->Print("Error: failed to start service '%s': %s\n", appName.Get(), start.Get());
			return result;
		}
		
		if (log)
            log->Print("Install successful\n");
		return 0;
	}
	
	int Uninstall()
	{
		// Stop the server:
		if (log)
            log->Print("Stopping server...\n");
		auto stop = LString::Fmt("systemctl stop %s", appName.Get());
		auto result = system(stop);
		if (result)
			if (log)
                log->Print("Error: failed to stop service '%s': %s\n", appName.Get(), stop.Get());

		// Disable the server:
		if (log)
            log->Print("Disabling server...\n");
		auto disable = LString::Fmt("systemctl disable %s", appName.Get());
		result = system(disable);
		if (result && log)
            log->Print("Error: failed to disable service '%s': %s\n", appName.Get(), disable.Get());

		// Remove the service file...
		auto sf = ServiceFile();
		if (LFileExists(sf))
		{
			LError err;
			if (!FileDev->Delete(sf, &err, false))
			{
				if (log)
                    log->Print("Error: can't remove '%s': %s\n", sf.Get(), err.ToString().Get());
				return -1;
			}
		}
		
		// systemctl daemon-reload
		if (log)
            log->Print("Reloading daemons...\n");
		LString reload = "systemctl daemon-reload";
		result = system(reload);
		if (result)
			if (log)
                log->Print("Error: failed to reload daemons: %s\n", appName.Get(), reload.Get());
		
		if (log)
            log->Print("Uninstall successful.\n");
		return 0;
	}
};