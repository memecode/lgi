#pragma once

#include "lgi/common/RemoveAnsi.h"

//////////////////////////////////////////////////////////////////////
// Platform stuff
enum SysPlatform
{
	PlatformUnknown = -2,
	PlatformCurrent = -1,
	PlatformWin = 0,		// 0x1
	PlatformLinux,			// 0x2
	PlatformMac,			// 0x4
	PlatformHaiku,			// 0x8
	PlatformMax,
};
#define PLATFORM_WIN32			(1 << PlatformWin)
#define PLATFORM_LINUX			(1 << PlatformLinux)
#define PLATFORM_MAC			(1 << PlatformMac)
#define PLATFORM_HAIKU			(1 << PlatformHaiku)
#define PLATFORM_ALL			(PLATFORM_WIN32|PLATFORM_LINUX|PLATFORM_MAC|PLATFORM_HAIKU)
extern SysPlatform PlatformFlagsToEnum(int flags);
extern LString PlatformFlagsToStr(int flags);
extern const char *ToString(SysPlatform p);
extern const char *PlatformNames[];

#if defined(_WIN32)
#define PLATFORM_CURRENT		PLATFORM_WIN32
#elif defined(MAC)
#define PLATFORM_CURRENT		PLATFORM_MAC
#elif defined(LINUX)
#define PLATFORM_CURRENT		PLATFORM_LINUX
#elif defined(HAIKU)
#define PLATFORM_CURRENT		PLATFORM_HAIKU
#endif

// This class can take ANSI code input and emit non-ANSI code output
struct StripAnsiStream : public LStream
{
	LStream *out = nullptr;
	LStringPipe p;

	StripAnsiStream(LStream *str) : out(str) {}
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
	{
		auto wr = p.Write(Ptr, Size);

		while (auto ln = p.Pop())
		{
			RemoveAnsi(ln);
			out->Write(ln);
			out->Write("\n", 1);
		}

		return wr;
	}
};

class SystemIntf : public LMutex
{
public:
	enum TPriority
	{
		TBackground,
		TForeground
	};

	constexpr static int WAIT_MS = 50;
	LStream *log = NULL;

	LString MakeContext(const char *file, int line, LString data)
	{
		return LString::Fmt("%s:%i %s", file, line, data.Get());
	}

protected:
	// Lock before using
	struct TWork;
	using TCallback = std::function<void()>;
	struct TWork
	{
		LString context;
		TCallback fp;

		TWork(LString &ctx, TCallback &&call)
		{
			context = ctx;
			fp = std::move(call);
		}

		~TWork()
		{
			// LStackTrace("%p::~TWork", this);
		}
	};
	struct TTimedWork : public TWork
	{
		uint64_t ts;
		TTimedWork(LString &ctx, TCallback &&call) : TWork(ctx, std::move(call)) {}
	};
	LArray<TWork*> foregroundWork, backgroundWork;
	LArray<TTimedWork*> timedWork;
	uint64_t lastLogTs = 0;

	// This adds work to the queue:
	void AddWork(LString ctx, TPriority priority, TCallback &&job);	
	// Call this in the main function of the sub-class:
	void DoWork();

public:
	#if defined(HAIKU) || defined(MAC)
		using TOffset = off_t;
	#elif defined(WINDOWS)
		using TOffset = _off_t;
	#else
		using TOffset = __off_t;
	#endif

	SystemIntf(LStream *logger, LString name) :
		LMutex(name + ".lock"),
		log(logger)
	{
	}
	virtual ~SystemIntf() {}

	virtual void GetSysType(std::function<void(SysPlatform)> cb) = 0;
	virtual bool IsReady() { return true; }

	// Path:
	virtual LString GetBasePath() = 0;
	virtual LString MakeRelative(LString absPath) = 0;
	virtual LString MakeAbsolute(LString relPath) = 0;
	virtual LString JoinPath(LString base, LString leaf) = 0;
	virtual void ResolvePath(LString path, LString::Array hints, std::function<void(LError&,LString)> cb) = 0;

	// Reading and writing:
	virtual bool Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) = 0;
	virtual bool ReadFolder(TPriority priority, const char *Path, std::function<void(LDirectory*)> results) = 0;
	virtual bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) = 0;
	virtual bool Read(TPriority priority, const char *Path, std::function<void(LError,LString)> result) = 0;
	virtual bool Write(TPriority priority, const char *Path, LString Data, std::function<void(LError)> result) = 0;
	virtual bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) = 0;
	virtual bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) = 0;

	// Searching:
	virtual bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) = 0;

	struct FindParams
	{
		enum TType
		{
			SearchPaths,		// Scan only paths in 'Paths'
			SearchDirectory,	// Scan recursively under 'Dir'
		}	Type;

		LString Dir;
		LString::Array Paths;

		LString Text;
		LString Ext;
		bool MatchWord = false;
		bool MatchCase = false;
		bool SubDirs = true;
	};
	virtual bool FindInFiles(FindParams *params, LStream *results) = 0;

	// Process:
	struct ProcessIo // Various options for process input/output
	{
		// Just get the output in a stream:
		LStream *output = nullptr;
		
		// Alternatively, get the console stream ptr as a callback
		// However once the client code is done with the stream/console
		// It will need to exit the process thread with a call to CallMethod(ObjCancel)
		// on the stream itself.
		std::function<void(LStream*)> ioCallback;
	};
	// Start a long running process independent of the main SSH connect.
	virtual bool RunProcess(const char *initDir,
							const char *cmdLine,
							ProcessIo *io,
							LCancel *cancel,
							std::function<void(int)> cb,
							LStream *alt_log = nullptr) = 0;
	// Run a short process and get the output
	virtual bool ProcessOutput(const char *cmdLine, std::function<void(int32_t,LString)> cb) = 0;
};

extern LAutoPtr<SystemIntf> CreateSystemInterface(LView *parent, LString uri, LStream *log);
