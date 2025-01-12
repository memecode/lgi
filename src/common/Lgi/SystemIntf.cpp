#include "lgi/common/Lgi.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Thread.h"
#include "lgi/common/Ssh.h"
#include "lgi/common/Mutex.h"
#include "lgi/common/RemoveAnsi.h"
#include "lgi/common/SystemIntf.h"

#ifndef HAS_LIBSSH
#error "Add ssh to this project"
#endif

#define INFO(...) { if (log) log->Print(__VA_ARGS__); }

const char *PlatformNames[] =
{
	"Windows",
	"Linux",
	"Mac",
	"Haiku",
	0
};

extern const char *ToString(SysPlatform p)
{
	if (p < PlatformWin || p >= PlatformMax)
		return NULL;
	return PlatformNames[p];
}

#if 1
#define LOG(...) log->Print(__VA_ARGS__)
#else
#define LOG(...)
#endif

// General SystemIntf work handling:
void SystemIntf::AddWork(LString ctx, TPriority priority, TCallback &&job)
{
	Auto lck(this, _FL);
	auto w = new TWork(ctx, std::move(job));
	if (priority == SystemIntf::TForeground)
		foregroundWork.Add(w);
	else
		backgroundWork.Add(w);
}

void SystemIntf::DoWork()
{
	// Is there work to do?
	auto now = LCurrentTime();
	LAutoPtr<TWork> curWork;
	{
		Auto lck(this, _FL);
		if (timedWork.Length())
		{
			// Is there any timed work to do?
			for (auto w: timedWork)
			{
				if (w->ts <= now)
				{
					// Yes!
					curWork.Reset(w);
					timedWork.PopFirst();
					break;
				}
			}
		}
		if (!curWork)
		{
			// Check the regular queues:
			if (foregroundWork.Length())
				curWork.Reset(foregroundWork.PopFirst());
			else if (backgroundWork.Length())
				curWork.Reset(backgroundWork.PopFirst());
		}
	}

	if (curWork)
	{
		curWork->fp();

		auto now = LCurrentTime();
		if (now - lastLogTs >= 500)
		{
			lastLogTs = now;
			log->Print("foreground %i, background %i\n", (int)foregroundWork.Length(), (int)backgroundWork.Length());
		}
	}
	else LSleep(WAIT_MS);
}

