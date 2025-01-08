#pragma once

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


class SystemIntf
{
public:
	enum TPriority
	{
		TBackground,
		TForeground
	};

	#if defined(HAIKU) || defined(MAC)
		using TOffset = off_t;
	#elif defined(WINDOWS)
		using TOffset = _off_t;
	#else
		using TOffset = __off_t;
	#endif

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
	virtual bool Read(TPriority priority, const char *Path, std::function<void(LError,LString)> result) = 0;
	virtual bool Write(TPriority priority, const char *Path, LString Data, std::function<void(LError)> result) = 0;
	virtual bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) = 0;
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
		LStream *output;
		
		// Alternatively, get the console stream ptr as a callback
		// However once the client code is done with the stream/console
		// It will need to exit the process thread with a call to CallMethod(ObjCancel)
		// on the stream itself.
		std::function<void(LStream*)> ioCallback;
	};
	// Start a long running process independent of the main SSH connect.
	virtual bool RunProcess(const char *initDir, const char *cmdLine, ProcessIo *io, LCancel *cancel, std::function<void(int)> cb) = 0;
	// Run a short process and get the output
	virtual bool ProcessOutput(const char *cmdLine, std::function<void(int32_t,LString)> cb) = 0;
};

extern LAutoPtr<SystemIntf> CreateSystemInterface(LView *parent, LString uri, LStream *log);
