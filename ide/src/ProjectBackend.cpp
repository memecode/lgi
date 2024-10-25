#include "lgi/common/Lgi.h"
#include "lgi/common/Uri.h"
#include "lgi/common/Thread.h"
#include "lgi/common/Ssh.h"
#include "lgi/common/Mutex.h"

#include "LgiIde.h"
#include "ProjectBackend.h"

#ifndef HAS_LIBSSH
#error "Add ssh to this project"
#endif

#define INFO(...) { if (log) log->Print(__VA_ARGS__); }

class SshBackend :
	public ProjectBackend,
	public LCancel,
	public LThread,
	public LMutex
{
	LView *app = NULL;
	LUri uri;
	LStream *log = NULL;
	LString prompt;
	IdePlatform sysType = PlatformUnknown;
	LString remoteSep;
	constexpr static const char *separators = "/\\";

	// Lock before use
	using TWork = std::function<void()>;
	LAutoPtr<LSsh> ssh;
	LAutoPtr<LSsh::SshConsole> console;
	LArray<TWork> work;

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
				ReadToPrompt();
				LgiTrace("got login prompt\n");
			}
		}
		return console;
	}

	uint64_t promptDetect = 0;

	bool AtPrompt(LStringPipe &p)
	{
		bool found = false;

		p.Iterate( [this, &found](auto ptr, auto size)
			{
				if (!prompt)
				{
					// Detect the prompt characters
					auto now = LCurrentTime();
					if (!promptDetect)
					{
						promptDetect = now;
					}
					else if (now - promptDetect >= 300)
					{
						LString last2((char*)ptr + size - 2, 2);
						if (last2 == "> " ||
							last2 == "$ " ||
							last2 == "# ")
						{
							// Unix like system
							prompt = last2;
							return true;
						}
						else if (size > 0 && ptr[size-1] == '>')
						{
							// Windows hopefully?
							prompt = ">";
							return true;
						}

						LAssert(!"Doesn't look like a prompt?");
					}

					return false;
				}
				else
				{
					auto promptLen = prompt.Length();
					if (size >= promptLen)
					{
						auto p = ptr + size - promptLen;
						auto cmp = memcmp(p, prompt, promptLen);
						if (cmp == 0)
							found = true;
					}
					return false;
				}
			},
			true);

		return found;
	}

	LString ReadToPrompt(LStream *output = NULL, LCancel *cancel = NULL)
	{
		LStringPipe p;
		if (auto c = GetConsole())
		{
			while (!IsCancelled())
			{
				if (cancel && cancel->IsCancelled())
				{
					break;
				}

				char buf[1024];
				auto rd = c->Read(buf, sizeof(buf));
				if (rd > 0)
				{
					// LgiTrace("ReadToPrompt got %i: %.16s\n", (int)rd, buf);
					p.Write(buf, rd);
					if (output)
						output->Write(buf, rd);
				}
				else if (AtPrompt(p))
					break;
				else
					LSleep(1);
			}
		}
		else LgiTrace("ReadToPrompt: no console.\n");

		auto s = p.NewLStr();
		RemoveAnsi(s);
		return s;
	}

	LString Cmd(LString cmd, int32_t *exitCode = NULL, LStream *outputStream = NULL, LCancel *cancel = NULL)
	{
		if (auto c = GetConsole())
		{
			// log->Print("Cmd: write '%s'\n", cmd.Strip().Get());
			if (!c->Write(cmd))
			{
				if (exitCode)
					*exitCode = -1;
				return LString();
			}

			auto output = ReadToPrompt(outputStream, cancel);
			
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
					auto val = ReadToPrompt();
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
		LThread("SshBackend.Thread"),
		LMutex("SshBackend.Lock"),
		app(parent),
		uri(u),
		log(logger)
	{
		Run();
	}

	~SshBackend()
	{
		Cancel();
		WaitForExit();
	}

	const char *GetClass() const { return "SshBackend"; }
	LString GetBasePath() override { return RemoteRoot(); }

	LString MakeRelative(LString absPath)
	{
		auto base = RemoteRoot();
		if (absPath.Find(base) == 0)
			return LString(".") + absPath(base.Length(), -1);
		return LString();
	}

	void OnConnected()
	{
		GetSysType(NULL);
	}

	void GetSysType(std::function<void(IdePlatform)> cb) override
	{
		if (sysType != PlatformUnknown)
		{
			if (cb)
				cb(sysType);
			return;
		}

		Auto lck(this, _FL);
		work.Add( [this, cb]()
		{
			if (auto output = Cmd("uname -a\n"))
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
		} );
	}

	class SshDir : public LDirectory
	{
		struct TEntry
		{
			LString perms, user, grp, name;
			LString month, day, time;
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
				e.day   = COL(6);
				e.time  = COL(7);
				if ((e.name = line(cols[7].End() + 1, -1).LStrip(" ")))
				{
					if (e.name(0) == '\'')
						e.name = e.name.Strip("\'");
				}
			}
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
		uint64 GetCreationTime() const { return 0; }
		uint64 GetLastAccessTime() const { return 0; }
		uint64 GetLastWriteTime() const { return 0; }
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

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) override
	{
		if (!Path || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, path = PreparePath(Path)]()
		{
			auto cmd = LString::Fmt("ls -lan %s\n", path.Get());
			auto ls = Cmd(cmd);
			auto lines = ls.SplitDelimit("\r\n").Slice(2, -2);
			
			bool debug = path.Equals("~/code/lgi/trunk/ide/");
			if (debug)
				log->Print("ReadFolder %s\n", path.Get());
			
			app->RunCallback( [dir = new SshDir(path, lines, debug ? log : NULL), results]() mutable
				{
					results(dir);
					delete dir;
				});
		} );
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) override
	{
		if (!searchTerms || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, searchTerms = LString(searchTerms)]()
		{
			auto root = RemoteRoot();
			auto parts = searchTerms.SplitDelimit();
			auto args = LString::Fmt("cd %s && find .", root.Get());
			for (size_t i=0; i<parts.Length(); i++)
				args += LString::Fmt("%s -iname \"*%s*\"", i ? " -and" : "", parts[i].Get());
			args += " -and -not -path \"*/.hg/*\"";

			auto result = Cmd(args + "\n");
			LArray<LString> lines = TrimContent(result).SplitDelimit("\r\n");
			app->RunCallback( [results, lines]() mutable
				{
					results(lines);
				});
		} );

		return true;
	}

	bool FindInFiles(FindParams *params, LStream *results) override
	{
		if (!params || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, params = new FindParams(params)]()
		{
			auto args = LString::Fmt("grep -R \"%s\" \"%s\"",
				params->Text.Get(),
				params->Dir.Get());
			results->Print("%s\n", args.Get());
			auto result = Cmd(args + "\n");
			auto output = TrimContent(result);
			results->Write(output);
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
		auto quoted = s.Replace(" ", "\\ ");

		if (remoteSep)
		{
			auto sep = remoteSep(0);
			for (char *c = quoted.Get(); *c; c++)
				if (Strchr(separators, *c))
					*c = sep;
		}

		return quoted;
	}

	bool Read(const char *Path, std::function<void(LError,LString)> result) override
	{
		if (!Path)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, result, Path = PreparePath(Path)]()
		{
			LStringPipe buf;
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

	bool Write(const char *Path, LString Data, std::function<void(LError)> result) override
	{
		if (!Path)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, result, Path = RemoteRoot(Path), Data]()
		{
			LMemStream stream(Data.Get(), Data.Length(), false);
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

		Auto lck(this, _FL);
		work.Add( [this, cb, createParents, Path = PreparePath(path)]()
		{
			auto args = LString::Fmt("mkdir%s %s", createParents ? " -p" : "", Path.Get());
			int32_t exitVal;
			auto result = Cmd(args + "\n", &exitVal);
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

		Auto lck(this, _FL);
		work.Add( [this, cb, recursiveForce, Path = PreparePath(path)]()
		{
			auto args = LString::Fmt("rm%s %s", recursiveForce ? " -rf" : "", Path.Get());
			int32_t exitVal;
			auto result = Cmd(args + "\n", &exitVal);
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

		Auto lck(this, _FL);
		work.Add( [this, cb, from = PreparePath(oldPath), to = PreparePath(newPath)]()
		{
			auto args = LString::Fmt("mv %s %s", from.Get(), to.Get());
			int32_t exitVal;
			auto result = Cmd(args + "\n", &exitVal);
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

	bool RunProcess(const char *initDir, const char *cmdLine, LStream *output, LCancel *cancel, std::function<void(int)> cb)
	{
		if (!cmdLine)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, cb, initDir = LString(initDir), cmdLine = LString(cmdLine), output, cancel]()
		{
			auto args = LString::Fmt("cd %s && %s", initDir.Get(), cmdLine.Get());

			int32_t exitVal;
			Cmd(args + "\n", &exitVal, output, cancel);
			if (cb)
			{
				app->RunCallback( [exitVal, cb]()
					{
						cb(exitVal);
					});
			}
		} );

		return true;
	}

	int Main() override
	{
		while (!IsCancelled())
		{
			LArray<TWork> todo;			
			{
				Auto lck(this, _FL);
				todo.Swap(work);
			}
			if (todo.Length())
			{
				for (auto &t: todo)
					t();
			}
			else LSleep(100);
		}

		return 0;
	}
};

class LocalBackend : public ProjectBackend
{
	LView *app;
	LString folder;
	LStream *log = NULL;

public:
	LocalBackend(LView *parent, LString uri, LStream *logger) :
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

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) override
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

	bool Read(const char *Path, std::function<void(LError,LString)> result) override
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

	bool Write(const char *Path, LString Data, std::function<void(LError)> result) override
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

	void GetSysType(std::function<void(IdePlatform)> cb) override
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

	bool RunProcess(const char *initDir, const char *cmdLine, LStream *output, LCancel *cancel, std::function<void(int)> cb)
	{
		return false;
	}
};


LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri, LStream *log)
{
	LAutoPtr<ProjectBackend> backend;
	LUri u(uri);
	if (u.IsProtocol("ssh"))
		backend.Reset(new SshBackend(parent, uri, log));
	else	
		backend.Reset(new LocalBackend(parent, uri, log));
	return backend;
}
