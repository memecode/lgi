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
	
	constexpr static int NO_TIMEOUT = -1;
	typedef std::function<CallbackResponse(const char *Msg, HostType Type)> KnownHostCallback;

protected:
	LCancel *CancelObj = NULL;
	ssh_session Ssh = NULL;
	bool Connected = false;
	bool OverideUnknownHost = false;
	KnownHostCallback HostCb;
	int timeoutMs = NO_TIMEOUT;

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

		IoProgress(LSsh *ssh)
		{
			s = ssh;
			Start = LCurrentTime();
			Pos = Length = 0;
		}

		~IoProgress()
		{
			if (s->Prog)
				s->Prog->Colour(Pos == Length ? LProgressView::cNormal : LProgressView::cError);
			if (s->TxtLabel)
				s->TxtLabel->Name(NULL);
		}

		void SetLength(uint64_t len)
		{
			Length = len;
			if (s->Prog)
			{
				s->Prog->SetRange(Length);
				s->Prog->Colour(LProgressView::cPaused);
			}
		}
		
		void Value(uint64_t i)
		{
			Pos = i;
			if (s->TxtLabel)
			{
				auto Now = LCurrentTime();
				if (Now - UpdateTs >= 500)
				{
					double Sec = (double)(LCurrentTime()-Start)/1000.0;
					double Rate = (double)i / Sec;
					double TotalTime = Length / MAX(Rate, 1);
					double RemainingTime = TotalTime - Sec;
					int DisplayTime = i == Length ? (int)TotalTime : (int)RemainingTime;
					int Hrs = DisplayTime / 3600;
					DisplayTime -= Hrs * 3600;
					int Mins = DisplayTime / 60;
					DisplayTime -= Mins * 60;
					int Secs = DisplayTime;

					LString Msg, Time;
					if (!Hrs)
						Time.Printf("%im %is", Mins, Secs);
					else
						Time.Printf("%ih %im %is", Hrs, Mins, Secs);

					if (i == Length)
					{
						Msg.Printf(	"Completed: %s\n"
									"%s/s in %s",
									LFormatSize(Length).Get(),
									LFormatSize((uint64_t)Rate).Get(),
									Time.Get());
					}
					else
					{
						Msg.Printf(	"%s of %s\n"
									"%.1f%%, %s/s, %s",
									LFormatSize(i).Get(), LFormatSize(Length).Get(),
									(double)i * 100.0 / Length,
									LFormatSize((uint64_t)Rate).Get(),
									Time.Get());
					}
					s->TxtLabel->Name(Msg);
					UpdateTs = Now;
				}
			}
			if (s->Prog)
				s->Prog->Value(i);
		}
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

		SshConsole(LSsh *ssh, bool createShell)
		{
			s = ssh;
			Create(createShell);
		}

		~SshConsole()
		{
			ssh_channel_close(channel);
			ssh_channel_send_eof(channel);
			ssh_channel_free(channel);
		}

		bool CallMethod(const char *MethodName, LScriptArguments &Args) override
		{
			auto id = LStringToDomProp(MethodName);
			switch (id)
			{
				case ObjCancel:
				{
					Cancel();
					return true;
				}
				default:
					break;
			}

			return false;
		}

		bool Create(bool createShell)
		{
			LAssert(!channel);
			channel = ssh_channel_new(s->Ssh);
			if (!channel)
				return false;

			int rc;
			do 
			{
				rc = ssh_channel_open_session(channel);
			}
			while (rc == SSH_AGAIN);
			if (rc != SSH_OK)
			{
				Empty();
				return false;
			}

			if (createShell)
			{
				do
				{
					rc = ssh_channel_request_pty(channel);
				}
				while (rc == SSH_AGAIN);
				if (rc != SSH_OK)
					return false;
 
				do
				{
					rc = ssh_channel_change_pty_size(channel, 240, 50);
				}
				while (rc == SSH_AGAIN);
				if (rc != SSH_OK)
					return false;
 
				do
				{
					rc = ssh_channel_request_shell(channel);
				}
				while (rc == SSH_AGAIN);
				if (rc != SSH_OK)
					return false;
			}

			return true;
		}

		void Empty()
		{
			if (channel)
			{
				ssh_channel_close(channel);
				ssh_channel_send_eof(channel);
				ssh_channel_free(channel);
				channel = NULL;
			}
		}

		ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override
		{
			if (channel)
				return ssh_channel_read_nonblocking(channel, Ptr, (uint32_t)Size, false);
			return 0;
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
		{
			if (channel)
				return ssh_channel_write(channel, Ptr, (uint32_t)Size);
			return 0;
		}

		bool Write(const LString &s)
		{
			if (channel)
				return ssh_channel_write(channel, s.Get(), (uint32_t)s.Length()) == s.Length();
			return false;
		}

		bool Command(LString cmd, LStream *out)
		{
			if (!out || !cmd)
				return false;
				
			if (!channel && !Create(false))
				return false;

			char buffer[512];
			int rc;
			do
			{
				rc = ssh_channel_request_exec(channel, cmd);
			}
			while (rc == SSH_AGAIN);
			if (rc != 0)
				return false;
			
			while ((rc = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0)
			{
			    auto wr = out->Write(buffer, rc);
			    if (wr < rc)
			    	break;
			}

			Empty(); // reset for the next..			
			return true;
		}

		LString Command(LString cmd)
		{
			LStringPipe p;
			Command(cmd, &p);			
			return p.NewLStr();
		}
	};

	LSsh(	KnownHostCallback hostCb,
			LStream *log,
			LCancel *cancel = NULL)
	{
		CancelObj = cancel ? cancel : &LocalCancel;
		Log = log;
		HostCb = hostCb;
	}

	~LSsh()
	{
		ssh_free(Ssh);
		Ssh = NULL;
	}

	void SetLog(LTextLog *log)
	{
		Log = log;
	}

	int GetTimeout() const
	{
		return timeoutMs;
	}

	void SetTimeout(int ms)
	{
		timeoutMs = ms;
	}
	
	struct ConfigHost
	{
		LString HostName, User;
		int Port = -1;
		bool UseKeychain;
	};

	ConfigHost ConfigHostLookup(const char *Host)
	{
		ConfigHost c;
		
		LFile::Path p(LSP_HOME);
		p += ".ssh";
		p += "config";
		if (!p.Exists())
			return c;
		
		LFile f(p, O_READ);
		if (!f)
			return c;
		
		auto lines = f.Read().Split("\n");
		bool inHost = false;
		for (auto l: lines)
		{
			auto p = l.SplitDelimit();
			if (p.Length() <= 1)
				continue;

			if (p[0].Equals("Host"))
			{
				inHost = p[1].Equals(Host);
			}
			else if (inHost)
			{
				if (p[0].Equals("HostName"))
					c.HostName = p[1];
				else if (p[0].Equals("Port"))
					c.Port = (int)p[1].Int();
				else if (p[0].Equals("User"))
					c.User = p[1];
				else if (p[0].Equals("UseKeychain"))
					c.UseKeychain = p[1].Equals("yes");
			}
		}
		
		return c;
	}
	
	void SetOverideUnknownHost(bool b)
	{
		OverideUnknownHost = b;
	}

	bool Open(const char *Host, const char *Username, const char *Password, bool PublicKey)
	{
		int Port = 22;
		
		if (sLog)
			sLog->Clear();		
		
		Ssh = ssh_new();
		ssh_set_log_userdata(this);
		// ssh_set_log_callback(logging_callback);
		// ssh_set_log_level(SSH_LOG_PROTOCOL);
		auto r = ssh_options_set(Ssh, SSH_OPTIONS_HOST, Host);
		r = ssh_options_set(Ssh, SSH_OPTIONS_PORT, &Port);

		if (CancelObj)
		{
			// If the user can cancel we need to go into non-blocking mode and poll 
			// the cancel object:
			ssh_set_blocking(Ssh, false);
		}
		else if (timeoutMs != NO_TIMEOUT)
		{
			int timeout = timeoutMs / 1000;
			r = ssh_options_set(Ssh, SSH_OPTIONS_TIMEOUT, &timeout);
		}

		// Check for a config entry...
		auto c = ConfigHostLookup(Host);
		if (c.HostName)
		{
			r = ssh_options_set(Ssh, SSH_OPTIONS_HOST, c.HostName);
			if (c.Port > 0)
				r = ssh_options_set(Ssh, SSH_OPTIONS_PORT, &c.Port);
				
			if (c.User)
				Username = c.User;
		}

		if (CancelObj)
		{
			auto startTs = LCurrentTime();
			while (!CancelObj->IsCancelled())
			{
				r = ssh_connect(Ssh);
				if (r == SSH_AGAIN)
				{
					if (TimedOut(startTs))
						break;

					LSleep(10);
					continue;
				}
				break;
			}
		}
		else
		{
			// Blocking connect:
			r = ssh_connect(Ssh);
		}

		if (r != SSH_OK)
		{
			ssh_free(Ssh);
			Ssh = NULL;

			if (Log)
				Log->Print("%s:%i - ssh_connect failed.\n", _FL);
			return false;
		}
		// Log->Print("%s:%i - ssh_connect ok.\n", _FL);

		#if IS_SSH_VER(0, 8)
		
		auto State = ssh_session_is_known_server(Ssh);
		if (State != SSH_KNOWN_HOSTS_OK &&
			!OverideUnknownHost)
		
		#else

		auto State = ssh_is_server_known(Ssh);
		if (State != SSH_SERVER_KNOWN_OK &&
			!OverideUnknownHost)
		
		#endif
		
		{
			// We don't know of the host... ask the user to confirm.
			CallbackResponse response = SshDisconnect;			
			if (HostCb)
			{
				switch (State)
				{
					#if IS_SSH_VER(0, 8)
					case SSH_KNOWN_HOSTS_CHANGED:
					#else
					case SSH_SERVER_KNOWN_CHANGED:
					#endif
						response = HostCb(	"The server key has changed. Either you are under attack or the administrator changed the key. You HAVE to warn the user about a possible attack.",
											SshHostChanged);
						break;
					#if IS_SSH_VER(0, 8)
					case SSH_KNOWN_HOSTS_OTHER:
					#else
					case SSH_SERVER_FOUND_OTHER:
					#endif
						response = HostCb(	"The server gave use a key of a type while we had an other type recorded. It is a possible attack.",
											SshHostOther);
						break;
					#if IS_SSH_VER(0, 8)
					case SSH_KNOWN_HOSTS_UNKNOWN:
					#else
					case SSH_SERVER_NOT_KNOWN:
					#endif
						response = HostCb(	"The server is unknown. You should confirm the public key hash is correct.",
											SshUnknown);
						break;
					#if IS_SSH_VER(0, 8)
					case SSH_KNOWN_HOSTS_NOT_FOUND:
					#else
					case SSH_SERVER_FILE_NOT_FOUND:
					#endif
						response = HostCb(	"Host list not found.",
											SshNotFound);
						break;
					default:
					#if IS_SSH_VER(0, 8)
					case SSH_KNOWN_HOSTS_ERROR:
					#else
					case SSH_SERVER_ERROR:
					#endif
						response = HostCb(	"There had been an error checking the host.",
											SshError);
						break;
				}
			}
			else
			{
				LgiTrace("%s:%i - ssh_session_is_known_server(%s@%s) returned %i\n",
						_FL,
						Username, Host,
						State);						
			}
			
			if (response != SshConnect)
				return false;
		}

		if (PublicKey)
		{
			do
			{
				r = ssh_userauth_publickey_auto(Ssh, Username, Password);
			}
			while (r == SSH_AUTH_AGAIN);

			if (r == SSH_AUTH_DENIED)
			{
				r = ssh_userauth_password(Ssh, Username, Password);
			}
		}
		else if (Username && Password)
		{
			do
			{
				r = ssh_userauth_password(Ssh, Username, Password);
			}
			while (r == SSH_AUTH_AGAIN);
		}
		else
		{
			Log->Print("%s:%i - No username and password.\n", _FL);
		}

		if (r != S_OK)
			return false;

		return Connected = true;			
	}

	void Close()
	{
		ssh_disconnect(Ssh);
		Connected = false;
	}

	LError DownloadFile(const char *To, const char *From)
	{
		LFile local(To, O_WRITE);
		if (!local.IsOpen())
		{
			Log->Print("%s:%i - Can't open '%s'.\n", _FL, To);
			return LError(local.GetError());
		}

		local.SetSize(0);
		return DownloadFile(&local, From);
	}

	#define ERR_MSG(...)                              \
		{                                             \
			auto msg = LString::Fmt(__VA_ARGS__);     \
			LgiTrace("%s:%i - %s\n", _FL, msg.Get()); \
			ret.Set(LErrorFuncFailed, msg);           \
		}

	LError DownloadFile(LStream *To, const char *From)
	{
		if (!To || !From)
			return LError(LErrorInvalidParam);

		ssh_scp Scp = ssh_scp_new(Ssh, SSH_SCP_READ, From);
		if (!Scp)
		{
			Log->Print("%s:%i - ssh_scp_new failed.\n", _FL);
			return LError(LErrorNoMem, "ssh_scp_new failed");
		}

		LError ret(LErrorNone);
		ssh_set_blocking(Ssh, true); // scp doesn't seem to like non-blocking
		auto r = ssh_scp_init(Scp);
		if (r != SSH_OK)
		{
			ERR_MSG("ssh_scp_init failed with %i", r);
		}
		else
		{
			size_t BufLen = 1 << 20;
			LArray<char> Buf(BufLen);
			uint64_t i = 0;
			IoProgress Meter(this);

			r = ssh_scp_pull_request(Scp);
			if (r == SSH_SCP_REQUEST_WARNING)
			{
				auto warn = ssh_scp_request_get_warning(Scp);
				ret.Set(LErrorPathNotFound, warn);
			}
			else if (r != SSH_SCP_REQUEST_NEWFILE)
			{
				ERR_MSG("ssh_scp_pull_request failed: %i", r);
			}
			else
			{
				auto Len = ssh_scp_request_get_size64(Scp);
				ssh_scp_accept_request(Scp);

				// Log->Print("%s:%i - Downloading %s...\n", _FL, From);
				Meter.SetLength(Len);

				while (!CancelObj->IsCancelled() && i < Len)
				{
					auto rd = ssh_scp_read(Scp, Buf.AddressOf(), Buf.Length());
					if (rd <= 0)
						break;
					auto wr = To->Write(Buf.AddressOf(), rd);
					if (wr < rd)
					{
						Log->Print("%s:%i - Write failed.\n", _FL);
						break;
					}

					i += rd;
					Meter.Value(i);
				}

				bool status = i == Len;
				// Log->Print("%s:%i - Download %s.\n", _FL, status ? "Successful" : "Error");
			}
		}

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return ret;
	}

	LError UploadFile(const char *To, const char *From)
	{
		if (!To || !From)
			return LError(LErrorInvalidParam);

		LFile in(From, O_READ);
		if (!in)
			return LError(in.GetError());

		return UploadFile(To, &in);
	}

	LError UploadFile(const char *To, LStream *From)
	{
		LError Err(LErrorFuncFailed);

		if (!To || !From)
			return LError(LErrorInvalidParam);

		// Write the file...
		auto Parts = LString(To).RSplit("/", 1);
		
		ssh_set_blocking(Ssh, true); // scp doesn't seem to like non-blocking
		ssh_scp Scp = ssh_scp_new(Ssh, SSH_SCP_WRITE, Parts[0]);
		if (!Scp)
		{
			Log->Print("%s:%i - ssh_scp_new failed.\n", _FL);
			return LError(LErrorNoMem);
		}

		auto r = ssh_scp_init(Scp);
		if (r == SSH_OK)
		{
			auto length = From->GetSize();
			r = ssh_scp_push_file(Scp, To, length, 0644);
			if (r == SSH_OK)
			{
				size_t BufLen = 128<<10;
				LArray<char> Buf(BufLen);
				int64_t i = 0;
				IoProgress Meter(this);
				Meter.SetLength(length);

				Log->Print("%s:%i - Writing %s.\n", _FL, LFormatSize(length).Get());
				for (i=0; !CancelObj->IsCancelled() && i<length; )
				{
					auto rd = From->Read(Buf.AddressOf(), Buf.Length());
					if (rd <= 0)
						break;
					r = ssh_scp_write(Scp, Buf.AddressOf(), rd);
					if (r != S_OK)
					{
						Log->Print("%s:%i - ssh_scp_write failed.\n", _FL);
						break;
					}

					i += rd;
					Meter.Value(i);
				}

				Meter.Value(i);
				Err = i == length ? LErrorNone : LErrorIoFailed;
				Log->Print("%s:%i - Upload: %s.\n", _FL, Err ? "Error" : "Ok");
			}
			else
			{
				Log->Print("%s:%i - ssh_scp_push_file(%s,%" PRIi64 ") failed with: %i.\n", _FL, To, length, r);
			}
		}
		else Log->Print("%s:%i - ssh_scp_init failed with %i\n", _FL, r);

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return Err;
	}

	LAutoPtr<SshConsole> CreateConsole(bool createShell = true)
	{
		return LAutoPtr<SshConsole>(new SshConsole(this, createShell));
	}
	
	LString GetPrompt()
	{
		return prompt;
	}

	void SetPrompt(const char *p)
	{
		if (p)
		{
			prompt = p;
		}
		else
		{
			promptDetect = 0;
			prompt.Empty();
		}
	}

	bool AtPrompt(LString &s)
	{
		bool found = false;
		ssize_t promptChar = -1;
		PromptDetect(found, promptChar, s.Get(), s.Length());
		return found;
	}

	bool AtPrompt(LStringPipe &p)
	{
		bool found = false;
		ssize_t promptChar = -1;
		auto lastLn = p.PeekLine(false);

		RemoveAnsi(lastLn);
		PromptDetect(found, promptChar, lastLn.Get(), lastLn.Length());

		return found;
	}

	LString ReadToPrompt(LSsh::SshConsole *c, LStream *output = NULL, LCancel *cancel = NULL)
	{
		LStringPipe p(1024);
		if (c)
		{
			auto startTs = LCurrentTime();
			char buf[1024];
			while (!CancelObj->IsCancelled())
			{
				auto rd = c->Read(buf, sizeof(buf));
				if (rd > 0)
				{
					SSH_SLOG("ReadToPrompt read:", rd, LString(buf, rd));

					p.Write(buf, rd);
					if (output)
						output->Write(buf, rd);
				}
				else if (AtPrompt(p))
				{
					break;
				}
				else
				{
					auto now = LCurrentTime();
					if (now - startTs > 2000)
					{
						auto lastLn = p.PeekLine(false);
						RemoveAnsi(lastLn);
						SSH_SLOG("ReadToPrompt not at prompt:", now - startTs, lastLn);
						startTs = now;
					}

					LSleep(1);
				}
			}
		}
		else
		{
			SSH_SLOG("ReadToPrompt: no console.\n");
		}

		return p.NewLStr();
	}

	bool RunCommands(LStream *Console, const char **Cmds, const char *PromptList = DEFAULT_PROMPTS)
	{
		LString Buf;
		int CmdIdx = 0;
		size_t logged = 0;
		auto Prompts = LString(PromptList).SplitDelimit("\t");

		while (Console && !CancelObj->IsCancelled())
		{
			char Bytes[512];
			auto Rd = Console->Read(Bytes, sizeof(Bytes)-1);
			if (Rd > 0)
			{
				Bytes[Rd] = 0;
				Buf += LString(Bytes, Rd);
				RemoveAnsi(Buf);

				auto end = Buf.Find("\x1B");
				if (end < 0)
					end = Buf.Length();
				auto bytes = end - logged;
				if (bytes > 0)
				{
					auto has_esc = Buf.Find("\x1B", 0, logged+bytes);
					LAssert(has_esc < 0);

					Log->Write(Buf.Get() + logged, bytes);
					logged += bytes;
				}

				// Wait for prompt
				auto LastNewLine = Buf.RFind("\n");
				ssize_t PromptPos = -1;
				for (auto p: Prompts)
				{
					PromptPos = Buf.Find(p, LastNewLine >= 0 ? LastNewLine+1 : 0);
					if (PromptPos >= 0)
						break;
				}
				if (PromptPos >= 0)
				{
					if (Cmds[CmdIdx])
					{
						// Send next command...
						LString c = LString(Cmds[CmdIdx]) + "\n";
						Console->Write(c, c.Length());
						CmdIdx++;
						Buf.Empty();
						logged = 0;
					}
					else return true; // We're done
				}
			}
			else if (Rd == 0)
				LSleep(10); // Wait for more data...
			else
				break;
		}

		return false;
	}
	
	LStructuredLog *GetStructLog() const
	{
		return sLog.Get();
	}
	
	void SetStructLog(LAutoPtr<LStructuredLog> slog)
	{
		sLog = slog;
	}	
};

#endif
