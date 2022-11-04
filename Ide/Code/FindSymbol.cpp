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
#define DEBUG_FILE				"BayesianFilter.cpp"

int SYM_FILE_SENT = 0;


class FindSymbolDlg : public LDialog
{
	// AppWnd *App;
	LList *Lst;
	FindSymbolSystem *Sys;

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
		LString::Array *Inc;
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
			char *Ext = LGetExtension(Path);
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

			return Status;
		}
	};

	int hApp;
	int MissingFiles;
	LArray<LString::Array*> IncPaths;
	
	#if USE_HASH
	LHashTbl<ConstStrKey<char,false>, FileSyms*> Files;
	#else
	LArray<FileSyms*> Files;
	#endif	
	
	uint32_t Tasks;
	uint64 MsgTs;
	bool DoingProgress;
	
	FindSymbolSystemPriv(int appSinkHnd) :
		LEventTargetThread("FindSymbolSystemPriv"),
		hApp(appSinkHnd)	
	{
		Tasks = 0;
		MsgTs = 0;
		MissingFiles = 0;
		DoingProgress = false;
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
	
	bool AddFile(LString Path, int Platforms)
	{
		#ifdef DEBUG_FILE
		bool Debug = false;
		if ((Debug = Path.Find(DEBUG_FILE) >= 0))
			printf("%s:%i - AddFile(%s)\n", _FL, Path.Get());
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
		f->Inc = IncPaths.Length() ? IncPaths.Last() : NULL;

		#if USE_HASH
		Files.Add(Path, f);
		#else
		Files.Add(f);
		#endif
		
		// Parse for headers...
		LTextFile Tf;
		if (!Tf.Open(Path, O_READ)  ||
			Tf.GetSize() < 4)
		{
			LgiTrace("%s:%i - Error: LTextFile.Open(%s) failed.\n", _FL, Path.Get());
			return false;
		}

		LAutoString Source = Tf.Read();
		LArray<char*> Headers;
		LArray<LString> EmptyInc;
		if (BuildHeaderList(Source, Headers, f->Inc ? *f->Inc : EmptyInc, false))
		{
			for (auto h: Headers)
				AddFile(h, 0);
		}
		Headers.DeleteArrays();
		
		// Parse for symbols...
		#ifdef DEBUG_FILE
		if (Debug)
			printf("%s:%i - About to parse '%s'.\n", _FL, f->Path.Get());
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
				LAutoPtr<FindSymRequest> Req((FindSymRequest*)Msg->A());
				bool AllPlatforms = (bool)Msg->B();
				if (Req && Req->SinkHnd >= 0)
				{
					LString::Array p = Req->Str.SplitDelimit(" \t");
					if (p.Length() == 0)
						break;
					
					LArray<FindSymResult*> ClassMatches;
					LArray<FindSymResult*> HdrMatches;
					LArray<FindSymResult*> SrcMatches;

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
						if (fs)
						{
							#ifdef DEBUG_FILE
							bool Debug = false;
							Debug = fs->Path.Find(DEBUG_FILE) >= 0;
							if (Debug)
								printf("%s:%i - Searching '%s' with %i syms...\n", _FL, fs->Path.Get(), (int)fs->Defs.Length());
							#endif

							// Check platforms...
							if (!AllPlatforms &&
								(fs->Platforms & PLATFORM_CURRENT) == 0)
								continue;

							// For each symbol...
							for (unsigned i=0; i<fs->Defs.Length(); i++)
							{
								DefnInfo &Def = fs->Defs[i];
								
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
									printf("%s:%i - '%s' = %i\n", _FL, Def.Name.Get(), Match);
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
						}
					}

					ClassMatches.Sort(ScoreCmp);
					Req->Results.Add(ClassMatches);
					Req->Results.Add(HdrMatches);
					Req->Results.Add(SrcMatches);
					
					int Hnd = Req->SinkHnd;
					PostObject(Hnd, M_FIND_SYM_REQUEST, Req);
				}
				break;
			}
			case M_FIND_SYM_FILE:
			{
				LAutoPtr<FindSymbolSystem::SymFileParams> Params((FindSymbolSystem::SymFileParams*)Msg->A());
				if (Params)
				{
					LString::Array Mime = LGetFileMimeType(Params->File).Split("/");
					if (!Mime[0].Equals("image"))
					{
						if (Params->Action == FindSymbolSystem::FileAdd)
							AddFile(Params->File, Params->Platforms);
						else if (Params->Action == FindSymbolSystem::FileRemove)
							RemoveFile(Params->File);
						else if (Params->Action == FindSymbolSystem::FileReparse)
							ReparseFile(Params->File);
					}
				}

				SYM_FILE_SENT--;
				break;
			}
			case M_FIND_SYM_INC_PATHS:
			{
				LAutoPtr<LString::Array> Paths((LString::Array*)Msg->A());
				if (Paths)
					IncPaths.Add(Paths.Release());
				break;
			}
			default:
			{
				LAssert(!"Implement handler for message.");
				break;
			}
		}
		
		auto Now = LCurrentTime();
		// printf("Msg->Msg()=%i " LPrintfInt64 " %i\n", Msg->Msg(), MsgTs, (int)GetQueueSize());
		if (Now - MsgTs > MSG_TIME_MS)
		{
			MsgTs = Now;
			DoingProgress = true;
			uint32_t Remaining = (uint32_t) (Tasks - GetQueueSize());
			if (Remaining > 0)
				Log("FindSym: %i of %i (%.1f%%)\n", Remaining, Tasks, (double)Remaining * 100.0 / MAX(Tasks, 1));
		}
		else if (GetQueueSize() == 0 && MsgTs)
		{
			if (DoingProgress)
			{
				Log("FindSym: Done.\n");
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
		case M_FIND_SYM_REQUEST:
		{
			LAutoPtr<FindSymRequest> Req((FindSymRequest*)m->A());
			if (Req)
			{
				LString Str = GetCtrlName(IDC_STR);
				if (Str == Req->Str)
				{
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
				}
			}
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
					// Create a search
					Sys->Search(AddDispatch(), Str, true);
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

FindSymResult FindSymbolSystem::OpenSearchDlg(LViewI *Parent)
{
	FindSymbolDlg Dlg(Parent, this);
	Dlg.DoModal();
	return Dlg.Result;	
}

bool FindSymbolSystem::SetIncludePaths(LString::Array &Paths)
{
	LString::Array *a = new LString::Array;
	if (!a)
		return false;
	*a = Paths;
	return d->PostEvent(M_FIND_SYM_INC_PATHS, (LMessage::Param)a);
}

bool FindSymbolSystem::OnFile(const char *Path, SymAction Action, int Platforms)
{
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

void FindSymbolSystem::Search(int ResultsSinkHnd, const char *SearchStr, bool AllPlat)
{
	FindSymRequest *Req = new FindSymRequest(ResultsSinkHnd);
	if (Req)
	{
		Req->Str = SearchStr;
		d->PostEvent(M_FIND_SYM_REQUEST, (LMessage::Param)Req, (LMessage::Param)AllPlat);
	}
}