class SshBackend :
	public SystemIntf,
	public LCancel,
	public LThread
{
	class Process :
		public LThread,
		public LSsh
	{
		SshBackend *backend;
		LString dir, cmd;
		LStream *log = NULL;
		ProcessIo *out = NULL;
		LCancel *cancel = NULL;
		std::function<void(int)> exitcodeCb;
		int32_t exitCode = -1;

	public:
		Process(SshBackend *be,
				LString initDir,
				LString cmdLine,
				ProcessIo *output,
				LStream *logger,
				LCancel *cancelObj,
				std::function<void(int)> cb) :
			LThread("Ssh.Process.Thread"),
			LSsh([this](auto Msg, auto Type)
				{
					return SshConnect;
				},
				logger,
				cancelObj),
			backend(be),
			dir(initDir),
			cmd(cmdLine),
			log(logger),
			out(output),
			cancel(cancelObj),
			exitcodeCb(cb)
		{
			LOG("Process: about to run..\n");
			Run();
		}

		~Process()
		{
			WaitForExit();
			if (exitcodeCb)
				exitcodeCb(exitCode);
		}

		int Main()
		{
			LOG("Process: in main..\n");
			SetTimeout(5000);

			LOG("Process: open..\n");
			if (!Open(backend->uri.sHost, backend->uri.sUser, NULL, true))
			{
				log->Print("Error: connecting to ssh server %s\n", backend->uri.sHost.Get());
				return -1;
			}
				
			LOG("Process: create console..\n");
			if (auto console = CreateConsole())
			{
				LOG("Process: ReadToPrompt..\n");
				backend->ReadToPrompt(console, NULL, cancel);
				auto args = LString::Fmt("cd %s && %s\n", dir.Get(), cmd.Get());				

				LStream *output = NULL;
				if (out)
				{
					if (out->ioCallback)
						out->ioCallback(console);
					else
						output = out->output;
				}

				if (out && out->ioCallback)
				{
					LOG("Process: write args..\n");
					console->Write(args);

					// Wait for process to finish...
					// Client code needs to call CallMethod(ObjCancel...) to quit this loop
					while (!console->IsCancelled())
					{
						LSleep(100);
					}

					out->ioCallback(NULL);
				}
				else
				{
					LOG("Process: Cmd..\n");
					auto result = backend->Cmd(console, args, &exitCode, output, cancel);
					log->Print("SshProcess finished with %i\n", exitCode);
				}
			}
			else
			{
				log->Print("Error: creating ssh console\n");
				return -1;
			}
			
			backend->OnProcessComplete();
			return 0;
		}
	};

	LView *app = NULL;
	LUri uri;
	LStream *log = NULL;
	SysPlatform sysType = PlatformUnknown;
	LString remoteSep;
	LString homePath;
	LArray<Process*> processes;
	constexpr static const char *separators = "/\\";

	// Lock before use
	using TWork = std::function<void()>;
	LAutoPtr<LSsh> ssh;
	LAutoPtr<LSsh::SshConsole> console;
	LArray<TWork> foregroundWork, backgroundWork;

	void OnProcessComplete()
	{
		AddWork(MakeContext(_FL, "OnProcessComplete"),
			SystemIntf::TForeground,
			[this]()
			{
				// Clean up all exited processes:
				for (unsigned i=0; i<processes.Length(); i++)
				{
					auto p = processes[i];
					if (p->IsExited())
					{
						processes.DeleteAt(i--);
						delete p;
					}
				}
			});
	}

	LSsh::SshConsole *GetConsole()
	{
		if (!ssh)
		{
			if (ssh.Reset(new LSsh( [this](auto msg, auto hostType) -> LSsh::CallbackResponse
				{
					return LSsh::SshConnect;
				},
				log,
				this)))
			{
				ssh->SetTimeout(5000);
				INFO("%s: Connecting to '%s'..\n", GetClass(), uri.sHost.Get())
				
				if (ssh->Open(uri.sHost, uri.sUser, NULL, true))
				{
					INFO("%s: Connected to '%s'\n", GetClass(), uri.sHost.Get())
					OnConnected();
				}
				else
				{
					INFO("%s: Error connecting to '%s'\n", GetClass(), uri.sHost.Get())
					return NULL;
				}
			}
		}
		if (ssh && !console)
		{
			if ((console = ssh->CreateConsole()))
			{
				LgiTrace("Created console... reading to prompt:\n");
				ReadToPrompt(GetConsole());
				LgiTrace("got login prompt\n");
			}
		}
		return console;
	}

	LString ReadToPrompt(LSsh::SshConsole *c, LStream *output = NULL, LCancel *cancel = NULL)
	{
		auto s = ssh->ReadToPrompt(c, output, cancel);
		RemoveAnsi(s);
		return s;
	}

	LString Cmd(LSsh::SshConsole *c, LString cmd, int32_t *exitCode = NULL, LStream *outputStream = NULL, LCancel *cancel = NULL)
	{
		if (c)
		{
			// log->Print("Cmd: write '%s'\n", cmd.Strip().Get());
			if (!c->Write(cmd))
			{
				if (exitCode)
					*exitCode = -1;
				return LString();
			}

			auto output = ReadToPrompt(c, outputStream, cancel);
			
			if (cancel && cancel->IsCancelled())
			{
				if (exitCode)
					*exitCode = -1;
			}
			else if (exitCode)
			{
				LString echo = "echo $?\n";
				if (c->Write(echo))
				{
					auto val = ReadToPrompt(c);
					// log->Print("echo output: %s", val.Get());

					auto lines = val.SplitDelimit("\r\n");
					if (lines.Length() > 1)
						*exitCode = (int32_t) lines[1].Int();
					else
						*exitCode = -1;

					if (*exitCode)
						log->Print("Cmd failed: %s\n", output.Get());
				}
			}

			return output;
		}

		return LString();
	}

public:
	SshBackend(LView *parent, LString u, LStream *logger) :
		SystemIntf(logger, "SshBackend"),
		LThread("SshBackend.Thread"),
		app(parent),
		uri(u),
		log(logger)
	{
		Run();
	}

	~SshBackend()
	{
		Cancel();
		{
			Auto lck(this, _FL);
			for (auto p: processes)
				delete p;
		}
		WaitForExit();
	}

	const char *GetClass() const { return "SshBackend"; }
	LString GetBasePath() override { return RemoteRoot(); }
	bool IsReady() override
	{
		return remoteSep && ssh->GetPrompt() && homePath;
	}

	LString MakeNative(LString path)
	{
		LAssert(remoteSep);

		if (remoteSep(0) == '/')
			return path.Replace("\\", remoteSep);
		else
			return path.Replace("/", remoteSep);
	}

	LString MakeRelative(LString absPath) override
	{
		absPath = MakeNative(absPath);

		// Strip off the boot part...
		auto boot = "/boot";
		if (absPath.Find(boot) == 0)
			absPath = absPath.Replace(boot);
			
		// Is it in the home folder?
		auto home = "/home";
		if (absPath.Find(home) == 0)
			absPath = absPath.Replace(home, "~");

		// Convert to relative from the base folder:
		auto base = RemoteRoot();
		if (absPath.Find(base) == 0)
			// Seems to based off the same root, so remove that...
			return LString(".") + absPath(base.Length(), -1);

		return LString();
	}

	LString MakeAbsolute(LString relPath) override
	{
		if (relPath(0) == remoteSep(0))
		{
			LAssert(!"this doesn't look like a relative path bro?");
		}

		auto native = MakeNative(relPath);
		if (native(0) == '~')
		{
			if (homePath)
				native = native.Replace("~", homePath);
			else
				LAssert(!"no home path available?");
		}
		return native;
	}

	LString JoinPath(LString base, LString leaf)
	{
		LString::Array a;
		a.SetFixedLength(false);
		a += base.SplitDelimit("\\/");
		a += leaf.SplitDelimit("\\/");
		for (size_t i = 0; i < a.Length(); i++)
		{
			if (a[i] == ".")
				a.DeleteAt(i--, true);
			else if (a[i] == "..")
			{
				a.DeleteAt(i--, true);
				a.DeleteAt(i--, true);
			}
		}
		return remoteSep.Join(a);
	}

	LString::Array ParentsOf(LString path)
	{
		LString::Array p;
		auto parts = path.SplitDelimit(remoteSep);
		for (size_t i = parts.Length()-1; i > 0; i--)
			p.Add( remoteSep.Join(parts.Slice(0, i)) );
		return p;
	}

	void ResolvePath(LString path, LString::Array hints, std::function<void(LError&,LString)> cb)
	{
		if (!cb)
			return;

		AddWork(
			MakeContext(_FL, path),
			TForeground,
			[this, path = PreparePath(path), hints, cb]() mutable
			{
				LString found;
				if (path(0) == remoteSep(0))
				{
					// Absolute?
					auto abs = PreparePath(path);
					auto p = LString::Fmt("stat %s\n", abs.Get());
					int32_t code;
					Cmd(GetConsole(), p, &code);
					if (code == 0)
						found = abs;
				}
				else if (path(0) == '~')
				{
					// Uses home path notation?
					if (homePath)
					{
						path = path.Replace("~", homePath);
						auto p = LString::Fmt("stat %s\n", path.Get());
						int32_t code;
						Cmd(GetConsole(), p, &code);
						if (code == 0)
							found = path;
					}
					else LAssert(!"no home folder?");
				}
				else
				{
					// Relative path?
					for (auto hint: hints)
					{
						auto rel = JoinPath(hint, path);
						auto p = LString::Fmt("stat %s\n", rel.Get());
						int32_t code;
						Cmd(GetConsole(), p, &code);
						if (code == 0)
							found = rel;
					}

					if (!found)
					{
						// Maybe it's relative to one of the parent folders in the hints?
						for (auto hint: hints)
						{
							auto parents = ParentsOf(hint);
							for (auto parent: parents)
							{
								auto rel = JoinPath(parent, path);
								auto p = LString::Fmt("stat %s\n", rel.Get());
								int32_t code;
								Cmd(GetConsole(), p, &code);
								if (code == 0)
								{
									found = rel;
									break;
								}
							}
						}
					}
				}

				app->RunCallback( [this, cb, found]() mutable
					{
						LError err(found ? LErrorPathNotFound : LErrorNone, found ? nullptr : "path not found");
						cb(err, found);
					});
			}
		);
	}

	void OnConnected()
	{
		GetSysType(
			[this](auto sys)
			{
				ProcessOutput("echo $HOME",
					[this](auto exitCode, auto str)
					{
						if (!exitCode)
						{
							RemoveAnsi(str);
							homePath = str;
						}
					});
			});
	}

	void GetSysType(std::function<void(SysPlatform)> cb) override
	{
		if (sysType != PlatformUnknown)
		{
			if (cb)
				cb(sysType);
			return;
		}

		AddWork(
			MakeContext(_FL, "GetSysType"),
			SystemIntf::TForeground,
			[this, cb]()
			{
				if (auto output = Cmd(GetConsole(), "uname -a\n"))
				{
					auto parts = TrimContent(output).SplitDelimit();
					auto sys = parts[0].Lower();
					if (sys.Find("haiku") >= 0)
						sysType = PlatformHaiku;
					else if (sys.Find("linux") >= 0)
						sysType = PlatformLinux;
					else if (sys.Find("darwin") >= 0)
						sysType = PlatformMac;
					else if (sys.Find("windows") >= 0)
						sysType = PlatformWin;
					else
						LAssert(!"unknown system type?");

					log->Print("System is: %s\n", sys.Get());
					remoteSep = sysType == PlatformWin ? "\\" : "/";

					if (cb)
					{
						app->RunCallback( [this, cb]() mutable
							{
								cb(sysType);
							});
					}
				}
			}
		);
	}

	class SshDir : public LDirectory
	{
		struct TEntry
		{
			LString perms, user, grp, name, linkTarget;
			LString month, day, timeOrYear;
			int64_t size;

			bool IsDir()      const { return perms(0) == 'd'; }
			bool IsSymLink()  const { return perms(0) == 'l'; }
			bool IsReadOnly() const { return perms(8) != 'w'; }
			bool IsHidden()   const { return name(0)  == '.'; }
		};
		LArray<TEntry> entries;
		ssize_t pos = 0;
		LString BasePath;
		LString Cache;
		LStream *Log = NULL;

		bool Ok() const { return pos < (ssize_t)entries.Length(); }

	public:
		SshDir(const char *basePath, LArray<LString> &lines, LStream *log) :
			Log(log)
		{
			BasePath = basePath;

			//0.........10........20........30........40........50........60
			//drwxrwxr-x   2 matthew matthew    4096 Mar 12 12:46  .redhat
			LArray<bool> non;
			for (auto &line: lines)
				for (int i=0; i<line.Length(); i++)
					if (!IsWhite(line[i]))
						non[i] = true;
			LArray<LRange> cols;
			for (int i=0; i<non.Length();)
			{
				// Skip whitespace
				while (i<non.Length() && !non[i]) i++;
				auto start = i;
				while (i<non.Length() && non[i]) i++;
				cols.New().Set(start, i - start);
			}
			for (auto &line: lines)
			{
				if (Log)
				{
					if (line.Find("13462126") > 0)
					{
						int asd=0;
					}
					Log->Print("Line: %s\n", line.Get());
				}

				#define COL(idx) line(cols[idx].Start, cols[idx].End())
				auto &e = entries.New();
				e.perms = COL(0);
				e.user  = COL(2);
				e.grp   = COL(3);
				auto sz = COL(4);
				e.size = sz.Strip().Int();
				e.month = COL(5);
				e.day   = COL(6).Strip();
				e.timeOrYear = COL(7);
				if ((e.name = line(cols[7].End() + 1, -1).LStrip(" ")))
				{
					if (e.name(0) == '\'')
						e.name = e.name.Strip("\'");

					if (e.name.Equals(".") ||
						e.name.Equals(".."))
					{
						entries.PopLast();
						continue;
					}
					
					if (e.IsSymLink())
					{
						const char *key = " -> ";
						auto pos = e.name.Find(key);
						if (pos > 0)
						{
							auto parts = e.name.Split(key);
							if (parts.Length() == 2)
							{
								e.name = parts[0];
								e.linkTarget = parts[1];
							}
							else LAssert(!"wrong parts count?");
						}
					}
				}
			}
		}

		~SshDir()
		{
			int asd=0;
		}

		int First(const char *Name, const char *Pattern = LGI_ALL_FILES) { pos = 0; return Ok(); }	
		int Next() { pos++; return Ok(); }
		int Close() { pos = 0; return true; }
		bool Path(char *s, int BufSize)	const
		{
			if (!s || !Ok()) return false;
			strcpy_s(s, BufSize, entries.ItemAt(pos).name);
			return true;
		}
		const char *FullPath()
		{
			if (!Ok()) return NULL;
			Cache = BasePath + "/" + entries[pos].name;
			return Cache;
		}
		long GetAttributes() const { return 0; }
		const char *GetName() const
		{
			if (Ok())
				return entries.ItemAt(pos).name;
			return NULL;
		}		
		LString FileName() const
		{
			if (Ok())
				return entries.ItemAt(pos).name;
			return LString();
		}
		int GetUser(bool Group) const
		{
			if (Ok())
				return (int) Atoi(entries.ItemAt(pos).user.Get());
			return 0;
		}
		uint64 GetLastWriteTime() const
		{
			if (!Ok())
				return 0;
			
			auto &e = entries.ItemAt(pos);
			
			LDateTime dt;
			dt.SetNow();

			auto month = LDateTime::IsMonth(e.month);
			if (month < 0)
				return 0;

			dt.Month(month + 1);
			dt.Day((int) e.day.Int());
			if (e.timeOrYear.Find(":") > 0)
				dt.SetTime(e.timeOrYear);
			else
				dt.Year((int) e.timeOrYear.Int());

			return dt.Ts().Get();
		}
		uint64 GetCreationTime() const { return 0; }
		uint64 GetLastAccessTime() const { return 0; }
		uint64 GetSize() const
		{
			if (Ok())
				return entries.ItemAt(pos).size;
			return 0;
		}
		int64 GetSizeOnDisk()
		{
			if (Ok())
				return entries.ItemAt(pos).size;
			return 0;
		}
		bool IsDir() const
		{
			if (Ok())
				return entries.ItemAt(pos).IsDir();
			return false;
		}	
		bool IsSymLink() const
		{
			if (Ok())
				return entries.ItemAt(pos).IsSymLink();
			return false;
		}	
		bool IsReadOnly() const
		{
			if (Ok())
				return entries.ItemAt(pos).IsReadOnly();
			return true;
		}
		bool IsHidden() const
		{
			if (Ok())
				return entries.ItemAt(pos).IsHidden();
			return false;
		}

		LDirectory *Clone() { return NULL; }	
		LVolumeTypes GetType() const { return VT_FOLDER; }
	};

	LString RemoteRoot(const char *subFolder = NULL)
	{
		LString sep("/");
		auto pathParts = uri.sPath.SplitDelimit(separators);
		pathParts.SetFixedLength(false);
		pathParts.AddAt(0, "~");

		if (subFolder)
			pathParts += LString(subFolder).SplitDelimit(separators);
		for (int i=0; i<pathParts.Length(); i++)
		{
			if (pathParts[i] == ".")
			{
				pathParts.DeleteAt(i--, true);
			}
			else if (pathParts[i] == "..")
			{
				pathParts.DeleteAt(i--, true);
				pathParts.DeleteAt(i--, true);
			}
			// printf("cur=%s\n", sep.Join(pathParts).Get());
		}
		
		LString prefix = pathParts[0] == "~" ? LString() : sep;
		auto finalPath = prefix + sep.Join(pathParts);
		printf("RemoteRoot=%s\n", finalPath.Get());
		return finalPath;
	}

	bool Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) override
	{
		if (!path || !cb)
			return false;
		
		AddWork(
			MakeContext(_FL, path),
			SystemIntf::TForeground,
			[this, cb, path = PreparePath(path)]()
			{
				auto cmd = LString::Fmt("stat %s\n", path.Get());
				int32_t exitCode = 0;
				auto out = Cmd(GetConsole(), cmd, &exitCode);
				auto lines = out.SplitDelimit("\r\n").Slice(1, -2);

				app->RunCallback(
					[cb, exitCode, lines]() mutable
					{
						LError err;
						if (exitCode)
						{
							err.Set(LErrorPathNotFound, lines[0]);
							cb(nullptr, LString(), err);
						}
						else
						{
							struct stat s = {};
							LDateTime dt;
							LString file;
							for (auto ln: lines)
							{
								auto p = ln.Strip().SplitDelimit(":", 1);
								if (p.Length() != 2) continue;
								auto var = p[0].Strip();
								auto val = p[1].Strip();
								if (var.Equals("file"))
								{
									file = val;
								}
								else if (var.Equals("size"))
								{
									auto parts = val.SplitDelimit();
									s.st_size = (TOffset) parts[0].Int();
								}
								else if (var.Equals("access"))
								{
									auto parts = val.SplitDelimit("(/) ");
									if (parts[0].Length() == 4 && parts[0](0) == '0')
									{
										// access mode...
										s.st_mode = (unsigned short) Atoi(parts[0].Get(), 8);
									}
									else
									{
										// access time...
										if (dt.Set(val))
											s.st_atime = dt.GetUnix();
									}
								}
								else if (var.Equals("modify"))
								{
									if (dt.Set(val))
										s.st_mtime = dt.GetUnix();
								}
								else if (var.Equals("change"))
								{
									if (dt.Set(val))
										s.st_ctime = dt.GetUnix();
								}
							}
							cb(&s, file, err);
						}
					});
			}
		);
		return true;
	}

	bool ReadFolder(TPriority priority, const char *Path, std::function<void(LDirectory*)> cb) override
	{
		if (!Path || !cb)
			return false;

		AddWork(
			MakeContext(_FL, Path),
			priority,
			[this, cb, path = PreparePath(Path)]()
			{
				auto cmd = LString::Fmt("ls -lan %s\n", path.Get());
				auto ls = Cmd(GetConsole(), cmd);
				auto lines = ls.SplitDelimit("\r\n").Slice(2, -2);

				app->RunCallback( [dir = new SshDir(path, lines, NULL), cb]() mutable
					{
						cb(dir);
						delete dir;
					});
			} );
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) override
	{
		if (!searchTerms || !results)
			return false;

		AddWork(
			MakeContext(_FL, searchTerms),
			SystemIntf::TForeground,
			[this, results, searchTerms = LString(searchTerms)]()
			{
				auto root = RemoteRoot();
				auto parts = searchTerms.SplitDelimit();
				auto args = LString::Fmt("cd %s && find .", root.Get());
				for (size_t i=0; i<parts.Length(); i++)
					args += LString::Fmt("%s -iname \"*%s*\"", i ? " -and" : "", LGetLeaf(parts[i]));
				args += " -and -not -path \"*/.hg/*\"";

				int32_t exitCode = 0;
				auto result = Cmd(GetConsole(), args + "\n", &exitCode);
				if (!exitCode)
				{
					LArray<LString> lines = TrimContent(result).SplitDelimit("\r\n");
					app->RunCallback( [results, lines]() mutable
						{
							results(lines);
						});
				}
			} );

		return true;
	}

	bool FindInFiles(FindParams *params, LStream *results) override
	{
		if (!params || !results)
			return false;

		AddWork(
			MakeContext(_FL, "FindInFiles"),
			SystemIntf::TForeground,
			[this, results, params = new FindParams(*params)]()
			{
				auto args = LString::Fmt("grep -Rn \"%s\" %s",
					params->Text.Get(),
					params->Dir.Get());
				results->Print("%s\n", args.Get());
				int32_t exitCode = 0;
				auto result = Cmd(GetConsole(), args + "\n", &exitCode);
				if (!exitCode)
				{
					auto output = TrimContent(result);
					results->Write(output);
				}
			} );

		return true;
	}

	LString TrimContent(LString in)
	{
		auto first = in.Find("\n");
		auto last = in.RFind("\n");
		LString out;
		if (first >= 0 &&
			last >= 0)
		{
			out = in(first + 1, last).Replace("\r");
		}
		return out;
	}

	LString PreparePath(LString s)
	{
		LString cp = s.Get();;

		if (remoteSep)
		{
			auto sep = remoteSep(0);
			for (char *c = cp.Get(); *c; c++)
				if (Strchr(separators, *c))
					*c = sep;
		}

		return cp.Replace(" ", "\\ ");
	}

	bool Read(TPriority priority, const char *Path, std::function<void(LError,LString)> result) override
	{
		if (!Path)
			return false;

		AddWork(
			MakeContext(_FL, Path),
			priority,
			[this, result, Path = PreparePath(Path)]() mutable
			{
				LStringPipe buf;

				if (Path.Find("~") == 0 && homePath)
					Path = Path.Replace("~", homePath);

				auto err = ssh->DownloadFile(&buf, Path);
				if (result)
				{
					app->RunCallback( [this, err, result, data=buf.NewLStr()]() mutable
						{
							result(err, data);
						});
				}
			} );

		return true;
	}

	bool Write(TPriority priority, const char *Path, LString Data, std::function<void(LError)> result) override
	{
		if (!Path)
			return false;
		if (!Data)
		{
			LAssert(!"should be some data?");
			return false;
		}

		AddWork(
			MakeContext(_FL, Path),
			priority,
			[this, result, Path = PreparePath(Path), Data]() mutable
			{
				LMemStream stream(Data.Get(), Data.Length(), false);

				if (Path.Find("~") == 0 && homePath)
					Path = Path.Replace("~", homePath);

				auto err = ssh->UploadFile(Path, &stream);
				if (result)
				{
					app->RunCallback( [this, err, result]() mutable
						{
							result(err);
						});
				}
			} );

		return true;
	}

	bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) override
	{
		if (!path || !cb)
			return false;

		AddWork(
			MakeContext(_FL, path),
			SystemIntf::TForeground,
			[this, cb, createParents, Path = PreparePath(path)]()
			{
				auto args = LString::Fmt("mkdir%s %s", createParents ? " -p" : "", Path.Get());
				int32_t exitVal;
				auto result = Cmd(GetConsole(), args + "\n", &exitVal);
				if (cb)
				{
					app->RunCallback( [exitVal, cb]() mutable
						{
							cb(exitVal == 0);
						});
				}
			} );

		return true;
	}

	bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) override
	{
		if (!path || !cb)
			return false;

		AddWork(
			MakeContext(_FL, path),
			TForeground,
			[this, cb, recursiveForce, Path = PreparePath(path)]()
			{
				auto args = LString::Fmt("rm%s %s", recursiveForce ? " -rf" : "", Path.Get());
				int32_t exitVal;
				auto result = Cmd(GetConsole(), args + "\n", &exitVal);
				if (cb)
				{
					app->RunCallback( [exitVal, cb]() mutable
						{
							cb(exitVal == 0);
						});
				}
			} );

		return true;
	};

	bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) override
	{
		if (!oldPath || !newPath)
			return false;

		AddWork(
			MakeContext(_FL, newPath),
			TForeground,
			[this, cb, from = PreparePath(oldPath), to = PreparePath(newPath)]()
			{
				auto args = LString::Fmt("mv %s %s", from.Get(), to.Get());
				int32_t exitVal;
				auto result = Cmd(GetConsole(), args + "\n", &exitVal);
				if (cb)
				{
					app->RunCallback( [exitVal, cb]() mutable
						{
							cb(exitVal == 0);
						});
				}
			} );

		return true;
	}

	bool RunProcess(const char *initDir, const char *cmdLine, ProcessIo *output, LCancel *cancel, std::function<void(int)> cb, LStream *alt_log)
	{
		if (!cmdLine)
			return false;

		Auto lck(this, _FL);
		// Use a long running thread / separate SSH console to stop this main connection from responding quickly.
		processes.Add(new Process(this, initDir, cmdLine, output, alt_log ? alt_log : log, cancel, cb));
		return true;
	}

	bool ProcessOutput(const char *cmdLine, std::function<void(int32_t,LString)> cb) override
	{
		if (!cmdLine || !cb)
			return false;

		AddWork(
			MakeContext(_FL, cmdLine),
			TBackground,
			[this, cb, cmd=LString::Fmt("%s\n", cmdLine)]()
			{
				int32_t exitVal;
				LStringPipe out;
				auto result = Cmd(GetConsole(), cmd, &exitVal, &out);
				if (cb)
				{
					app->RunCallback( [exitVal, cb, out=TrimContent(out.NewLStr())]() mutable
						{
							cb(exitVal, out);
						});
				}
			} );

		return true;
	}

	int Main() override
	{
		while (!IsCancelled())
			DoWork();

		return 0;
	}
};

