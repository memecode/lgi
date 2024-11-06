#pragma once

#include "lgi/common/TextLog.h"

class LDebugContext : public LDebugEvents
{
	class LDebugContextPriv *d;
	
public:
	LTree *Watch = NULL;
	LList *Locals = NULL;
	LList *CallStack = NULL;
	LList *Threads = NULL;
	LTextLog *ObjectDump = NULL;
	LTextLog *MemoryDump = NULL;
	LStream *DebuggerLog = NULL;
	LTextLog *Registers = NULL;
	std::function<void()> onFinished;

	// Object
	LDebugContext(AppWnd *App, class IdeProject *Proj, IdePlatform Platform, const char *Exe, const char *Args, bool RunAsAdmin, const char *Env, const char *InitDir);
	virtual ~LDebugContext();

	// Impl
	bool ParseFrameReference(const char *Frame, LAutoString &File, int &Line);
	void SetFrame(int Frame);
	void UpdateLocals();
	void UpdateWatches();
	void UpdateRegisters();
	void UpdateCallStack();
	void UpdateThreads();
	void SelectThread(int ThreadId, LDebugger::TStatusCb cb);
	bool DumpObject(const char *Var, const char *Val);
	
	// Ui events...
	bool OnCommand(int Cmd);
	void OnUserCommand(const char *Cmd);
	LMessage::Param OnEvent(LMessage *m);
	void OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex);
	void FormatMemoryDump(int WordSize, int Width, bool InHex);
	void Quit();
	
	// Debugger events...
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
	void OnState(bool Debugging, bool Running) override;
	void OnFileLine(const char *File, int Line, bool CurrentIp) override;
	void OnError(LString Str) override;
	void OnCrash(int Code) override;
	void Ungrab() override;
	void OnWarning(LString str) override;
};
