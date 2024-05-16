#pragma once

#if HAS_LIBSSH

// If you don't have libssh.h then you might want to:
//		sudo apt-get install libssh-dev
#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include "DeEscape.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/ProgressView.h"

#define DEFAULT_PROMPT		"# "

#define IS_SSH_VER(maj, min)	((LIBSSH_VERSION_MAJOR < maj) \
								 || \
								 ((LIBSSH_VERSION_MAJOR == maj) && (LIBSSH_VERSION_MINOR >= min)))

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
	
	typedef std::function<CallbackResponse(const char *Msg, HostType Type)> KnownHostCallback;

protected:
	LCancel *Cancel = NULL;

protected:
	LTextLog *Log = NULL;
	ssh_session Ssh = NULL;
	LViewI *TxtLabel = NULL;
	LProgressView *Prog = NULL;
	bool Connected = false;
	bool OverideUnknownHost = false;
	KnownHostCallback HostCb;

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
					int Remain = (int)RemainingTime;
					int Hrs = Remain / 3600;
					Remain -= Hrs * 3600;
					int Mins = Remain / 60;
					Remain -= Mins * 60;
					int Secs = Remain;

					LString Msg, Time;
					if (!Hrs)
						Time.Printf("%im %is", Mins, Secs);
					else
						Time.Printf("%ih %im %is", Hrs, Mins, Secs);

					Msg.Printf(	"%s of %s\n"
								"%.1f%%, %s/s, %s",
								LFormatSize(i).Get(), LFormatSize(Length).Get(),
								(double)i * 100.0 / Length,
								LFormatSize((uint64_t)Rate).Get(),
								Time.Get());
					s->TxtLabel->Name(Msg);
					UpdateTs = Now;
				}
			}
			if (s->Prog)
				s->Prog->Value(i);
		}
	};

	struct SshConsole : public LStream
	{
		LSsh *s;
		ssh_channel channel;

		SshConsole(LSsh *ssh)
		{
			s = ssh;
			
			channel = ssh_channel_new(s->Ssh);
			if (!channel)
				return;

			int rc;
			do 
			{
				rc = ssh_channel_open_session(channel);
			}
			while (rc == SSH_AGAIN);
			if (rc != SSH_OK)
			{
				ssh_channel_free(channel);
				channel = NULL;
				return;
			}

			do
			{
				rc = ssh_channel_request_pty(channel);
			}
			while (rc == SSH_AGAIN);
			if (rc != SSH_OK)
				return;
 
			do
			{
				rc = ssh_channel_change_pty_size(channel, 240, 50);
			}
			while (rc == SSH_AGAIN);
			if (rc != SSH_OK)
				return;
 
			do
			{
				rc = ssh_channel_request_shell(channel);
			}
			while (rc == SSH_AGAIN);
			if (rc != SSH_OK)
				return;
		}

		~SshConsole()
		{
			ssh_channel_close(channel);
			ssh_channel_send_eof(channel);
			ssh_channel_free(channel);
		}

		ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override
		{
			return ssh_channel_read_nonblocking(channel, Ptr, (uint32_t)Size, false);
		}

		ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override
		{
			return ssh_channel_write(channel, Ptr, (uint32_t)Size);
		}
	};

