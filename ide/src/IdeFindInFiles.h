#ifndef _FIND_IN_FILES_H_
#define _FIND_IN_FILES_H_

#include "lgi/common/EventTargetThread.h"
#include "History.h"

enum FifSearchType
{
	FifSearchSolution,
	FifSearchDirectory,
};

class FindParams
{
public:
	FifSearchType Type;
	LString Text;
	LString Ext;
	LString Dir;
	bool MatchWord = false;
	bool MatchCase = false;
	bool SubDirs = true;
	LString::Array ProjectFiles;
	
	FindParams(const FindParams *Set = NULL)
	{
		Type = FifSearchDirectory;

		Ext = "*.c* *.h *.java";

		char Exe[MAX_PATH_LEN];
		LMakePath(Exe, sizeof(Exe), LGetExePath(), "..");
		Dir = Exe;
		
		if (Set)
			*this = *Set;
	}
	
	FindParams &operator =(const FindParams *p)
	{
		Type = p->Type;
		
		// Make explicit copies of the LString's to ensure thread safety.
		Text = p->Text.Get();
		Ext = p->Ext.Get();
		Dir = p->Dir.Get();
		
		ProjectFiles.Length(p->ProjectFiles.Length());
		for (unsigned i=0; i<ProjectFiles.Length(); i++)
			ProjectFiles[i] = p->ProjectFiles.ItemAt(i).Get();
		
		MatchWord = p->MatchWord;
		MatchCase = p->MatchCase;
		SubDirs = p->SubDirs;
		
		return *this;
	}
};

class FindInFiles : public LDialog
{
	class AppWnd *App = NULL;
	LHistory *TypeHistory = NULL;
	LHistory *FolderHistory = NULL;
	bool OwnParams = false;

public:
	FindParams *Params = NULL;
	
	FindInFiles(AppWnd *app, FindParams *params = NULL);
	~FindInFiles();
	
	int OnNotify(LViewI *v, LNotification n) override;
	void OnCreate() override;
};

class FindInFilesThread : public LEventTargetThread
{
	class FindInFilesThreadPrivate *d;

	void Log(const char *Str /*Heap Str*/);
	void SearchFile(char *File);

public:
	enum Msgs
	{
		M_START_SEARCH = M_USER + 100, // A=(FindParams*)
	};

	FindInFilesThread(int AppHnd);
	~FindInFilesThread();
	
	void Stop();
	LMessage::Result OnEvent(LMessage *Msg);
};

#endif
