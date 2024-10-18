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
	LView *app;
	LUri uri;
	LStream *log = NULL;
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
					INFO("%s: Connected to '%s'\n", GetClass(), uri.sHost.Get())
				else
				{
					INFO("%s: Error connecting to '%s'\n", GetClass(), uri.sHost.Get())
					return NULL;
				}
			}
		}
		if (ssh && !console)
		{
			if (console = ssh->CreateConsole())
			{
				LgiTrace("Created console... reading to prompt:\n");
				ReadToPrompt();
				LgiTrace("got login prompt\n");
			}
		}
		return console;
	}

	bool AtPrompt(LStringPipe &p)
	{
		bool found = false;
		const char *prompt = "$ ";

		p.Iterate( [this, prompt, &found](auto ptr, auto size)
			{
				auto promptLen = Strlen(prompt);
				if (size >= promptLen)
				{
					auto p = ptr + size - promptLen;
					auto cmp = memcmp(p, prompt, promptLen);
					if (cmp == 0)
						found = true;
				}
				return false;
			},
			true);

		return found;
	}

	LString ReadToPrompt()
	{
		LStringPipe p;
		if (auto c = GetConsole())
		{
			while (!IsCancelled())
			{
				char buf[1024];
				auto rd = c->Read(buf, sizeof(buf));
				if (rd > 0)
				{
					// LgiTrace("ReadToPrompt got %i: %.16s\n", (int)rd, buf);
					p.Write(buf, rd);
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

	LString Cmd(LString cmd, int32_t *exitCode = NULL)
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

			auto output = ReadToPrompt();
			
			if (exitCode)
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
		size_t pos = 0;
		LString BasePath;
		LString Cache;

		bool Ok() const { return pos < entries.Length(); }

	public:
		SshDir(const char *basePath, LArray<LString> &lines)
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
				e.name  = line(cols[7].End() + 1, -1);
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
				return Atoi(entries.ItemAt(pos).user.Get());
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
		auto pathParts = uri.sPath.SplitDelimit(separators);
		if (subFolder)
			pathParts += LString(subFolder).SplitDelimit(separators);
		for (int i=0; i<pathParts.Length(); i++)
			if (pathParts[i] == ".")
			{
				pathParts.DeleteAt(i--, true);
			}
			else if (pathParts[i] == "..")
			{
				pathParts.DeleteAt(i--, true);
				pathParts.DeleteAt(i--, true);
			}
		pathParts.SetFixedLength(false);
		pathParts.AddAt(0, "~");

		return LString("/").Join(pathParts);
	}

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) override
	{
		if (!Path || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, path = LString(Path)]()
		{
			auto ls = Cmd(LString::Fmt("ls -lan %s\n", path.Get()));
			auto lines = ls.SplitDelimit("\r\n").Slice(2, -2);
			app->RunCallback( [dir = new SshDir(path, lines), results]() mutable
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

			auto result = Cmd(args + "\n");
			LArray<LString> lines = TrimContent(result).SplitDelimit("\r\n");
			app->RunCallback( [results, lines]() mutable
				{
					results(lines);
				});
		} );

		return true;
	}

	bool FindInFiles(FindParams &params, std::function<void(LArray<Result>&)> results)
	{
		if (!params.term || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, params]()
		{
			auto root = RemoteRoot(params.subFolder);
			auto args = LString::Fmt("grep -R \"%s\" \"%s\"",
				params.term.Get(),
				root.Get());

			auto result = Cmd(args + "\n");
			LArray<LString> lines = TrimContent(result).SplitDelimit("\r\n");
			app->RunCallback( [results, lines]() mutable
				{
					// Parse lines into Result structs
					LAssert(!"impl me");
				});
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

	LString Quote(LString s)
	{
		return s.Replace(" ", "\\ ");
	}

	bool Read(const char *Path, std::function<void(LError,LString)> result) override
	{
		if (!Path)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, result, Path = LString(Path)]()
		{
			auto root = RemoteRoot(Path);			
			LStringPipe buf;
			auto err = ssh->DownloadFile(&buf, root);
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

	bool Write(const char *Path, LString Data, std::function<void(LError)> result)
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
		work.Add( [this, cb, createParents, Path = Quote(path)]()
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
		work.Add( [this, cb, recursiveForce, Path = Quote(path)]()
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

	int Main()
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

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results)
	{
		LDirectory dir;
		if (!dir.First(Path))
			return false;

		results(&dir);
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results)
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

	bool FindInFiles(FindParams &params, std::function<void(LArray<Result>&)> results)
	{
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

	bool Write(const char *Path, LString Data, std::function<void(LError)> result)
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

	bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb)
	{
		LError err(LErrorNone);
		auto result = FileDev->CreateFolder(path, false, &err);
		if (cb)
			cb(!err);
		return result;
	}

	bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb)
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
