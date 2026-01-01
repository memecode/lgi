#pragma once

#if HAS_LIBSSH

	#include "lgi/common/Ssh.h"
	#include "lgi/common/EventTargetThread.h"
	#include "lgi/common/Uri.h"

	class SshConnection : public LSsh, public LEventTargetThread
	{
		int GuiHnd;
		LUri Host;
		LAutoPtr<SshConsole> console;
		LString Uri, Prompt;
		AppPriv *d;

		LMessage::Result OnEvent(LMessage *Msg);
		LStream *GetStream();
		SshConsole *GetConsole();
		bool WaitPrompt(LStream *c, LString *Data = NULL, const char *Debug = NULL, int timeoutMs = 0);

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

		using TConsoleCallback = std::function<void(SshConsole&)>;
		bool WithConsole(TConsoleCallback cb);

		// This is the GUI thread message handler
		static bool HandleMsg(LMessage *m);



		struct ProcessOutput
		{
			LString out;
			int code;
		};

		ProcessOutput RunCmd(LSsh::SshConsole &console, LString cmd)
		{
			console.Write(cmd + "\n");
			LStringPipe out;
			char buf[512];
			while (!console.s->AtPrompt(out))
			{
				auto rd = console.Read(buf, sizeof(buf));
				if (rd > 0) out.Write(buf, rd);
				else LSleep(10);
			}
			ProcessOutput p;
			p.out = out.NewLStr().Replace("\r");
			RemoveAnsi(p.out);
			// strip first and last lines
			auto first = p.out.Find("\n");
			auto last = p.out.RFind("\n");
			if (first >= 0 && last > first)
				p.out = p.out(first + 1, last);

			console.Write("echo $?\n");
			while (!console.s->AtPrompt(out))
			{
				auto rd = console.Read(buf, sizeof(buf));
				if (rd > 0) out.Write(buf, rd);
				else LSleep(10);
			}

			auto result = out.NewLStr();
			RemoveAnsi(result);
			auto lines = result.Replace("\r").SplitDelimit("\n");
			p.code = lines.Length() > 1 ? (int)lines[1].Int() : -1;
			return p;
		};
	};

#else
	#ifdef _MSC_VER
	#pragma message("Building without libssh.")
	#else
	#warning "Building without libssh."
	#endif
#endif