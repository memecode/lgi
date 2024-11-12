#pragma once

#include "IdeFindInFiles.h"
#include "IdePlatform.h"

class ProjectBackend
{
public:
	virtual ~ProjectBackend() {}

	virtual void GetSysType(std::function<void(IdePlatform)> cb) = 0;
	virtual bool IsReady() { return true; }

	// Path:
	virtual LString GetBasePath() = 0;
	virtual LString MakeRelative(LString absPath) = 0;
	virtual LString MakeAbsolute(LString relPath) = 0;
	virtual LString JoinPath(LString base, LString leaf) = 0;
	virtual void ResolvePath(LString path, LString::Array hints, std::function<void(LError&,LString)> cb) = 0;

	// Reading and writing:
	virtual bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) = 0;
	virtual bool Read(const char *Path, std::function<void(LError,LString)> result) = 0;
	virtual bool Write(const char *Path, LString Data, std::function<void(LError)> result) = 0;
	virtual bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) = 0;
	virtual bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) = 0;
	virtual bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) = 0;

	// Searching:
	virtual bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) = 0;
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

extern LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri, LStream *log);
