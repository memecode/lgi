#ifndef _FIND_IN_FILES_H_
#define _FIND_IN_FILES_H_

#include "GHistory.h"

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
	bool MatchWord;
	bool MatchCase;
	bool SubDirs;
	LArray<LString> ProjectFiles;
	
	FindParams(const FindParams *Set = NULL)
	{
		Type = FifSearchDirectory;

		Ext = "*.c* *.h *.java";

		char Exe[MAX_PATH];
		LMakePath(Exe, sizeof(Exe), LGetExePath(), "..");
		Dir = Exe;
		
		MatchWord = false;
		MatchCase = false;
		SubDirs = true;
		
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
	AppWnd *App;
	GHistory *TypeHistory;
	GHistory *FolderHistory;
	bool OwnParams;

public:
	FindParams *Params;
	
	FindInFiles(AppWnd *app, FindParams *params = NULL);
	~FindInFiles();
	
	int OnNotify(LViewI *v, int f) override;
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
