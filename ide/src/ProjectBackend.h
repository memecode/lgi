#pragma once

#include "IdeFindInFiles.h"

class ProjectBackend
{
public:
	virtual ~ProjectBackend() {}

	virtual void GetSysType(std::function<void(IdePlatform)> cb) = 0;

	// Path:
	virtual LString GetBasePath() = 0;
	virtual LString MakeRelative(LString absPath) = 0;

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
	virtual bool RunProcess(const char *initDir, const char *cmdLine, LStream *output, LCancel *cancel, std::function<void(int)> cb) = 0;
};

extern LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri, LStream *log);
