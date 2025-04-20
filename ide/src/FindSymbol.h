#pragma once

#include "lgi/common/EventTargetThread.h"
#include "lgi/common/SystemIntf.h"

enum FindSymMessages
{
	/// Find symbol results message:
	/// A=FindSymRequest*
	M_FIND_SYM_REQUEST = M_USER + 1000,
	
	/// Send a file to the worker thread...
	/// A=FindSymbolSystem::SymFileParams*
	M_FIND_SYM_FILE,

	/// Send a file to the worker thread...
	/// A=LString::Array*
	M_FIND_SYM_INC_PATHS,

	// Set the backend
	// A=ProjectBackend*
	M_FIND_SYM_BACKEND, 

	// Scan a backend folder:
	// A=LString *path
	M_SCAN_FOLDER,

	// Sent from the find syms to the app to get the project cache folder
	M_GET_PROJECT_CACHE,

	// Clear the cached symbols and reparse
	M_CLEAR_PROJECT_CACHE,
};

struct FindSymResult
{
	LString Symbol, File;
	int Line;
	int Score;
	
	FindSymResult(const FindSymResult &c)
	{
		*this = c;
	}
	
	FindSymResult(const char *f = NULL, int line = 0)
	{
		Score = 0;
		File = f;
		Line = line;
	}
	
	FindSymResult &operator =(const FindSymResult &c)
	{
		Score = c.Score;
		Symbol = c.Symbol;
		File = c.File;
		Line = c.Line;
		return *this;
	}

	int Class()
	{
		if (Score > 0)
			return 0;

		auto e = LGetExtension(File);
		if (!e)
			return 3;
		
		if (!_stricmp(e, "h") ||
			!_stricmp(e, "hpp") ||
			!_stricmp(e, "hxx"))
			return 1;
		
		return 2;
	}

	// Sort symbol results by:
	int Compare(FindSymResult *r)
	{
		// Score first...
		if (Score != r->Score)
			return r->Score - Score;

		// Then headers...
		int c1 = Class();
		int c2 = r->Class();
		if (c1 != c2)
			return c1 - c2;

		// Then by file name...
		int c = _stricmp(File, r->File);
		if (c)
			return c;

		// Then by line number...
		return Line - r->Line;
	}
};

struct FindSymRequest
{
	int SinkHnd;
	LString Str;
	LArray<FindSymResult*> Results;
	
	FindSymRequest(int sinkhnd)
	{
		SinkHnd = sinkhnd;
	}
	
	~FindSymRequest()
	{
		Results.DeleteObjects();
	}
};

class FindSymbolSystem : public LEventSinkI
{
	struct FindSymbolSystemPriv *d;

public:
	enum SymAction
	{
		FileAdd,
		FileRemove,
		FileReparse,
		FileRemoveAll,
	};

	struct SymFileParams
	{
		SymAction Action;
		LString File;
		int Platforms = 0;
		bool cacheDirty = false;
	};

	struct SymPathParams
	{
		LString::Array Paths;
		LString::Array SysPaths;
		int Platforms = 0;
	};

	FindSymbolSystem(int AppHnd);
	~FindSymbolSystem();
	
	void SetBackend(SystemIntf *backend);
	int GetAppHnd();
	bool SetIncludePaths(LString::Array &Paths, LString::Array &SysPaths, int Platforms);
	bool OnFile(const char *Path, SymAction Action, int Platforms);
	void OpenSearchDlg(LViewI *Parent, std::function<void(FindSymResult&)> Callback);
	bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1);
	void ClearCache();
	
	/// This function searches the database for symbols and returns
	/// the results as a M_FIND_SYM_REQUEST message.
	void Search(int ResultsSinkHnd, const char *SearchStr, int Platforms);
	
	// If the find sym system is using a backend there could be multiple
	// outstanding requests that will get processed via callbacks. These
	// need to settle before the object can be deleted... this starts
	// that process and then calls the callback when safe to delete:
	void Shutdown(std::function<void()> callback);
};
