/** \file
	\author Matthew Allen
 */

#if defined(_MSC_VER) && defined(_DEBUG)
#include "crtdbg.h"
#endif

#include "lgi/common/Lgi.h"
#ifdef LGI_SDL
#include <SDL.h>
#endif

/** \brief The main entry point of a Lgi program

	To hide the differences between the different OS's standard entry points for programs
	the file LgiMain.cpp implements a simple platform specific entry point and then calls
	LgiMain() to pass execution onto your program.

	A simple LgiMain() looks like this:
	\code
	int LgiMain(OsAppArguments &Args)
	{
		LApp *App = new LApp("application/x-MyProgram", Args);
		if (App && App->IsOk())
		{
			App->AppWnd = new MyWindow;
			App->Run();
			delete App;
		}

		return 0;
	}
	\endcode
*/
extern int LgiMain
(
	/// The arguments passed in from the OS.
	OsAppArguments &AppArgs
);

bool _BuildCheck()
{
	#ifdef _DEBUG
	const char *AppBuild = "Debug";
	#else
	const char *AppBuild = "Release";
	#endif
	char *LgiBuild = (char*) (LIsReleaseBuild() ? "Release" : "Debug");

	if (_stricmp(AppBuild, LgiBuild))
	{
		LgiTrace("Build check failed, app=%s lgi=%s\n", AppBuild, LgiBuild);
		return LgiMsg(	NULL,
						"This application and it's libraries are mismatched:\n"
						"\n"
						"   Application:\t%s\n"
						"   Lgi:\t\t%s\n"
						"\n"
						"If you continue you may encounter crashes and other undesirable problems.\n"
						"Do you want to continue anyway?",
						"Warning",
						MB_YESNO,
						AppBuild,
						LgiBuild) == IDYES;
	}

	return true;
}

#ifdef WIN32

int
#if _CONSOLE
main(int args, char *arg[])
#else
WINAPI CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
	/*
	#if defined(_MSC_VER) && defined(_DEBUG)
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	// _CrtSetBreakAlloc(34058);
	#endif
	*/
	#ifdef __GTK_H__
	{
		#ifdef _MSC_VER
		// Really? REALLY? Get your shit together GTK.
		_putenv("GDK_WIN32_USE_EXPERIMENTAL_OLE2_DND=1");
		#endif

		#if _CONSOLE
		gtk_init(&args, &arg);
		#else
		gtk_init(&__argc, &__argv);
		#endif
	}
    #endif

	int Status = 0;
	#if !_CONSOLE && WINNATIVE
	_lgi_app_instance = hInstance;
	#endif
	if (_BuildCheck())
	{
		char16 *CL = (char16*)GetCommandLineW();

    	#if WINNATIVE
    	
			OsAppArguments AppArgs;
			#if !_CONSOLE
			AppArgs.hInstance = hInstance;
			AppArgs.nCmdShow = nCmdShow;
			#endif

			if (*CL == '\"')
			{
				AppArgs.lpCmdLine = StrchrW(CL+1, '\"');
			}
			else
			{
				AppArgs.lpCmdLine = StrchrW(CL+1, ' ');
			}
			if (AppArgs.lpCmdLine) AppArgs.lpCmdLine++;

		#else
			LString::Array Mem;
			LArray<char*> Args;
			char16 *Ws = L" \t\r\n";
			for (char16 *c = CL; *c; )
			{
			    while (*c && StrchrW(Ws, *c))
			    	c++;
			    if (!*c)
			    {
			    	printf("Breaking on NULL at %i\n", (int)(c-CL));
			    	break;
			    }
			    if (*c == '\"' || *c == '\'')
			    {
			        char16 delim = *c++;
			        char16 *end = StrchrW(c, delim);
			        if (end)
			        {
						LString s(c, end-c);
						Mem.New() = s;
			            Args.Add(s.Get());
			            c = end + 1;
			        }
			        else
			        {
						LString s(c);
						Mem.New() = s;
			            Args.Add(s.Get());
			            break;
			        }
			    }
			    else
			    {
			        char16 *end = c;
	    		    while (*end && !StrchrW(Ws, *end)) end++;
	    		    if (end > c)
		                Args.Add(WideToUtf8(c, end-c));
		            c = end + (*end != 0);
			    }
			}
			
			OsAppArguments AppArgs(Args.Length(), (const char**) &Args[0]);
			
		#endif
		
		Status = LgiMain(AppArgs);
	}

	return Status;
}

#else

int main(int Args, const char **Arg)
{
	int Status = 0;

	#ifdef __GTK_H__

		#if 0 && defined(_DEBUG)
		
			// This turns on fatal GKT warnings all the time... 
			// Useful for debugging.
			LArray<const char*> a;
			
			for (int i=0; i<Args; i++)
				a[i] = Arg[i];
			a.Add("--g-fatal-warnings");
			if (1)
				a.Add("--sync");
			
			Arg = a.AddressOf();
			Args = a.Length();

		#endif

		Gtk::gtk_init_check(&Args, (char***)&Arg);

	#endif

	OsAppArguments AppArgs(Args, Arg);
	if (_BuildCheck())
		Status = LgiMain(AppArgs);
	else
		LgiTrace("%s:%i - _BuildCheck failed.\n", _FL);

	return Status;
}

#endif

#if defined(LGI_MEM_DEBUG) && !defined(LGI_STATIC)

void *operator new(size_t size)
{
	return lgi_malloc(size);
}

void *operator new[](size_t size)
{
	return lgi_malloc(size);
}

void operator delete(void *p)
{
	lgi_free(p);
}

void operator delete[](void *p)
{
	lgi_free(p);
}

#endif
