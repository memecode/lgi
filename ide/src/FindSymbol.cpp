#include <stdio.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Token.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/TextFile.h"
#include "LgiIde.h"
#include "FindSymbol.h"
#include "ParserCommon.h"

#if 1
#include "lgi/common/ParseCpp.h"
#endif

#include "resdefs.h"

#define MSG_TIME_MS				1000

#define DEBUG_FIND_SYMBOL		0
#define DEBUG_NO_THREAD			1
// #define DEBUG_FILE				"PopupNotification.h"

int SYM_FILE_SENT = 0;

class FindSymbolDlg : public LDialog
{
	LList *Lst = NULL;
	FindSymbolSystem *Sys = NULL;
	int PlatformFlags = -1;

public:
	FindSymResult Result;

	FindSymbolDlg(LViewI *parent, FindSymbolSystem *sys);
	~FindSymbolDlg();
	
	int OnNotify(LViewI *v, LNotification n);
	void OnCreate();
	bool OnViewKey(LView *v, LKey &k);
	LMessage::Result OnEvent(LMessage *m);
};

int ScoreCmp(FindSymResult **a, FindSymResult **b)
{
	return (*b)->Score - (*a)->Score;
}

#define USE_HASH	1

struct FindSymbolSystemPriv : public LEventTargetThread
{
	struct FileSyms
	{
		LString Path;
		int Platforms;
		LHashTbl<ConstStrKey<char, false>, LString> *HdrMap = NULL;
		LArray<DefnInfo> Defs;
		bool IsSource;
		bool IsHeader;
		bool IsPython;
		bool IsJavascript;
		
		bool Parse(LAutoWString Source, bool Debug)
		{
			IsSource = false;
			IsHeader = false;
			IsPython = false;
			IsJavascript = false;
			Defs.Length(0);
			
			bool Status = false;
			auto Ext = LGetExtension(Path);
			if (Ext)
			{
				IsSource =	!_stricmp(Ext, "c")
							||
							!_stricmp(Ext, "cpp")
							||
							!_stricmp(Ext, "cxx");
				IsHeader =	!_stricmp(Ext, "h")
							||
							!_stricmp(Ext, "hpp")
							||
							!_stricmp(Ext, "hxx");
				IsPython =	!_stricmp(Ext, "py");
				IsJavascript = !_stricmp(Ext, "js");

				if (IsSource || IsHeader)
					Status = BuildCppDefnList(Path, Source, Defs, DefnNone, Debug);
				else if (IsJavascript)
					Status = BuildJsDefnList(Path, Source, Defs, DefnNone, Debug);
				else if (IsPython)
					Status = BuildPyDefnList(Path, Source, Defs, DefnNone, Debug);
			}
			else printf("%s:%i - No extension for '%s'\n", _FL, Path.Get());

			return Status;
		}
	};

	int hApp = 0;
	int MissingFiles = 0;
	LHashTbl<ConstStrKey<char, false>, LString> HdrMap;
	LString::Array IncPaths, SysIncPaths;
	
	#if USE_HASH
	LHashTbl<ConstStrKey<char,false>, FileSyms*> Files;
	#else
	LArray<FileSyms*> Files;
	#endif	
	
	int Tasks = 0;
	uint64 MsgTs = 0;
	bool DoingProgress = false;
	
	FindSymbolSystemPriv(int appSinkHnd) :
		LEventTargetThread("FindSymbolSystemPriv"),
		hApp(appSinkHnd)	
	{
	}

	~FindSymbolSystemPriv()
	{
		// End the thread...
		EndThread();

		// Clean up mem
		Files.DeleteObjects();
	}

	void Log(const char *Fmt, ...)
	{
		va_list Arg;
		va_start(Arg, Fmt);
		LString s;
		s.Printf(Arg, Fmt);
		va_end(Arg);
		
		if (s.Length())
			PostThreadEvent(hApp, M_APPEND_TEXT, (LMessage::Param)NewStr(s), AppWnd::BuildTab);
	}
	
	#if !USE_HASH
	int GetFileIndex(LString &Path)
	{
		for (unsigned i=0; i<Files.Length(); i++)
		{
			if (Path == Files[i]->Path)
				return i;
		}
		return -1;
	}
	#endif
	
