#pragma once

#include <initializer_list>
#include "lgi/common/File.h"

class LStructuredLog
{
	LFile f;
	size_t num_args;
	
	template<typename T>
	void Store(T &t)
	{
		int asd=0;
	}

public:
	LStructuredLog(const char *FileName);
	virtual ~LStructuredLog();

	template<typename... Args>
	void Log(Args&&... args)
	{
		num_args = sizeof...(Args);
		int dummy[] = { 0, ( (void) Store(std::forward<Args>(args)), 0) ... };
	}
};
