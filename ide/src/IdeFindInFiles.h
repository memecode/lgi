#ifndef _FIND_IN_FILES_H_
#define _FIND_IN_FILES_H_

#include "lgi/common/EventTargetThread.h"
#include "lgi/common/SystemIntf.h"
#include "History.h"

class FindParams : public SystemIntf::FindParams
{
public:
	FindParams(const FindParams *Set = NULL)
	{
		Type = SearchDirectory;

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
		
		Paths.Length(p->Paths.Length());
		for (unsigned i=0; i<Paths.Length(); i++)
			Paths[i] = p->Paths.ItemAt(i).Get();
		
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

	void OnFolderSelected(LString fld);

public:
	FindParams *Params = NULL;
	
	FindInFiles(AppWnd *app, FindParams *params = NULL);
	~FindInFiles();
	
	int OnNotify(LViewI *v, const LNotification &n) override;
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
