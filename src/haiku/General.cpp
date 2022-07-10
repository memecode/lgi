// Linux Implementation of General LGI functions
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#define _POSIX_TIMERS
#include <time.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/Token.h"

#include <errno.h>
#include <pwd.h>
#include <Alert.h>

#define DEBUG_GET_APPS_FOR_MIMETYPE			0

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
{
	if (Path)
	{
		if (LFileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (LFileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (LResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (LFileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

LString LCurrentUserName()
{
	struct passwd *pw = getpwuid(geteuid());
	if (pw)
		return pw->pw_name;

  	return "";
}


void LSleep(uint32_t i)
{
	struct timespec request, remain;

	ZeroObj(request);
	ZeroObj(remain);
	
	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	//printf("%i LSleep(%i)\n", LGetCurrentThread(), i);
	while (nanosleep(&request, &remain) == -1)
	{
	    request = remain;
		//printf("\t%i Resleeping=%i\n", LGetCurrentThread(), request.tv_sec*1000 + request.tv_nsec/1000);
	}
}

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;

		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		LString msg;
		msg.Printf("Assert failed: %s\n%s:%i", test, file, line);
		BAlert *alert = new BAlert("Assert", msg, "Abort", "Debug", "Ignore");
		auto result = alert->Go();
		printf("alert result=%i\n", result);
		switch (result)
		{
			case 0: // Abort
			{
				exit(-1);
				break;
			}
			case 1: // Debug
			{
				int *i = 0;
				*i = 0;
				break;
			}
			// default: Ignore..
		}

		Asserting = false;
	}
}

////////////////////////////////////////////////////////////////////////
// Implementations
LMessage CreateMsg(int m, int a, int b)
{
	return LMessage(m, a, b);
}

bool LGetMimeTypeExtensions(const char *Mime, LArray<LString> &Ext)
{
	int Start = Ext.Length();
	char *e;

	#define HardCodeExtention(Mime, Ext1, Ext2) \
		else if (!stricmp(Mime, Mime)) \
		{	if (Ext1) Ext.Add(Ext1); \
			if (Ext2) Ext.Add(Ext2); }

	const char *Null = NULL;
	if (!Mime);
	HardCodeExtention("text/calendar", "ics", Null)
	HardCodeExtention("text/x-vcard", "vcf", Null)
	HardCodeExtention("text/mbox", "mbx", "mbox");

	return Ext.Length() > Start;
}

LString LGetFileMimeType(const char *File)
{
	return LAppInst->GetFileMimeType(File);
}

bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize)
{
	bool Status = false;
	
	LAssert(!"Impl me.");
	
	return Status;
}

bool LGetAppsForMimeType(const char *Mime, LArray<LAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	char Args[MAX_PATH_LEN];
	sprintf(Args, "query default %s", Mime);
	LStringPipe Output;

	LLanguage *CurLang = LGetLanguageId();	
	char LangName[64];
	sprintf_s(LangName, sizeof(LangName), "Name[%s]", CurLang ? CurLang->Id : "en");

	#if DEBUG_GET_APPS_FOR_MIMETYPE
	printf("LGetAppsForMimeType('%s', ..., %i)\nRunning 'xdg-mime %s'\n",
		Mime, Limit, Args);
	#endif

	LSubProcess p("xdg-mime", Args);
	if (p.Start())
	{
		p.Communicate(&Output);
		LAutoString o(Output.NewStr());

		#if DEBUG_GET_APPS_FOR_MIMETYPE
		printf("Output:\n%s\n", o.Get());
		#endif
		if (o)
		{
			char *e = o + strlen(o);
			while (e > o && strchr(" \t\r\n", e[-1]))
				*(--e) = 0;
			
			char p[MAX_PATH_LEN];
			if (LMakePath(p, sizeof(p), "/usr/share/applications", o))
			{
				if (LFileExists(p))
				{
					LAutoString txt(LReadTextFile(p));
					LAutoString Section;

					#if DEBUG_GET_APPS_FOR_MIMETYPE
					printf("Reading '%s', got %i bytes\n", p, strlen(txt));
					#endif

					if (txt)
					{
						LAppInfo *ai = new LAppInfo;
						Apps.Add(ai);
						
						GToken t(txt, "\n");
						for (int i=0; i<t.Length(); i++)
						{
							char *Var = t[i];

							#if DEBUG_GET_APPS_FOR_MIMETYPE
							printf("    '%s'\n", Var);
							#endif
							
							if (*Var == '[')
							{
								Var++;
								char *End = strchr(Var, ']');
								if (End)
								{
									Section.Reset(NewStr(Var, End - Var));
								}							
							}
							else if (!Section || !stricmp(Section, "Desktop Entry"))
							{
								char *Value = strchr(Var, '=');
								if (Value)
								{
									*Value++ = 0;
									if (!stricmp(Var, "Exec"))
									{
										LAutoString exe(TrimStr(Value));
										char *sp = strchr(exe, ' ');
										if (sp)
											ai->Path.Set(exe, sp-exe);
										else
											ai->Path = exe;
										
										Status = true;
									}
									else if (!stricmp(Var, "Name") ||
											!stricmp(Var, LangName))
									{
										ai->Name = Value;
									}
								}
							}
						}

						#if DEBUG_GET_APPS_FOR_MIMETYPE
						printf("    ai='%s' '%s'\n", ai->Name.Get(), ai->Path.Get());
						#endif
					}
					else LgiTrace("%s:%i - Can't read from '%s'\n", _FL, p);
				}
				else LgiTrace("%s:%i - '%s' doesn't exist.", _FL, p);
			}
			else LgiTrace("%s:%i - Failed to create path.\n", _FL);
		}
		else LgiTrace("%s:%i - No output from xdg-mime\n", _FL);
	}
	else LgiTrace("%s:%i - Failed to execute xdg-mime\n", _FL);

	return Status;
}

LString LGetAppForMimeType(const char *Mime)
{
	LString App;
	LArray<LAppInfo*> Apps;
	if (LGetAppsForMimeType(Mime, Apps, 1))
		App = Apps[0]->Path.Get();
	Apps.DeleteObjects();
	return App;
}

int LRand(int Limit)
{
	return rand() % Limit;
}

LString LErrorCodeToString(uint32_t ErrorCode)
{
	LString e = strerror(ErrorCode);
	if (!e)
		e.Printf("UnknownError(%i)", ErrorCode);
	return e;
}

bool LExecute(const char *File, const char *Args, const char *Dir, LString *Error)
{
	if (File)
	{
		bool IsUrl = false;

		char App[MAX_PATH_LEN] = "";
		if (strnicmp(File, "http://", 7) == 0 ||
			strnicmp(File, "https://", 8) == 0)
		{
			IsUrl = true;
			LGetAppForMimeType("text/html", App, sizeof(App));
		}
		else
		{
			struct stat f;
			char Path[MAX_PATH_LEN];
			
			// see if the file is executable
			bool InPath = false;
			bool Ok = stat(File, &f) == 0;
			if (Ok)
			{
				strcpy_s(Path, sizeof(Path), File);
			}
			else
			{
				// look in the path
				InPath = true;
				GToken p(getenv("PATH"), LGI_PATH_SEPARATOR);
				for (int i=0; i<p.Length() && !Ok; i++)
				{
					LMakePath(Path, sizeof(Path), p[i], File);
					Ok = stat(Path, &f) == 0;
				}
			}
			
			if (Ok)			
			{
				if (S_ISDIR(f.st_mode))
				{
					// open the directory in the current browser
					LGetAppForMimeType("inode/directory", App, sizeof(App));
				}
				else
				{
					// look up the type...
					auto Mime = LGetFileMimeType(File);
					if (Mime)
					{
						// printf("LGetFileMimeType(%s)=%s\n", File, Mime);
						
						if (stricmp(Mime, "application/x-executable") == 0 ||
							stricmp(Mime, "application/x-shellscript") == 0 ||
							stricmp(Mime, "text/x-python") == 0)
						{
							TreatAsExe:
							char f[512];
							sprintf_s(f, sizeof(f), "\"%s\" %s &", File, Args ? Args : (char*)"");
							if (Dir)
								chdir(Dir);
							return system(f) == 0;
						}
						else
						{
							// got the mime type
							if (!LGetAppForMimeType(Mime, App, sizeof(App)))
							{
								// printf("%s:%i: LExecute - LgiGetAppForMimeType failed to return the app for '%s'\n", _FL, File);
								goto TreatAsExe;
							}
						}
					}
					else
					{
						// printf("LExecute: LGetFileMimeType failed to return a mime type for '%s'\n", File);
						
						// If a file can't be typed it's most likely an executable.
						// goto TreatAsExe;
					}
				}
			}
			else if (Error)
				Error->Printf("'%s' doesn't exist.\n", File);
		}

		if (App[0])
		{
			bool FileAdded = false;
			LString AppPath;
			LString EscFile = LString::Escape(File, -1, " &");

			LString a = App;
			int Pos;
			while ((Pos = a.Find("%")) >= 0)
			{
				char *s = a.Get() + Pos;
				printf("a='%s'\n", a.Get());
				switch (s[1])
				{			
					case 'f':
					case 'F':
					{
						a = a(0,Pos) + EscFile + a(Pos+2,-1);
						FileAdded = true;
						break;
					}
					case 'u':
					case 'U':
					{
						if (IsUrl)
							a = a(0,Pos) + EscFile + a(Pos+2,-1);
						else
							a = a(0,Pos) + LString("file:") + EscFile + a(Pos+2,-1);
						FileAdded = true;
						break;
					}
					default:
					{
						// we don't understand this command
						a = a(0,Pos) + a(Pos+2,-1);
						break;
					}
				}
				printf("a='%s'\n", a.Get());
			}

			if (!FileAdded)
			{
				a += " ";
				a += EscFile;
			}

			a += " > /dev/null 2> /dev/null &";

			int e;
			if (Dir) chdir(Dir);
			printf("a=\n%s\n", a.Get());
			if (e = system(a))
			{
				if (Error)
					*Error = LErrorCodeToString(errno);
				return false;
			}

			return true;
		}
	}
	return false;
}

bool LPlaySound(const char *FileName, int ASync)
{
	LAssert(!"Impl me.");
	return false;
}
