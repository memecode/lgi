#pragma once

class ProjectBackend
{
public:
	virtual ~ProjectBackend() {}

	virtual bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) = 0;
	virtual bool SearchFileNames(const char *searchTerms, std::function<void(LArray<LString>&)> results) = 0;
	virtual bool Read(const char *Path, std::function<void(LError,LString)> result) = 0;
	virtual bool Write(const char *Path, LString Data, std::function<void(LError)> result) = 0;
};

extern LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri, LStream *log);