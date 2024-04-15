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

	// Object
	LDebugContext(AppWnd *App, class IdeProject *Proj, const char *Exe, const char *Args, bool RunAsAdmin, const char *Env, const char *InitDir);
	virtual ~LDebugContext();

	// Impl
	bool ParseFrameReference(const char *Frame, LAutoString &File, int &Line);
	bool SetFrame(int Frame);
	bool UpdateLocals();
	bool UpdateWatches();
	bool UpdateRegisters();
	void UpdateCallStack();
	void UpdateThreads();
	bool SelectThread(int ThreadId);
	bool DumpObject(const char *Var, const char *Val);
	bool OnBreakPoint(LDebugger::BreakPoint &b, bool Add);
	
	// Ui events...
	bool OnCommand(int Cmd);
	void OnUserCommand(const char *Cmd);
	LMessage::Param OnEvent(LMessage *m);
	void OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex);
	void FormatMemoryDump(int WordSize, int Width, bool InHex);
	
	// Debugger events...
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0);
	void OnState(bool Debugging, bool Running);
	void OnFileLine(const char *File, int Line, bool CurrentIp);
	void OnError(int Code, const char *Str);
	void OnCrash(int Code);
	void Ungrab();
	void OnWarning(LString str) override;
};
