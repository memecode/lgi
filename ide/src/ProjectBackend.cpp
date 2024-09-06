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

class SshBackend :
	public ProjectBackend,
	public LCancel,
	public LThread,
	public LMutex
{
	LView *app;
	LUri uri;
	LTextLog *log = NULL;

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
				if (!ssh->Open(uri.sHost, uri.sUser, NULL, true))
					return NULL;
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
		const char *prompt = ":~$ ";

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
			while (true)
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

	LString Cmd(LString cmd)
	{
		if (auto c = GetConsole())
		{
			LgiTrace("Cmd: write '%s'\n", cmd.Strip().Get());
			if (!c->Write(cmd))
				return LString();

			LgiTrace("Cmd: reading:\n");
			return ReadToPrompt();
		}
		return LString();
	}

public:
	SshBackend(LView *parent, LString u) :
		LThread("SshBackend.Thread"),
		LMutex("SshBackend.Lock"),
		app(parent),
		uri(u)
	{		Run();
	}

	~SshBackend()
	{
		Cancel();
		WaitForExit();
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
		int GetUser(bool Group) const { return 0; }
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

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results)
	{
		if (!Path || !results)
			return false;

		Auto lck(this, _FL);
		work.Add( [this, results, path = LString(Path)]()
		{
			auto full = LString::Fmt("~%s/%s", uri.sPath.Get(), path.Get());
			auto ls = Cmd(LString::Fmt("ls -la %s\n", full.Get()));
			auto lines = ls.SplitDelimit("\r\n").Slice(2, -2);
			app->RunCallback( [dir = new SshDir(path, lines), results]() mutable
				{
					results(dir);
					delete dir;
				});
		} );
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LString::Array&)> results)
	{
		return false;
	}

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

public:
	LocalBackend(LView *parent, LString uri) :
		app(parent)
	{
		LUri u(uri);
		folder = u.LocalPath();
	}

	~LocalBackend()
	{
	}

	bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results)
	{
		LDirectory dir;
		if (!dir.First(Path))
			return false;

		results(&dir);
		return true;
	}

	bool SearchFileNames(const char *searchTerms, std::function<void(LString::Array&)> results)
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
};

LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri)
{
	LAutoPtr<ProjectBackend> backend;
	LUri u(uri);
	if (u.IsProtocol("ssh"))
		backend.Reset(new SshBackend(parent, uri));
	else	
		backend.Reset(new LocalBackend(parent, uri));
	return backend;
}