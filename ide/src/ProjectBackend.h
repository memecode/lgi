#pragma once

class ProjectBackend
{
public:
	virtual ~ProjectBackend() {}

	virtual bool ReadFolder(const char *Path, std::function<void(LDirectory*)> results) = 0;
	virtual bool SearchFileNames(const char *searchTerms, std::function<void(LString::Array&)> results) = 0;
};

extern LAutoPtr<ProjectBackend> CreateBackend(LView *parent, LString uri);