	#define PROFILE_ADDFILE 0
	#if PROFILE_ADDFILE
	#define PROF(...) prof.Add(__VA_ARGS__)
	#else
	#define PROF(...)
	#endif

	bool AddFile(LString Path, int Platforms)
	{
		#if PROFILE_ADDFILE
		LProfile prof("AddFile");
		#endif
		bool Debug = false;
		#ifdef DEBUG_FILE
		if ((Debug = Path.Find(DEBUG_FILE) >= 0))
			LgiTrace("%s:%i - AddFile(%s, %s)\n", _FL, Path.Get(), PlatformFlagsToStr(Platforms).Get());
		#endif
		
		// Already added?
		#if USE_HASH
			FileSyms *f = Files.Find(Path);
			if (f)
			{
				if (Platforms && f->Platforms == 0)
					f->Platforms = Platforms;
				return true;
			}
		#else
			int Idx = GetFileIndex(Path);

			if (Idx >= 0)
			{
				if (Platforms && Files[Idx]->Platforms == 0)
					Files[Idx]->Platforms = Platforms;
				return true;
			}

			FileSyms *f;
		#endif

		PROF("exists");
		if (!LFileExists(Path))
		{
			Log("Missing '%s'\n", Path.Get());
			MissingFiles++;
			return false;
		}
			
		LAssert(!LIsRelativePath(Path));

		// Setup the file sym data...
		f = new FileSyms;
		if (!f) return false;	
		f->Path = Path;
		f->Platforms = Platforms;
		f->HdrMap = &HdrMap;

		#if USE_HASH
		Files.Add(Path, f);
		#else
		Files.Add(f);
		#endif
		
		PROF("open");

		// Parse for headers...
		LTextFile Tf;
		if (!Tf.Open(Path, O_READ)  ||
			Tf.GetSize() < 4)
		{
			// LgiTrace("%s:%i - Error: LTextFile.Open(%s) failed.\n", _FL, Path.Get());
			return false;
		}

		PROF("build hdr");

		LAutoString Source = Tf.Read();
		LString::Array Headers;
		auto *Map = &HdrMap;
		if (BuildHeaderList(Source,
							Headers,
							false,
							[Map](auto Name)
							{
								return Map->Find(Name);
							}))
		{
			for (auto h: Headers)
				AddFile(h, 0);
		}
		
		// Parse for symbols...
		PROF("parse");
		#ifdef DEBUG_FILE
		if (Debug)
			LgiTrace("%s:%i - About to parse '%s'.\n", _FL, f->Path.Get());
		#endif
		return f->Parse(LAutoWString(Utf8ToWide(Source)), Debug);
	}
	
	bool ReparseFile(LString Path)
	{
		#if USE_HASH
			FileSyms *f = Files.Find(Path);
			int Platform = f ? f->Platforms : 0;
		#else
			int Idx = GetFileIndex(Path);
			int Platform = Idx >= 0 ? Files[Idx]->Platforms : 0;
		#endif
		
		if (!RemoveFile(Path))
			return false;

		return AddFile(Path, Platform);
	}

	void AddPaths(LString::Array &out, LString::Array &in, int Platforms)
	{
		LHashTbl<StrKey<char>, bool> map; // of existing scanned folders
		for (auto p: out)
			map.Add(p, true);

		for (auto p: in)
		{
			if (!map.Find(p))
			{
				// LgiTrace("add path: %s\n", p.Get());
				out.Add(p);
				map.Add(p, true);

				LDirectory dir;
				for (auto b=dir.First(p); b; b=dir.Next())
				{
					bool willAdd = !dir.IsDir() &&
									MatchStr("*.h*", dir.GetName());
						
					#ifdef DEBUG_FILE
					if (!Stricmp(dir.GetName(), DEBUG_FILE))
						LgiTrace("!!!GOT %s willAdd=%i !!!\n", dir.FullPath(), willAdd);
					#endif
					
					if (willAdd)
					{
						HdrMap.Add(dir.GetName(), dir.FullPath());
						AddFile(dir.FullPath(), Platforms);
					}
				}
			}
		}
	}
	