class LocalBackend : public SystemIntf
{
	LView *app;
	LString folder;
	LStream *log = NULL;

public:
	LocalBackend(LView *parent, LString uri, LStream *logger) :
		SystemIntf(logger, "LocalBackend"),
		app(parent),
		log(logger)
	{
		LUri u(uri);
		folder = u.LocalPath();
	}

	~LocalBackend()
	{
	}

	LString GetBasePath() override { return folder; }

	LString MakeRelative(LString absPath)
	{
		return LMakeRelativePath(folder, absPath);
	}

	LString MakeAbsolute(LString relPath) override
	{
		LAssert(!"impl me");
		return relPath;
	}

	LString JoinPath(LString base, LString leaf)
	{
		LFile::Path p(base);
		p += leaf;
		return p.GetFull();
	}

	void ResolvePath(LString path, LString::Array hints, std::function<void(LError&,LString)> cb)
	{
		LAssert(!"impl me");
	}

	bool Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) override
	{
		if (!path || !cb)
			return false;
		struct stat s;
		LError err;
		if (stat(path, &s))
			err = errno;
		cb(&s, path, err);
		return true;
	}

	bool ReadFolder(TPriority priority, const char *Path, std::function<void(LDirectory*)> results) override
	{
		LDirectory dir;
		if (!dir.First(Path))
			return false;

		results(&dir);
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) override
	{
		// This could probably be threaded....
		auto parts = LString(searchTerms).SplitDelimit();
		LArray<char*> files;
		if (!LRecursiveFileSearch(folder, NULL, &files))
			return false;

		LString::Array matches;
		for (auto f: files)
		{
			bool matched = true;
			for (auto p: parts)
			{
				if (!Stristr(f, p.Get()))
				{
					matched = false;
					break;
				}
			}
			if (matched)
				matches.Add(f);
		}

		files.DeleteArrays();
		results(matches);
		return true;
	}

	bool FindInFiles(FindParams *params, LStream *results) override
	{
		LAssert(!"impl me");
		return false;
	}

	bool Read(TPriority priority, const char *Path, std::function<void(LError,LString)> result) override
	{
		if (!Path)
		{
			if (result)
				result(LErrorInvalidParam, LString());
			return false;
		}

		LFile f(Path, O_READ);
		LError err;
		LString data;
		if (f)
			data = f.Read();
		else
			err = f.GetError();
		if (result)
			result(err, data);
		return true;
	}

	bool Write(TPriority priority, const char *Path, LString Data, std::function<void(LError)> result) override
	{
		if (!Path)
		{
			if (result)
				result(LErrorInvalidParam);
			return false;
		}
		LFile out(Path, O_WRITE);
		if (!out)
		{
			if (result)
				result(out.GetError());
			return false;
		}
		out.SetSize(0);
		auto status = out.Write(Data);
		if (result)
			result(status ? LErrorNone : LErrorIoFailed);
		return status;
	}

	bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) override
	{
		LError err(LErrorNone);
		auto result = FileDev->CreateFolder(path, false, &err);
		if (cb)
			cb(!err);
		return result;
	}

	bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) override
	{
		if (!path)
			return false;
		if (LDirExists(path))
		{
			auto status = FileDev->RemoveFolder(path, recursiveForce);
			if (cb)
				cb(status);
		}
		else
		{
			auto status = FileDev->Delete(path);
			if (cb)
				cb(status);
		}
		return true;
	}

	bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) override
	{
		if (!oldPath || !newPath)
			return false;

		auto status = FileDev->Move(oldPath, newPath);
		if (cb)
			cb(status);

		return status;
	}

	void GetSysType(std::function<void(SysPlatform)> cb) override
	{
		if (!cb)
			return;
		#if defined(WINDOWS)
			cb(PlatformWin);
		#elif defined(MAC)
			cb(PlatformMac);
		#elif defined(LINUX)
			cb(PlatformLinux);
		#elif defined(HAIKU)
			cb(PlatformHaiku);
		#else
			#error "No platform defined"
			cb(PlatformCurrent);
		#endif
	}

	bool RunProcess(const char *initDir, const char *cmdLine, ProcessIo *output, LCancel *cancel, std::function<void(int)> cb, LStream *alt_log)
	{
		return false;
	}

	bool ProcessOutput(const char *cmdLine, std::function<void(int32_t,LString)> cb) override
	{
		return false;
	}
};