public:
	LSsh(	KnownHostCallback hostCb,
			LTextLog *log,
			LCancel *cancel = NULL)
	{
		Cancel = cancel ? cancel : &LocalCancel;
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
		int Port = 22, Timeout = 60;
		Ssh = ssh_new();
		ssh_set_log_userdata(this);
		// ssh_set_log_callback(logging_callback);
		// ssh_set_log_level(SSH_LOG_PROTOCOL);
		auto r = ssh_options_set(Ssh, SSH_OPTIONS_HOST, Host);
		r = ssh_options_set(Ssh, SSH_OPTIONS_PORT, &Port);

		if (Cancel)
		{
			// If the user can cancel we need to go into non-blocking mode and poll 
			// the cancel object:
			ssh_set_blocking(Ssh, false);
		}
		else
		{
			r = ssh_options_set(Ssh, SSH_OPTIONS_TIMEOUT, &Timeout);
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

		if (Cancel)
		{
			while (!Cancel->IsCancelled())
			{
				r = ssh_connect(Ssh);
				if (r == SSH_AGAIN)
				{
					LSleep(10);
					continue;
				}

				LgiTrace("ssh_connect=%i\n", r);
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

	bool DownloadFile(const char *To, const char *From)
	{
		ssh_scp Scp = ssh_scp_new(Ssh, SSH_SCP_READ, From);
		if (!Scp)
		{
			Log->Print("%s:%i - ssh_scp_new failed.\n", _FL);
			return false;
		}

		auto r = ssh_scp_init(Scp);
		if (r == SSH_OK)
		{
			LFile local(To, O_WRITE);
			if (local.IsOpen())
			{
				size_t BufLen = 1 << 20;
				LArray<char> Buf(BufLen);
				uint64_t i = 0;
				IoProgress Meter(this);

				r = ssh_scp_pull_request(Scp);
				if (r == SSH_SCP_REQUEST_NEWFILE)
				{
					auto Len = ssh_scp_request_get_size64(Scp);
					ssh_scp_accept_request(Scp);

					local.SetSize(0);
					Log->Print("%s:%i - Downloading %s to %s.\n", _FL, From, To);
					Meter.SetLength(Len);

					while (!Cancel->IsCancelled() && i < Len)
					{
						auto rd = ssh_scp_read(Scp, Buf.AddressOf(), Buf.Length());
						if (rd <= 0)
							break;
						auto wr = local.Write(Buf.AddressOf(), rd);
						if (wr < rd)
						{
							Log->Print("%s:%i - Write failed.\n", _FL);
							break;
						}

						i += rd;
						Meter.Value(i);
					}

					bool status = i == Len;
					Log->Print("%s:%i - Download %s.\n", _FL, status ? "Successful" : "Error");
				}
			}
			else Log->Print("%s:%i - Can't open '%s'.\n", _FL, To);
		}
		else Log->Print("%s:%i - ssh_scp_init failed with %i\n", _FL, r);

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return true;
	}

	bool UploadFile(const char *To, const char *From)
	{
		bool Status = false;

		// Write the file...
		auto Parts = LString(To).RSplit("/", 1);
		ssh_scp Scp = ssh_scp_new(Ssh, SSH_SCP_WRITE, Parts[0]);
		if (!Scp)
		{
			Log->Print("%s:%i - ssh_scp_new failed.\n", _FL);
			return false;
		}

		auto r = ssh_scp_init(Scp);
		if (r == SSH_OK)
		{
			auto length = LFileSize(From);
			r = ssh_scp_push_file(Scp, Parts[1], length, 0644);
			if (r == SSH_OK)
			{
				LFile in(From, O_READ);
				if (in.IsOpen())
				{
					size_t BufLen = 128<<10;
					LArray<char> Buf(BufLen);
					int64_t i = 0;
					IoProgress Meter(this);
					Meter.SetLength(length);

					Log->Print("%s:%i - Writing %s.\n", _FL, LFormatSize(length).Get());
					for (i=0; !Cancel->IsCancelled() && i<length; )
					{
						auto rd = in.Read(Buf.AddressOf(), Buf.Length());
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

					Status = i==length;
					Log->Print("%s:%i - Upload: %s.\n", _FL, Status ? "Ok" : "Error");
				}
				else Log->Print("%s:%i - Can't open '%s'.\n", _FL, From);
			}
			else Log->Print("%s:%i - ssh_scp_push_file(%s,%" PRIi64 ") failed with: %i.\n", _FL, Parts[1].Get(), length, r);
		}
		else Log->Print("%s:%i - ssh_scp_init failed with %i\n", _FL, r);

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return Status;
	}

	LAutoPtr<LStream> CreateConsole()
	{
		LAutoPtr<LStream> con(new SshConsole(this));
		return con;
	}

	bool RunCommands(LStream *Console, const char **Cmds, const char *Prompt = DEFAULT_PROMPT)
	{
		LString Buf;
		int CmdIdx = 0;
		size_t logged = 0;

		while (Console && !Cancel->IsCancelled())
		{
			char Bytes[512];
			auto Rd = Console->Read(Bytes, sizeof(Bytes)-1);
			if (Rd > 0)
			{
				Bytes[Rd] = 0;
				Buf += LString(Bytes, Rd);
				DeEscape(Buf);

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
				auto PromptPos = Buf.Find(Prompt, LastNewLine >= 0 ? LastNewLine+1 : 0);
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
};

#endif