#pragma once

#if HAS_LIBSSH

	#include "lgi/common/Ssh.h"
	#include "lgi/common/EventTargetThread.h"
	#include "lgi/common/Uri.h"

	class SshConnection : public LSsh, public LEventTargetThread
	{
		int GuiHnd;
		LUri Host;
		LAutoPtr<LStream> c;
		LString Uri, Prompt;
		AppPriv *d;

		LMessage::Result OnEvent(LMessage *Msg);
		LStream *GetConsole();
		bool WaitPrompt(LStream *c, LString *Data = NULL, const char *Debug = NULL);

	public:
		enum LoggingType
		{
			LogNone,
			LogInfo,
			LogDebug
		};

		LHashTbl<StrKey<char,false>,VersionCtrl> Types;
		LArray<VcFolder*> TypeNotify;
	
		SshConnection(LTextLog *log, const char *uri, const char *prompt);
		~SshConnection();
		bool DetectVcs(VcFolder *Fld);
		bool Command(VcFolder *Fld, LString Exe, LString Args, ParseFn Parser, ParseParams *Params, LoggingType Logging);
	
		// This is the GUI thread message handler
		static bool HandleMsg(LMessage *m);
	};

#else
	#ifdef _MSC_VER
	#pragma message("Building without libssh.")
	#else
	#warning "Building without libssh."
	#endif
#endif