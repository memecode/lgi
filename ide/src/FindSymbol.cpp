#include <stdio.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Token.h"
#include "lgi/common/EventTargetThread.h"
#include "lgi/common/TextFile.h"
#include "lgi/common/ParseCpp.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Stat.h"

#include "LgiIde.h"
#include "FindSymbol.h"
#include "ParserCommon.h"
#include "resdefs.h"

#define MSG_TIME_MS				1000

#define DEBUG_FIND_SYMBOL		0
#define DEBUG_NO_THREAD			1
#define DEBUG_FILE				"os/interface/View.h"
#define KNOWN_EXT				"c,cpp,cxx,h,hpp,hxx,py,js"
#define INDEX_EXT				"idx"

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
	
	int OnNotify(LViewI *v, const LNotification &n) override;
	void OnCreate() override;
	bool OnViewKey(LView *v, LKey &k) override;
	LMessage::Result OnEvent(LMessage *m) override;
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
		bool IsSource = false;
		bool IsHeader = false;
		bool IsPython = false;
		bool IsJavascript = false;
		
		bool Parse(LAutoWString Source, LError &err, bool Debug)
		{
			IsSource = false;
			IsHeader = false;
			IsPython = false;
			IsJavascript = false;
			Defs.Length(0);
			
			bool Status = false;
			auto Ext = LGetExtension(Path);
			if (!Ext)
			{
				err.Set(LErrorInvalidParam, LString::Fmt("No extension for '%s'\n", Path.Get()));
				return false;
			}

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
				Status = BuildCppDefnList(Path, Source, Defs, DefnNone, err, Debug);
			else if (IsJavascript)
				Status = BuildJsDefnList(Path, Source, Defs, DefnNone, err, Debug);
			else if (IsPython)
				Status = BuildPyDefnList(Path, Source, Defs, DefnNone, err, Debug);
			else if (
				!Stricmp(Ext, "html") ||
				!Stricmp(Ext, "css") ||
				!Stricmp(Ext, "hvif"))
				return true; // These are fine... no need to print an error if they show up.
			else
				err.Set(LErrorInvalidParam, LString::Fmt("Unknown file type '%s'\n", Ext));

			return Status;
		}

		bool Serialize(LString cacheFile, bool write)
		{
			auto tab = "\t";
			LFile f(cacheFile, write ? O_WRITE : O_READ);
			if (write)
			{
				f.SetSize(0);
				for (auto &def: Defs)
				{
					auto safe = def.Name.Replace(tab, " ").Replace("\n");
					f.Print("%i%s" "%s%s" "%i%s" LPrintfSSizeT "%s" LPrintfSSizeT "\n",
						def.Type, tab,
						safe.Get(), tab,
						def.Line, tab,
						def.FnName.Start, tab,
						def.FnName.Len);
				}
			}
			else
			{
				Defs.Length(0);
				auto lines = f.Read().SplitDelimit("\r\n");
				for (auto &line: lines)
				{
					auto parts = line.SplitDelimit(tab);
					if (parts.Length() == 5)
					{
						auto &def = Defs.New();
						def.File = Path;
						def.Type = (DefnType)parts[0].Int();
						def.Name = parts[1];
						def.Line = (int) parts[2].Int();
						def.FnName.Start = parts[3].Int();
						def.FnName.Len = parts[4].Int();
					}
					else
					{
						// Regenerate corrupt file:
						f.Close();
						FileDev->Delete(cacheFile);
						return false;
					}
				}
			}

			return true;
		}
	};

	int hApp = 0;
	int MissingFiles = 0;
	LHashTbl<ConstStrKey<char, false>, LString> HdrMap;
	LHashTbl<ConstStrKey<char, false>, bool> KnownExt;
	LString::Array IncPaths, SysIncPaths;
	SystemIntf *backend = nullptr;
	LString projectCache; // lock before using?
	
	// Back end call handling:
	bool waitBackendReady = false;
	int backendCalls = 0;
	LHashTbl<ConstStrKey<char,false>,int> backendCallers;

	std::function<void()> onShutdown;
	bool IncCalls(const char *file, int line)
	{
		if (onShutdown)
			return false;
		
		backendCalls++;
		auto ref = LString::Fmt("%s:%i", file, line);
		int calls = backendCallers.Find(ref);
		backendCallers.Add(ref, calls + 1);
		// LgiTrace("inc backendCalls=%i %s:%i\n", backendCalls, file, line);
		
		return true;
	}
	void DecCalls(const char *file, int line)
	{
		//LAssert(backendCalls > 0);
		if (backendCalls > 0)
			backendCalls--;
		else
			LgiTrace("%s:%i - backendCalls mismatched.\n", _FL);
		
		auto ref = LString::Fmt("%s:%i", file, line);
		int calls = backendCallers.Find(ref);
		if (calls > 1)
			backendCallers.Add(ref, calls - 1);
		else if (calls == 1)
			backendCallers.Delete(ref);
		else
			LgiTrace("dec backendCalls=%i %s:%i\n", backendCalls, file, line);
		
		if (!backendCalls && onShutdown)
		{
			onShutdown();
			onShutdown = nullptr;
		}
	}
	
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
		for (auto &e: LString(KNOWN_EXT).SplitDelimit(","))
			KnownExt.Add(e, true);
	}

	~FindSymbolSystemPriv()
	{
		// End the thread...
		EndThread();

		// Clean up mem
		Files.DeleteObjects();
	}

	void LogBuild(const char *Fmt, ...)
	{
		va_list Arg;
		va_start(Arg, Fmt);
		LString s;
		s.Printf(Arg, Fmt);
		va_end(Arg);
		if (s.Length())
			PostThreadEvent(hApp, M_APPEND_TEXT, (LMessage::Param)NewStr(s), AppWnd::BuildTab);
	}

	void LogNetwork(const char *Fmt, ...)
	{
		va_list Arg;
		va_start(Arg, Fmt);
		LString s;
		s.Printf(Arg, Fmt);
		va_end(Arg);
		if (s.Length())
			PostThreadEvent(hApp, M_APPEND_TEXT, (LMessage::Param)NewStr(s), AppWnd::NetworkTab);
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

	LString CachePath(LString path)
	{
		Auto lck(this, _FL);
		// LAssert(InThread());
		
		if (!projectCache)
			return LString();

		LString p = path;

		if (backend)
		{
			if (auto rel = backend->MakeRelative(p))
			{
				if (rel.Find("./") == 0)
					p = rel(2, -1);
				else
					p = rel;
			}
		}

		auto leaf = LUri::EncodeStr(p, "/~") + "." + INDEX_EXT;
		LFile::Path out(projectCache);
		return (out / leaf).GetFull();
	}

	void AddFile(LString Path, int Platforms, bool cacheDirty = false)
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
			auto f = Files.Find(Path);
			if (f)
			{
				if (Platforms && f->Platforms == 0)
					f->Platforms = Platforms;
				return;
			}
		#else
			int Idx = GetFileIndex(Path);

			if (Idx >= 0)
			{
				if (Platforms && Files[Idx]->Platforms == 0)
					Files[Idx]->Platforms = Platforms;
				return;
			}

			FileSyms *f;
		#endif

		PROF("exists");
		if (!backend)
		{
			if (!LFileExists(Path))
			{
				LogBuild("Missing '%s'\n", Path.Get());
				MissingFiles++;
				return;
			}

			LAssert(!LIsRelativePath(Path));
		}

		// Setup the file sym data...
		f = new FileSyms;
		if (!f)
			return;	
		f->Path = Path;
		f->Platforms = Platforms;
		f->HdrMap = &HdrMap;

		#if USE_HASH
		Files.Add(Path, f);
		#else
		Files.Add(f);
		#endif
		
		PROF("open");

		if (backend)
		{
			auto cacheFile = CachePath(Path);
			int line;
			if (!Debug && !cacheDirty && LFileExists(cacheFile))
			{
				// Load from the cache...
				f->Serialize(cacheFile, false);
			}
			else if (IncCalls(__FILE__, line=__LINE__))
			{
				backend->Read(SystemIntf::TBackground, Path, [this, f, Debug, line](auto data, auto err) mutable
				{
					if (err)
						LogBuild("Backend.Read.Err: %s\n", err.ToString().Get());
					else
						AddFileData(f, data, Debug);

					DecCalls(__FILE__, line);
				});
			}
		}
		else
		{
			// Parse for headers...
			LTextFile Tf;
			if (!Tf.Open(Path, O_READ)  ||
				Tf.GetSize() < 4)
			{
				// LgiTrace("%s:%i - Error: LTextFile.Open(%s) failed.\n", _FL, Path.Get());
				return;
			}

			PROF("build hdr");

			if (auto Source = Tf.Read())
				AddFileData(f, Source, Debug);
		}
	}

	void AddFileData(FileSyms *f, const char *data, bool Debug = false)
	{
		LString::Array Headers;
		auto *Map = &HdrMap;
		if (BuildHeaderList(data,
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

		// Log("AddFileData %s\n", f->Path.Get());
		
		// Parse for symbols...
		PROF("parse");
		#ifdef DEBUG_FILE
		if (Debug)
			LgiTrace("%s:%i - About to parse '%s'.\n", _FL, f->Path.Get());
		#endif
		
		LAutoWString w(Utf8ToWide(data));
		LError err;
		if (f->Parse(w, err, Debug))
		{
			if (backend)
			{
				if (auto cacheFile = CachePath(f->Path))
				{
					// Write the cache file..
					f->Serialize(cacheFile, true);
				}
			}
		}
		else
		{
			LogBuild("AddFileData.error: Parse(%s) failed: %s\n", f->Path.Get(), err.ToString().Get());
		}
	}
	
	void ReparseFile(LString Path)
	{
		#if USE_HASH
			FileSyms *f = Files.Find(Path);
			int Platform = f ? f->Platforms : 0;
		#else
			int Idx = GetFileIndex(Path);
			int Platform = Idx >= 0 ? Files[Idx]->Platforms : 0;
		#endif
		
		if (!RemoveFile(Path))
			return;

		AddFile(Path, Platform);
	}

	void AddDirectoryEntry(LDirectory &dir, int Platforms)
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

	void AddPaths(LString::Array &out, LString::Array &in, int Platforms, bool recurse = false)
	{
		LHashTbl<StrKey<char>, bool> map; // of existing scanned folders
		for (auto p: out)
			map.Add(p, true);

		LString::Array subFolders;
		for (auto p: in)
		{
			if (!map.Find(p))
			{
				// Log("add path: %s %i\n", p.Get(), recurse);
				out.Add(p);
				map.Add(p, true);

				if (backend)
				{
					backend->ReadFolder(SystemIntf::TBackground, p, [this, Platforms, recurse, out = &out](auto dir, auto err)
						{
							LString::Array subFolders;
							for (int b = true; b; b = dir->Next())
							{
								if (recurse && dir->IsDir())
									subFolders.Add(dir->FullPath());
								else
									AddDirectoryEntry(*dir, Platforms);
							}
							if (subFolders.Length())
								AddPaths(*out, subFolders, Platforms, recurse);
						});
				}
				else // local folder:
				{
					LDirectory dir;
					for (auto b=dir.First(p); b; b=dir.Next())
					{
						if (recurse && dir.IsDir())
							subFolders.Add(dir.FullPath());
						else
							AddDirectoryEntry(dir, Platforms);
					}
				}
			}
		}

		if (subFolders.Length())
			AddPaths(out, in, Platforms, recurse);
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

	bool gettingCacheFolder = false;

	void OnPulse()
	{
		if (onShutdown)
		{
			LogNetwork("Shutdown callers active: %i\n", backendCalls);
			for (auto p: backendCallers)
				LogNetwork("	%s = %i\n", p.key, p.value);

			if (backend->IsFinished())
			{
				SetPulse();
				LogNetwork("Backend is finished\n");
				if (onShutdown)
				{
					onShutdown();
					onShutdown = nullptr;
				}
			}
		}
		else if (waitBackendReady &&
				backend &&
				backend->IsReady())
		{
			Auto lck(this, _FL);

			if (!projectCache)
			{
				if (!gettingCacheFolder)
				{
					PostThreadEvent(hApp, M_GET_PROJECT_CACHE);
					gettingCacheFolder = true;
				}
			}
			else
			{
				SetPulse();
				waitBackendReady = false;
				PostEvent(M_SCAN_FOLDER, (LMessage::Param)new LString(backend->GetBasePath()));
			}
		}
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		if (IsCancelled())
			return -1;
		
		switch (Msg->Msg())
		{
			case M_FIND_SYM_BACKEND:
			{
				if ((backend = (SystemIntf*)Msg->A()))
				{
					waitBackendReady = true;
					SetPulse(500);
				}
				break;
			}
			case M_GET_PROJECT_CACHE:
			{
				Auto lck(this, _FL);
				if (auto folder = Msg->AutoA<LString>())
					projectCache = *folder;
				break;
			}
			case M_CLEAR_PROJECT_CACHE:
			{
				Auto lck(this, _FL);
				if (projectCache)
				{
					LDirectory dir;
					int cleared = 0, errors = 0;
					for (auto i=dir.First(projectCache); i; dir.Next())
					{
						if (dir.IsDir())
							continue;

						auto ext = LGetExtension(dir.GetName());
						if (!Stricmp(ext, INDEX_EXT))
						{
							if (FileDev->Delete(dir.FullPath()))
								cleared++;
							else
								errors++;
						}
					}
					LogBuild("FindSymbolSystemPriv.clearCache: cleared %i files (%i errors).\n", cleared, errors);
				}
				else LogBuild("FindSymbolSystemPriv.error: no project cache dir set.\n");
				break;
			}
			case M_SCAN_FOLDER:
			{
				if (auto path = Msg->AutoA<LString>())
				{
					int line;
					if (IncCalls(__FILE__, line=__LINE__))
					{
						backend->ReadFolder(
							SystemIntf::TBackground,
							*path,
							[this, path = LString(*path), line](auto dir, auto err)
							{
								if (err || !dir)
								{
									LogBuild("ReadFolder failed: %s\n", err.ToString().Get());
								}
								else
								{
									for (int i=true; i; i=dir->Next())
									{
										auto nm = dir->GetName();
										auto full = dir->FullPath();
										if (dir->IsDir())
										{
											if (!stricmp(nm, ".") ||
												!stricmp(nm, "..") ||
												!stricmp(nm, ".hg") ||
												!stricmp(nm, ".git") ||
												!stricmp(nm, ".svn"))
												continue;
									
											PostEvent(M_SCAN_FOLDER, (LMessage::Param)new LString(full));
										}
										else if (dir->GetSize() > 0)
										{
											auto ext = LGetExtension(nm);
											if (!ext)
												continue;
											if (KnownExt.Find(ext))
											{
												// Check the date against the cache?
												auto cache = CachePath(full);
												bool cacheDirty = false;
												LStat st(cache);
												if (st)
												{
													auto writeTm = dir->GetLastWriteTime();
													auto remoteModified = dir->TsToUnix(writeTm);
													auto cacheModified = st.GetLastWriteTime();
													if (remoteModified > cacheModified)
													{
														cacheDirty = true;
													}
												}

												if (auto add = new FindSymbolSystem::SymFileParams)
												{
													add->Action = FindSymbolSystem::FileAdd;
													add->File = full;
													add->Platforms = 0;
													add->cacheDirty = cacheDirty;

													PostEvent(M_FIND_SYM_FILE, (LMessage::Param)add);
												}
											}
										}
									}
								}

								DecCalls(__FILE__, line);
							});
					}
				}
				break;
			}
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
					if (!backend && 
						fs->Platforms != 0 &&
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
				if (seconds > 0.1)
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

				bool isImage = false;
				if (!backend)
				{
					auto MimeType = LGetFileMimeType(Params->File);
					LString::Array Parts = MimeType.Split("/");
					isImage = Parts[0].Equals("image");
				}
				if (!isImage)
				{
					if (Params->Action == FindSymbolSystem::FileAdd)
						AddFile(Params->File, Params->Platforms, Params->cacheDirty);
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
				AddPaths(SysIncPaths, Params->SysPaths, Params->Platforms, true);
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
				LogBuild("FindSym: %i of %i (%.1f%%)\n", Remaining, Tasks, (double)Remaining * 100.0 / MAX(Tasks, 1));
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
				LogBuild("(%i files are missing)\n", MissingFiles);
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

int FindSymbolDlg::OnNotify(LViewI *v, const LNotification &n)
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

void FindSymbolSystem::Shutdown(std::function<void()> callback)
{
	if (!callback)
	{
		LAssert(!"No callback");
		return;
	}
		
	if (d->backendCalls)
	{
		d->onShutdown = callback;
		if (d->backend)
			d->backend->Cancel();
		d->SetPulse(1000);
	}
	else
	{
		callback();
	}
}

void FindSymbolSystem::OpenSearchDlg(LViewI *Parent, std::function<void(FindSymResult&)> Callback)
{
	FindSymbolDlg *Dlg = new FindSymbolDlg(Parent, this);
	Dlg->DoModal([Dlg, Callback](auto d, auto code)
	{
		if (Callback)
			Callback(Dlg->Result);
	});
}

bool FindSymbolSystem::PostEvent(int Cmd, LMessage::Param a, LMessage::Param b, int64_t TimeoutMs)
{
	return d->PostEvent(Cmd, a, b, TimeoutMs);
}

void FindSymbolSystem::ClearCache()
{
	d->PostEvent(M_CLEAR_PROJECT_CACHE);
}

void FindSymbolSystem::SetBackend(SystemIntf *backend)
{
	d->PostEvent(M_FIND_SYM_BACKEND, (LMessage::Param)backend);
}

int FindSymbolSystem::GetAppHnd()
{
	return d->hApp;
}

bool FindSymbolSystem::SetIncludePaths(LString::Array &Paths, LString::Array &SysPaths, int Platforms)
{
	#if 0
	for (auto p: SysPaths)
		LgiTrace("SysPath:%s\n", p.Get());
	if (SysPaths.Length() == 0)
		LgiTrace("NoSysPath\n");
	#endif

	if (auto params = new FindSymbolSystem::SymPathParams)
	{
		params->Paths = Paths;
		params->SysPaths = SysPaths;
		params->Platforms = Platforms;
		return d->PostEvent(M_FIND_SYM_INC_PATHS,
							(LMessage::Param)params);
	}

	return false;
}

bool FindSymbolSystem::OnFile(const char *Path, SymAction Action, int Platforms)
{
	if (!Path)
		return false;
	if (Platforms == 0)
	{
		printf("%s:%i - Can't add '%s' with no platforms.\n", _FL, Path);
		return false;
	}

	if (Stristr(Path, "/fileteepee"))
	{
		int asd=0;
	}

	if (auto ext = LGetExtension(Path))
	{
		if (!d->KnownExt.Find(ext))
			return false;
	}
	else return false;

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