#include "lgi/common/Ftp.h"
#include "lgi/common/OpenSSLSocket.h"

class FtpBackend :
	public SystemIntf,
	public LCancel,
	public LThread,
	public IFtpCallback
{
	LView *parent = nullptr;
	LUri uri;
	LStream *log = nullptr;
	bool Connected = false;
	LString remote;

	// Lock before using:
	struct TCache
	{
		LString data;
		IFtpEntry entry;
	};
	LAutoPtr<IFtp> ftp;
	LHashTbl<StrKey<char>, TCache*> cache;

	bool GetReady(int timeout = 6000)
	{
		auto start = LCurrentTime();
		while (!IsCancelled() && !Connected)
		{
			auto now = LCurrentTime();
			if (now - start > timeout)
				return false;
			LSleep(10);
		}

		return true;
	}

	class FtpDir : public LArray<IFtpEntry*>, public LDirectory
	{
		ssize_t pos = 0;
		LString base;
		LString curEntry;
		LString sep = "/";

		bool Ok() const
		{
			return IdxCheck(pos);
		}

	public:
		FtpDir(LString path)
		{
			if (path.Find("directory is ") >= 0)
			{
				int asd=0;
			}

			base = path;
		}

		~FtpDir()
		{
			DeleteObjects();
		}

		bool Valid()
		{
			/*
			if (base.Str && (base.Str->Len > 1000 || base.Str->Refs > 100 || base.Str->Refs < 0))
				return false;
			*/
			return true;
		}

		IFtpEntry *Find(const char *name)
		{
			for (auto e: *this)
			{
				if (e->Name.Equals(name))
					return e;
			}
			return nullptr;
		}

		int First(const char *Name, const char *Pattern) override
		{
			pos = 0;
			return Ok();
		}
	
		int Next() override
		{
			pos++;
			return Ok();
		}

		int Close() override
		{
			pos = -1;
			return true;
		}

		bool Path(char *s, int BufSize)	const override
		{
			if (!base)
				return false;

			auto e = ItemAt(pos);
			if (!e)
				return false;

			auto ok = LMakePath(s, BufSize, base, e->Name);
			if (ok)
			{
				#ifdef WINDOWS
				for (char *c = s; *c; c++)
					if (*c == '/')
						*c = '\\';
				#endif
			}
			return ok;
		}

		const char *FullPath() override
		{
			if (!Ok())
				return nullptr;

			auto e = ItemAt(pos);
			LString::Array a;
			a.SetFixedLength(false);
			a.Add(base);
			a.Add(e->Name);
			curEntry = sep.Join(a);

			return curEntry;
		}

		long GetAttributes() const override
		{
			LAssert(0);
			return 0;
		}
	
		const char *GetName() const override
		{
			if (!Ok())
				return nullptr;

			auto e = ItemAt(pos);
			return e->Name;
		}

		LString FileName() const override
		{
			if (!Ok())
				return false;

			auto e = ItemAt(pos);
			return e->Name;
		}
	
		int GetUser(bool Group) const override
		{
			LAssert(0);
			return 0;
		}
	
		uint64 GetCreationTime() const override
		{
			LAssert(0);
			return 0;
		}

		uint64 GetLastAccessTime() const override
		{
			LAssert(0);
			return 0;
		}

		uint64 GetLastWriteTime() const override
		{
			if (!Ok())
				return false;

			auto e = ItemAt(pos);
			return e->Date.GetUnix();
		}

		uint64 GetSize() const override
		{
			if (!Ok())
				return false;

			auto e = ItemAt(pos);
			return e->Size;
		}
	
		int64 GetSizeOnDisk() override
		{
			return GetSize();
		}

		bool IsDir() const override
		{
			if (!Ok())
				return false;

			auto e = ItemAt(pos);
			return e->IsDir();
		}
	
		bool IsSymLink() const override
		{
			LAssert(0);
			return 0;
		}
	
		bool IsReadOnly() const override
		{
			LAssert(0);
			return 0;
		}

		bool IsHidden() const override
		{
			if (!Ok())
				return false;

			auto e = ItemAt(pos);
			return e->IsHidden();
		}

		LDirectory *Clone() override
		{
			LAssert(0);
			return nullptr;
		}
	
		LVolumeTypes GetType() const override
		{
			LAssert(0);
			return VT_NONE;
		}
	};

public:
	FtpBackend(LView *view, LString addr, LStream *logger) :
		SystemIntf(logger, "FtpBackend"),
		LThread("FtpBackend.Thread"),
		parent(view),
		uri(addr),
		log(logger)
	{
		Run();
	}

	~FtpBackend()
	{
		Cancel();
		WaitForExit(5000);
		cache.DeleteObjects();
	}

	void GetSysType(std::function<void(SysPlatform)> cb) override
	{
		if (cb) cb(PlatformLinux);
	}

	bool IsReady() override
	{
		return Connected;
	}

	LString GetBasePath() override
	{
		return ".";
	}

	LString MakeRelative(LString absPath) override
	{
		return absPath;
	}

	LString MakeAbsolute(LString relPath) override
	{
		LAssert(0);
		return LString();
	}

	LString JoinPath(LString base, LString leaf) override
	{
		LAssert(0);
		return LString();
	}

	void ResolvePath(LString path, LString::Array hints, std::function<void(LError&,LString)> cb) override
	{
		if (path && cb)
		{
			LError err;
			cb(err, path);
		}
	}

	// Reading and writing:
	LString ConvertPath(LString s)
	{
		LString unix = s.Replace("\\", "/");
		if (unix(0) == '.')
			return unix(1, -1);
		return unix;
	}

	bool SetRemote(LString s, bool stripLeaf)
	{
		if (stripLeaf)
		{
			auto lastSep = s.RFind("/");
			if (lastSep >= 0)
				s = s(0, lastSep ? lastSep : 1);
		}

		if (remote == s)
			return true;

		return ftp->SetDir(remote = s);
	}

	bool Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) override
	{
		if (!path || !cb)
			return false;

		printf("%s:%i - stat(%s)\n", _FL, path.Get());

		AddWork(MakeContext(_FL, path),
			TForeground,
			[this, path, cb=std::move(cb)]()
			{
				LArray<IFtpEntry*> dir;
				LError err;
				struct stat s = {};

				if (!SetRemote(path, true))
				{
					err.Set(LErrorPathNotFound);
				}
				else if (!ftp->ListDir(dir))
				{
					err.Set(LErrorFuncFailed);
				}
				else // look through 'dir' 
				{
					auto leaf = LGetLeaf(path);
					for (auto e: dir)
					{
						if (e->Path.Equals(leaf))
						{
							// found the entry
							#ifdef WINDOWS
							auto unixTime = e->Date.GetUnix();
							#else
							timespec unixTime = { e->Date.GetUnix(), 0 };
							#endif
							s.st_size = (TOffset) e->Size;
							s.st_mode = 0644;
							#ifdef MAC
								s.st_atimespec = unixTime;
								s.st_mtimespec = unixTime;
								s.st_ctimespec = unixTime;
							#elif !defined WINDOWS
								s.st_atime = unixTime;
								s.st_mtime = unixTime;
								s.st_ctime = unixTime;
							#else
								s.st_atim = unixTime;
								s.st_mtim = unixTime;
								s.st_ctim = unixTime;
							#endif
						}
					}
				}

				parent->RunCallback([this, s, err, path, cb=std::move(cb)]() mutable
					{
						if (err)
							cb(nullptr, LString(), err);
						else
							cb(&s, path, err);
					});
			});

		return true;
	}

	bool ReadFolder(TPriority priority, const char *Path, std::function<void(LDirectory*)> results) override
	{
		if (!Path || !results)
			return false;

		AddWork(MakeContext(_FL, Path),
			priority,
			[this, Path = ConvertPath(Path), results]() mutable
			{
				if (!GetReady())
					return;

				if (!SetRemote(Path, false))
				{
					log->Print("SetDir(%s) failed.\n", Path.Get());
					return;
				}

				LAutoPtr<FtpDir> curDir(new FtpDir(Path));
				if (ftp->ListDir(*curDir))
				{
					{
						// Store the names in the cache
						Auto lck(this, _FL);
						for (auto e: *curDir)
						{
							if (!e->IsDir())
							{
								auto full = LString::Fmt("%s/%s", Path ? Path.Get() : "", e->Name.Get());
								auto c = cache.Find(full);
								if (!c)
									cache.Add(full, c = new TCache);
								if (c)
									c->entry = *e;
							}
						}
					}

					curDir->Valid();
					parent->RunCallback([this, results, curDir=curDir.Release()]()
					{
						curDir->Valid();
						results(curDir);
						curDir->Valid();
						delete curDir;
					});
				}
			});

		return true;
	}

	bool Read(TPriority priority, const char *inPath, std::function<void(LError,LString)> result) override
	{
		if (!inPath || !result)
			return false;

		auto path = ConvertPath(inPath);
		{
			Auto lck(this, _FL);

			// Check if we have the data in the cache first...
			if (auto c = cache.Find(path))
			{
				if (c->data)
				{
					LError err;
					result(err, c->data);
					return true;
				}
			}
		}

		AddWork(
			MakeContext(_FL, path),
			priority,
			[this, path, result]() mutable
			{
				LError err;
				LString fileData;

				if (!GetReady())
				{
					err.Set(LErrorFuncFailed, "connection not ready.");
				}
				else
				{
					IFtpEntry entry;
					{
						Auto lck(this, _FL);
						if (auto c = cache.Find(path))
							entry = c->entry;
						else
							err.Set(LErrorFuncFailed, "entry not found.");
					}

					// Make sure we're in the right folder...
					if (!err)
					{
						try
						{
							if (!SetRemote(path, true))
							{
								err.Set(LErrorFuncFailed, "failed to set dir.");
							}
							else if (entry.Name)
							{
								LStringPipe data(16 << 10);

								if (ftp->DownloadStream(&data, &entry))
								{
									fileData = data.NewLStr();
									{
										// Store the data in the cache
										Auto lck(this, _FL);
										auto c = cache.Find(path);
										LAssert(c);
										if (c)
											c->data = fileData;
									}
								}
								else err.Set(LErrorIoFailed, "download failed.");
							}
						}
						catch (ssize_t result)
						{
							auto msg = LString::Fmt("Ftp cmd failed with " LPrintfSSizeT "\n", result);
							err.Set(LErrorFuncFailed, msg);
							log->Print(msg);
						}
					}
				}

				parent->RunCallback([this, err, fileData, result]()
				{
					result(err, fileData);
				});
			});

		return true;
	}

	bool Write(TPriority priority, const char *outPath, LString Data, std::function<void(LError)> result) override
	{
		if (!outPath || !result)
			return false;

		auto path = ConvertPath(outPath);
		AddWork(
			MakeContext(_FL, path),
			priority,
			[this, path, result, Data]() mutable
			{
				LError err;

				if (!GetReady())
				{
					err.Set(LErrorFuncFailed, "connection not ready.");
				}
				else
				{
					{
						Auto lck(this, _FL);

						// Update the cache
						if (auto c = cache.Find(path))
							c->data = Data;
						else
							err.Set(LErrorFuncFailed, "cache obj not found.");
					}

					if (!err)
					{
						// Make sure we're in the right folder...
						if (!SetRemote(path, true))
						{
							err.Set(LErrorFuncFailed, "failed to set dir.");
						}
						else
						{
							LMemStream wrapper(Data.Get(), Data.Length(), false);
							if (!ftp->UploadStream(&wrapper, path))
								err.Set(LErrorIoFailed, "upload failed.");
						}
					}
				}

				parent->RunCallback([this, err, result]()
				{
					result(err);
				});
			});

		return true;
	}

	bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) override
	{
		LAssert(0);
		return false;
	}

	bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) override
	{
		LAssert(0);
		return false;
	}

	bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) override
	{
		LAssert(0);
		return false;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) override
	{
		if (!searchTerms || !results)
			return false;

		LArray<LString> matches;
		auto terms = LString(searchTerms).SplitDelimit();
		Auto lck(this, _FL);
		for (auto p: cache)
		{
			int match = 0;
			for (auto t: terms)
			{
				if (Strstr(p.key, t.Get()))
					match++;
			}
			if (match == terms.Length())
				matches.Add(p.key);
		}

		results(matches);
		return true;
	}

	struct TFindInFiles
	{
		enum TState
		{
			TNone,
			TReading,
			TChecked,
		};

		FindParams params;
		FtpBackend *owner = nullptr;
		LStream *results = nullptr;
		LHashTbl<ConstStrKey<char>, TState> states;

		TFindInFiles(FtpBackend *o) : owner(o)
		{
			// Do an initial iterate to see what's there...
			Post(owner->MakeContext(_FL, "FindInFiles"), 0);
		}

		void Post(LString ctx, int addMilliseconds)
		{
			LMutex::Auto lck(owner, _FL);
			if (auto w = new TTimedWork(ctx, [this]() { Iterate(); }))
			{
				w->ts = LCurrentTime() + addMilliseconds;
				owner->timedWork.Add(w);
			}
		}

		void Read(const char *file)
		{
			LMutex::Auto lck(owner, _FL);
			states.Add(file, TReading);
			owner->Read(SystemIntf::TBackground, file, [this](auto err, auto data)
				{
				});
		}

		void Scan(const char *fileName, LString &data)
		{
			LAssert(data);
			states.Add(fileName, TChecked);
			auto terms = params.Text.SplitDelimit();
			auto lines = data.SplitDelimit("\n");

			for (size_t i=0; i<lines.Length(); i++)
			{
				auto &ln = lines[i];
				for (auto &t: terms)
				{
					if (ln.Find(t) >= 0)
					{
						results->Print("%s:%i %s\n", fileName+1, (int)(i+1), ln.Get());
						break;
					}
				}
			}
		}

		bool MatchExtension(const char *file)
		{
			if (!params.Ext)
				return true; // match all

			auto leaf = LGetLeaf(file);
			auto parts = params.Ext.SplitDelimit(", \t");
			for (auto p: parts)
			{
				if (MatchStr(p, leaf))
					return true;
			}
			return false;
		}

		void Iterate()
		{
			LMutex::Auto lck(owner, _FL);

			int done = 0, reading = 0;
			for (auto p: owner->cache)
			{
				if (!MatchExtension(p.key))
				{
					done++;
					continue;
				}

				auto state = states.Find(p.key);
				switch (state)
				{
				case FtpBackend::TFindInFiles::TNone:
					if (p.value->data)
					{
						Scan(p.key, p.value->data);
						done++;
					}
					else
					{
						Read(p.key);
						reading++;
					}
					break;
				case FtpBackend::TFindInFiles::TReading:
					if (p.value->data)
					{
						Scan(p.key, p.value->data);
						done++;
					}
					else reading++;
					break;
				case FtpBackend::TFindInFiles::TChecked:
					done++;
					break;
				default:
					break;
				}
			}

			owner->log->Print("findinfiles: reading=%i, done=%i of %i\n", reading, done, (int)owner->cache.Length());
			if (done < owner->cache.Length())
			{
				Post(owner->MakeContext(_FL, "FindInFiles.Pulse"), 1000); // have another look in 1 second...
			}
			else
			{
				delete this;
			}
		}
	};

	bool FindInFiles(FindParams *params, LStream *results) override
	{
		if (!params || !results)
			return false;

		if (auto fif = new TFindInFiles(this))
		{
			fif->params = *params;
			fif->results = results;
			return true;
		}

		return false;
	}

	bool RunProcess(const char *initDir, const char *cmdLine, ProcessIo *io, LCancel *cancel, std::function<void(int)> cb, LStream *alt_log) override
	{
		// No process support
		return false;
	}

	bool ProcessOutput(const char *cmdLine, std::function<void(int32_t,LString)> cb) override
	{
		// No process support
		return false;
	}

	void OnSocketConnect() override
	{
		log->Print("Connected to '%s'\n", uri.sHost.Get());
		Connected = true;
	}

	int MsgBox(const char *Msg, const char *Title, int Btn = MB_OK) override
	{
		LAssert(0);
		return -1;
	}

	int Alert(const char *Title, const char *Text, const char *Btn1, const char *Btn2 = 0, const char *Btn3 = 0)
	{
		LAssert(0);
		return -1;
	}

	void Disconnect() override
	{
		log->Print("Disconnected from '%s'\n", uri.sHost.Get());
		Connected = false;
	}

	struct SslLoggingSocket : public SslSocket
	{
	public:
		SslLoggingSocket(LStream *log) : SslSocket(log)
		{
		}

		void OnRead(char *Data, ssize_t Len) override
		{
			log->Print("   > %.*s", (int)Len, Data);
		}

		void OnWrite(const char *Data, ssize_t Len) override
		{
			log->Print("   < %.*s", (int)Len, Data);
		}

		void OnDisconnect() override
		{
			log->Print("   disconnected\n");
		}

		void OnError(int ErrorCode, const char *ErrorDescription) override
		{
			log->Print("   error: %i, %s\n", ErrorCode, ErrorDescription);
		}
		
		void OnInformation(const char *Str)
		{
			log->Print("   info: %s\n", Str);
		}
	};

	int Main() override
	{
		bool Tls = true;
		bool LoggingSock = false;
		uint64_t lastConnect = 0;

		while (!IsCancelled())
		{
			if (!ftp || !Connected)
			{
				auto now = LCurrentTime();

				if (!ftp)
					ftp.Reset(new IFtp(this, this, Tls));

				if (now - lastConnect > 5000)
				{
					lastConnect = now;
					SslSocket *sock = nullptr;
					if (LoggingSock)
						sock = new SslLoggingSocket(log);
					else
						sock = new SslSocket(log);
					if (sock)
					{
						sock->SetCancel(this);
						auto res = ftp->Open(sock, uri.sHost, uri.Port, uri.sUser, uri.sPass);
						log->Print("Open=%i Conn=%i\n", res, Connected);
					}
					else log->Print("alloc err.\n");
				}
				else LSleep(WAIT_MS);
			}
			else if (ftp && !ftp->IsOpen())
			{
				// Disconnected?
				ftp.Reset();
				// LoggingSock = true;
				LSleep(1000);
				log->Print("ftp: resetting connection...\n");
			}
			else
			{
				DoWork();
			}
		}

		return 0;
	}
};

LAutoPtr<SystemIntf> CreateSystemInterface(LView *parent, LString uri, LStream *log)
{
	LAutoPtr<SystemIntf> backend;
	LUri u(uri);
	if (u.IsProtocol("ssh"))
		backend.Reset(new SshBackend(parent, uri, log));
	else if (u.IsProtocol("ftp"))
		backend.Reset(new FtpBackend(parent, uri, log));
	else	
		backend.Reset(new LocalBackend(parent, uri, log));
	return backend;
}