	bool RemoveFile(LString Path)
	{
		#if USE_HASH
			FileSyms *f = Files.Find(Path);
			if (!f) return false;
			Files.Delete(Path);
			delete f;
		#else
			int Idx = GetFileIndex(Path);
			if (Idx < 0) return false;
			delete Files[Idx];
			Files.DeleteAt(Idx);
		#endif

		return true;
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		if (IsCancelled())
			return -1;
		
		switch (Msg->Msg())
		{
			case M_FIND_SYM_REQUEST:
			{
				auto Req = Msg->AutoA<FindSymRequest>();
				auto Platforms = Msg->B();
				if (!Req || Req->SinkHnd < 0)
					break;

				LString::Array p = Req->Str.SplitDelimit(" \t");
				if (p.Length() == 0)
					break;
					
				LArray<FindSymResult*> ClassMatches;
				LArray<FindSymResult*> HdrMatches;
				LArray<FindSymResult*> SrcMatches;
				size_t recordsSearched = 0;
				auto startTs = LCurrentTime();

				// For each file...
				#if USE_HASH
				for (auto it : Files)
				{
					FileSyms *fs = it.value;
				#else
				for (unsigned f=0; f<Files.Length(); f++)
				{
					FileSyms *fs = Files[f];
				#endif
					if (!fs)
						continue;

					#ifdef DEBUG_FILE
						bool Debug = false;
						Debug = fs->Path.Find(DEBUG_FILE) >= 0;
						if (Debug)
							LgiTrace("%s:%i - Searching '%s' with %i syms...\n", _FL, fs->Path.Get(), (int)fs->Defs.Length());
					#endif

					// Check platforms...
					if (fs->Platforms != 0 &&
						(fs->Platforms & Platforms) == 0)
					{
						#ifdef DEBUG_FILE
							if (fs->Path.Find(DEBUG_FILE) >= 0)
								LgiTrace("%s:%i - '%s' doesn't match platform: %s %s\n",
										_FL,
										fs->Path.Get(),
										PlatformFlagsToStr(fs->Platforms).Get(),
										PlatformFlagsToStr(Platforms).Get());
						#endif
						continue;
					}

					// For each symbol...
					for (unsigned i=0; i<fs->Defs.Length(); i++)
					{
						DefnInfo &Def = fs->Defs[i];
								
						#ifdef DEBUG_FILE
						if (Debug)
							LgiTrace("%s:%i - '%s'\n", _FL, Def.Name.Get());
						#endif
								
						// For each search term...
						bool Match = true;
						int ScoreSum = 0;
						for (unsigned n=0; n<p.Length(); n++)
						{
							const char *Part = p[n];
							bool Not = *Part == '-';
							if (Not)
								Part++;

							int Score = Def.Find(Part);
							if
							(
								(Not && Score != 0)
								||
								(!Not && Score == 0)
							)
							{
								Match = false;
								break;
							}

							ScoreSum += Score;
						}

						#ifdef DEBUG_FILE
						if (Debug)
							LgiTrace("	'%s' = %i\n", _FL, Def.Name.Get(), Match);
						#endif
								
						if (Match)
						{
							// Create a result for this match...
							FindSymResult *r = new FindSymResult();
							if (r)
							{
								r->Score = ScoreSum;
								r->File = Def.File.Get();
								r->Symbol = Def.Name.Get();
								r->Line = Def.Line;

								if (Def.Type == DefnClass)
									ClassMatches.Add(r);
								else if (fs->IsHeader)
									HdrMatches.Add(r);
								else
									SrcMatches.Add(r);
							}
						}
					}

					recordsSearched += fs->Defs.Length();
				}

				double seconds = (double)(LCurrentTime() - startTs) / 1000.0;
				LgiTrace("%s:%i M_FIND_SYM_REQUEST searched " LPrintfSizeT " symbols in %gs\n",
					_FL, recordsSearched, seconds);

				ClassMatches.Sort(ScoreCmp);
				Req->Results.Add(ClassMatches);
				Req->Results.Add(HdrMatches);
				Req->Results.Add(SrcMatches);
					
				int Hnd = Req->SinkHnd;
				PostObject(Hnd, M_FIND_SYM_REQUEST, Req);
				break;
			}
			case M_FIND_SYM_FILE:
			{
				auto Params = Msg->AutoA<FindSymbolSystem::SymFileParams>();
				if (!Params)
					break;

				auto MimeType = LGetFileMimeType(Params->File);
				LString::Array Parts = MimeType.Split("/");
				if (!Parts[0].Equals("image"))
				{
					if (Params->Action == FindSymbolSystem::FileAdd)
						AddFile(Params->File, Params->Platforms);
					else if (Params->Action == FindSymbolSystem::FileRemove)
						RemoveFile(Params->File);
					else if (Params->Action == FindSymbolSystem::FileReparse)
						ReparseFile(Params->File);
				}

				SYM_FILE_SENT--;
				break;
			}
			case M_FIND_SYM_INC_PATHS:
			{
				auto Params = Msg->AutoA<FindSymbolSystem::SymPathParams>();
				if (!Params)
					break;
				AddPaths(IncPaths, Params->Paths, Params->Platforms);
				AddPaths(SysIncPaths, Params->SysPaths, Params->Platforms);
				break;
			}
			default:
			{
				LAssert(!"Implement handler for message.");
				break;
			}
		}
		
		auto Now = LCurrentTime();
		// LgiTrace("Msg->Msg()=%i " LPrintfInt64 " %i\n", Msg->Msg(), MsgTs, (int)GetQueueSize());
		if (Now - MsgTs > MSG_TIME_MS)
		{
			MsgTs = Now;
			DoingProgress = true;
			int Remaining = (int)(Tasks - GetQueueSize());
			if (Remaining > 0)
				Log("FindSym: %i of %i (%.1f%%)\n", Remaining, Tasks, (double)Remaining * 100.0 / MAX(Tasks, 1));
		}
		else if (GetQueueSize() == 0 && MsgTs)
		{
			if (DoingProgress)
			{
				// Log("FindSym: Done.\n");
				DoingProgress = false;
			}
			if (MissingFiles > 0)
			{
				Log("(%i files are missing)\n", MissingFiles);
			}
			
			MsgTs = 0;
			Tasks = 0;
		}

		return 0;
	}	
};

int AlphaCmp(LListItem *a, LListItem *b, NativeInt d)
{
	return stricmp(a->GetText(0), b->GetText(0));
}

FindSymbolDlg::FindSymbolDlg(LViewI *parent, FindSymbolSystem *sys)
{
	Lst = NULL;
	Sys = sys;
	SetParent(parent);
	if (LoadFromResource(IDD_FIND_SYMBOL))
	{
		MoveToCenter();

		LViewI *f;
		if (GetViewById(IDC_STR, f))
			f->Focus(true);
		GetViewById(IDC_RESULTS, Lst);
	}
}

FindSymbolDlg::~FindSymbolDlg()
{
}

void FindSymbolDlg::OnCreate()
{
	RegisterHook(this, LKeyEvents);
}

bool FindSymbolDlg::OnViewKey(LView *v, LKey &k)
{
	switch (k.vkey)
	{
		case LK_UP:
		case LK_DOWN:
		case LK_PAGEDOWN:
		case LK_PAGEUP:
		{
			return Lst->OnKey(k);
			break;
		}
	}
	
	return false;
}

LMessage::Result FindSymbolDlg::OnEvent(LMessage *m)
{
	switch (m->Msg())
	{
		case M_GET_PLATFORM_FLAGS:
		{
			// Ok the app replied with the current platform flags:
			PlatformFlags = m->B();

			// Now continue the original search:
			Sys->Search(AddDispatch(), GetCtrlName(IDC_STR), PlatformFlags);
			break;
		}
		case M_FIND_SYM_REQUEST:
		{
			LAutoPtr<FindSymRequest> Req((FindSymRequest*)m->A());
			if (!Req)
				break;

			LString Str = GetCtrlName(IDC_STR);
			if (Str != Req->Str)
				break;

			Lst->Empty();
			List<LListItem> Ls;

			LString s;
			int CommonPathLen = 0;					
			for (unsigned i=0; i<Req->Results.Length(); i++)
			{
				FindSymResult *r = Req->Results[i];

				if (i)
				{
					char *a = s.Get();
					char *a_end = strrchr(a, DIR_CHAR);

					char *b = r->File.Get();
					char *b_end = strrchr(b, DIR_CHAR);

					int Common = 0;
					while (	*a && a <= a_end
							&& 
							*b && b <= b_end
							&&
							ToLower(*a) == ToLower(*b))
					{
						Common++;
						a++;
						b++;
					}
					if (i == 1)
						CommonPathLen = Common;
					else
						CommonPathLen = MIN(CommonPathLen, Common);
				}
				else s = r->File;
			}

			for (unsigned i=0; i<Req->Results.Length(); i++)
			{
				FindSymResult *r = Req->Results[i];
				LListItem *it = new LListItem;
				if (it)
				{
					LString Ln;
					Ln.Printf("%i", r->Line);
							
					it->SetText(r->File.Get() + CommonPathLen, 0);
					it->SetText(Ln, 1);
					it->SetText(r->Symbol, 2);
					Ls.Insert(it);
				}
			}

			Lst->Insert(Ls);
			Lst->ResizeColumnsToContent();
			break;
		}
	}

	return LDialog::OnEvent(m);
}

int FindSymbolDlg::OnNotify(LViewI *v, LNotification n)
{
	switch (v->GetId())
	{
		case IDC_STR:
		{
			if (n.Type != LNotifyReturnKey)
			{
				auto Str = v->Name();
				if (Str && strlen(Str) > 2)
				{
					if (PlatformFlags < 0)
					{
						// We don't know the currently selected platform flags yet, so ask the app for that:
						PostThreadEvent(Sys->GetAppHnd(), M_GET_PLATFORM_FLAGS, (LMessage::Param)AddDispatch());
					}
					else
					{
						// Create a search
						Sys->Search(AddDispatch(), Str, PlatformFlags);
					}
				}
			}
			break;
		}
		case IDC_RESULTS:
		{
			if (n.Type == LNotifyItemDoubleClick)
			{
				// Fall throu
			}
			else break;
		}
		case IDOK:
		{
			if (Lst)
			{
				LListItem *i = Lst->GetSelected();
				if (i)
				{
					Result.File = i->GetText(0);
					Result.Line = atoi(i->GetText(1));
				}
			}
			
			EndModal(true);
			break;
		}
		case IDCANCEL:
		{
			EndModal(false);
			break;
		}
	}
	
	return LDialog::OnNotify(v, n);
}

///////////////////////////////////////////////////////////////////////////
FindSymbolSystem::FindSymbolSystem(int AppHnd)
{
	d = new FindSymbolSystemPriv(AppHnd);
}

FindSymbolSystem::~FindSymbolSystem()
{
	delete d;
}

void FindSymbolSystem::OpenSearchDlg(LViewI *Parent, std::function<void(FindSymResult&)> Callback)
{
	FindSymbolDlg *Dlg = new FindSymbolDlg(Parent, this);
	Dlg->DoModal([Dlg, Callback](auto d, auto code)
	{
		if (Callback)
			Callback(Dlg->Result);
		delete Dlg;
	});
}

int FindSymbolSystem::GetAppHnd()
{
	return d->hApp;
}

bool FindSymbolSystem::SetIncludePaths(LString::Array &Paths, LString::Array &SysPaths, int Platforms)
{
	#if 0
	for (auto p: SysPaths)
		printf("SysPath:%s\n", p.Get());
	if (SysPaths.Length() == 0)
		printf("NoSysPath\n");
	#endif

	auto *params = new FindSymbolSystem::SymPathParams;
	params->Paths = Paths;
	params->SysPaths = SysPaths;
	params->Platforms = Platforms;
	return d->PostEvent(M_FIND_SYM_INC_PATHS,
						(LMessage::Param)params);
}

bool FindSymbolSystem::OnFile(const char *Path, SymAction Action, int Platforms)
{
	LAssert(Platforms != 0);

	if (d->Tasks == 0)
		d->MsgTs = LCurrentTime();
	d->Tasks++;
	
	LAutoPtr<FindSymbolSystem::SymFileParams> Params(new FindSymbolSystem::SymFileParams);
	Params->File = Path;
	Params->Action = Action;
	Params->Platforms = Platforms;

	if (d->PostObject(d->GetHandle(), M_FIND_SYM_FILE, Params))
	{
		SYM_FILE_SENT++;
	}
	return false;
}

void FindSymbolSystem::Search(int ResultsSinkHnd, const char *SearchStr, int Platforms)
{
	LAssert(Platforms != 0);
	
	FindSymRequest *Req = new FindSymRequest(ResultsSinkHnd);
	if (Req)
	{
		Req->Str = SearchStr;
		d->PostEvent(M_FIND_SYM_REQUEST, (LMessage::Param)Req, (LMessage::Param)Platforms);
	}
}
