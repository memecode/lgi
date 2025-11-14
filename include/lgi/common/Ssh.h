#pragma once

#if HAS_LIBSSH

// If you don't have libssh.h then you might want to:
// Linux:
//		sudo apt-get install libssh-dev
// Haiku:
//		pkgman install libssh_devel
#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include "lgi/common/RemoveAnsi.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/ProgressView.h"
#include "lgi/common/StructuredLog.h"

#define DEFAULT_PROMPTS		"# \t:~$ "

#define IS_SSH_VER(maj, min)	((LIBSSH_VERSION_MAJOR < maj) \
								 || \
								 ((LIBSSH_VERSION_MAJOR == maj) && (LIBSSH_VERSION_MINOR >= min)))


#if 1
	#define SSH_SLOG(...)			if (sLog) sLog->Log(__VA_ARGS__)
#else
	#define SSH_SLOG(...)
#endif

class LSsh
{
	friend struct IoProgress;
	friend struct SshConsole;

	LCancel LocalCancel;

public:
	enum HostType
	{
		SshKnownHost,
		SshHostChanged,
		SshHostOther,
		SshUnknown,
		SshNotFound,
		SshError,
	};
	
	enum CallbackResponse
	{
		SshDisconnect,
		SshConnect
	};
	
	enum TransferType
	{
		SshAuto, // Sftp first, and if that fails, scp
		SshSftp,
		SshScp,
	};
	
	constexpr static int NO_TIMEOUT = -1;
	typedef std::function<CallbackResponse(const char *Msg, HostType Type)> KnownHostCallback;

protected:
	LCancel *CancelObj = NULL;
	ssh_session Ssh = NULL;
	bool Connected = false;
	bool OverideUnknownHost = false;
	KnownHostCallback HostCb;
	int timeoutMs = NO_TIMEOUT;
	TransferType transferType = SshAuto;

	// Prompt detection
	LString prompt;
	uint64_t promptDetect = 0;

	// Logging and UI:
	LAutoPtr<LStructuredLog> sLog;
	LStream *Log = NULL;
	LViewI *TxtLabel = NULL;
	LProgressView *Prog = NULL;

	bool TimedOut(uint64_t startTs)
	{
		return	timeoutMs != NO_TIMEOUT &&
				(LCurrentTime() - startTs) > timeoutMs;
	}

	struct IoProgress
	{
		LSsh *s;
		uint64_t Start, UpdateTs = 0;
		uint64_t Pos, Length;

		IoProgress(LSsh *ssh);
		~IoProgress();

		void SetLength(uint64_t len);
		void Value(uint64_t i);
	};

	template<typename TChar>
	bool PromptDetect(bool &found, ssize_t &promptChar, TChar *ptr, size_t size)
	{
		if (!prompt)
		{
			// Detect the prompt characters
			auto now = LCurrentTime();
			if (!promptDetect)
			{
				promptDetect = now;
			}
			else if (now - promptDetect >= 300)
			{
				LString last2((char*)ptr + size - 2, 2);
				if (last2 == "> " ||
					last2 == "$ " ||
					last2 == "# ")
				{
					// Unix like system
					prompt = last2;
					found = true;

					SSH_SLOG("PromptDetect: got unix prompt");
					return true;
				}
				else if (size > 0 && ptr[size-1] == '>')
				{
					// Windows hopefully?
					prompt = ">";
					found = true;

					SSH_SLOG("PromptDetect: got windows prompt");
					return true;
				}

				// SSH_SLOG("PromptDetect not a prompt:", last2);
			}

			return false;
		}
		else
		{
			if (promptChar < 0)
			{
				LAssert(prompt.Length() > 0);
				promptChar = prompt.Length() - 1;
			}

			// This has to compare over memory block boundaries:
			for (auto i=size-1; i>=0; i--)
			{
				if (prompt[promptChar] != ptr[i])
					// Not found...
					return true;
						
				promptChar--;
				if (promptChar < 0)
				{
					found = true;
					return false;
				}
			}

			return true;
		}
	}

public:
	struct SshConsole : public LStream, public LCancel
	{
		LSsh *s;
		ssh_channel channel = NULL;

		SshConsole(LSsh *ssh, bool createShell);
		~SshConsole();

		bool CallMethod(const char *MethodName, LScriptArguments &Args) override;
		bool Create(bool createShell);
		void Empty();
		ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override;
		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
		bool Write(const LString &s);
		bool Command(LString cmd, LStream *out);
		LString Command(LString cmd);
	};

	// Object
	LSsh(	KnownHostCallback hostCb,
			LStream *log,
			LCancel *cancel = NULL);
	~LSsh();

	// Logging
	LString FmtError(LString context);
	void SetLog(LTextLog *log);
	LStructuredLog *GetStructLog() const;
	void SetStructLog(LAutoPtr<LStructuredLog> slog);
	
	// Configuration
	LCancel *GetCancel() const;
	int GetTimeout() const;
	void SetTimeout(int ms);
	TransferType GetTransferType();
	void SetTransferType(TransferType t);
	void SetOverideUnknownHost(bool b);
	
	struct ConfigHost
	{
		LString HostName, User;
		int Port = -1;
		bool UseKeychain;
	};
	ConfigHost ConfigHostLookup(const char *Host);

	// Connection
	bool Open(const char *Host, const char *Username, const char *Password, bool PublicKey, int Port = SSH_PORT);
	void Close();
	
	// File transfer
	LError DownloadFile(const char *To, const char *From);
	LError DownloadFile(LStream *To, const char *From);
	LError UploadFile(const char *To, const char *From);
	LError UploadFile(const char *To, LStream *From);

	// Console related:
	LAutoPtr<SshConsole> CreateConsole(bool createShell = true);
	bool RunCommands(LStream *Console, const char **Cmds, const char *PromptList = DEFAULT_PROMPTS);
	
	// Prompt related
	LString GetPrompt();
	void SetPrompt(const char *p);
	bool AtPrompt(LString &s);
	bool AtPrompt(LStringPipe &p);
	LString ReadToPrompt(LSsh::SshConsole *c, LStream *output = NULL, LCancel *cancel = NULL);
};

#endif
