#include "lgi/common/Lgi.h"
#include "resdefs.h"
#include "lgi/common/TextLog.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "VsLaunch";

struct VsInfo
{
	int Year;
	int MscVer;
	double Ver;
	bool x64;
}
Versions[] =
{
	{2022, _MSC_VER_VS2022, 17.0, true},
	{2019, _MSC_VER_VS2019, 16.0, false},
	{2017, _MSC_VER_VS2017, 15.0, false},
	{2015, _MSC_VER_VS2015, 14.0, false},
	{2013, _MSC_VER_VS2013,	12.0, false}
};

VsInfo *GetVsInfo(int VsYear)
{
	for (int i=0; i<CountOf(Versions); i++)
	{
		if (Versions[i].Year == VsYear)
			return Versions + i;
	}
	
	return NULL;
}

double YearToVer(int64 y)
{
	for (int i=0; i<CountOf(Versions); i++)
	{
		if (Versions[i].Year == y)
			return Versions[i].Ver;
	}
	
	return 0.0;
}

int VerToYear(double v)
{
	for (int i=0; i<CountOf(Versions); i++)
	{
		if (Versions[i].Ver - v < 0.001)
			return Versions[i].Year;
	}
	
	return 0;
}

class App : public LWindow
{
	LTextLog *Log;

public:
    App()
    {
        Name(AppName);
        LRect r(0, 0, 500, 300);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			AddView(Log = new LTextLog(100));
			AttachChildren();
			SetPulse(500);
        }
    }

	void OnPulse()
	{
		Visible(true);
	}
    
	void OnReceiveFiles(LArray<const char*> &Files)
	{
		Log->Print("Got %i files...\n", Files.Length());
		for (unsigned i=0; i<Files.Length(); i++)
		{
			auto File = Files[i];
			Log->Print("\t%s...\n", File);
	
			auto Ext = LGetExtension(File);
			if (Ext && !_stricmp(Ext, "sln"))
			{
				LFile f;
				if (f.Open(File, O_READ))
				{
					auto a = f.Read().Split("\n");
					f.Close();
					
					double FileVersion = 0.0;
					int64 VsYear = -1;
					double VsVersion = 0.0;
					for (unsigned i=0; i<a.Length() && VsVersion == 0.0; i++)
					{
						if (a[i].Find("File, Format Version") >= 0)
						{
							auto p = a[i].Split(" ");
							FileVersion = p.Last().Float();
						}
						else if (a[i](0) == '#')
						{
							LString::Array p = a[i].Split(" ");
							for (auto s : p)
							{
								auto i = s.Int();
								if (i > 2000 && i < 2030)
								{
									VsYear = i;
									VsVersion = YearToVer(VsYear);
									break;
								}
								else if (i >= 10 && i <= 23)
								{
									VsVersion = (double)i;
									VsYear = VerToYear(VsVersion);
								}
							}
						}
					}
					
					if (VsVersion > 0.0)
					{
						auto Info = GetVsInfo(VsYear);
						LFile::Path p(LSP_USER_APPS, Info && Info->x64 ? 64 : 32);
						LString v;

						if (VsVersion >= 16.0)
							v.Printf("Microsoft Visual Studio\\%" PRIi64 "\\Community", VsYear);
						else
							v.Printf("Microsoft Visual Studio %.1f", VsVersion);

						p += v;
						p += "Common7\\IDE\\devenv.exe";
						if (p.IsFile())
						{
							LExecute(p, File);
							LCloseApp();
						}
						else Log->Print("\t\tError: The file '%s' doesn't exist.", p.GetFull().Get());
					}
					else Log->Print("\t\tError: No version in '%s'.", File);
				}
				else Log->Print("\t\tError: Failed to open '%s'\n", File);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

