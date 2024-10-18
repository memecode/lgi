#pragma once

class ProjectBackend
{
public:
	struct FindParams
	{
		LString term;
		LString::Array fileTypes;
		LString subFolder;
		bool matchWord = false;
		bool matchCase = false;
	};
	struct Result
	{
		LString file;
		int line;
		LString preview;
	};

	virtual ~ProjectBackend() {}

	// Path:
	virtual LString GetBasePath() = 0;

	// Reading and writing:
	virtual bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) = 0;
	virtual bool Read(const char *Path, std::function<void(LError,LString)> result) = 0;
	virtual bool Write(const char *Path, LString Data, std::function<void(LError)> result) = 0;
	virtual bool CreateFolder(const char *path, bool createParents, std::function<void(bool)> cb) = 0;
	virtual bool Delete(const char *path, bool recursiveForce, std::function<void(bool)> cb) = 0;
	virtual bool Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) = 0;

	// Seaching:
	virtual bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) = 0;
	virtual bool FindInFiles(FindParams &params, std::function<void(LArray<Result>&)> results) = 0;
};

extern LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri, LStream *log);