#pragma once
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include "DeEscape.h"

#define DEFAULT_PROMPT		"~#"

class LSsh : public LCancel
{
	friend struct IoProgress;
	friend struct SshConsole;

protected:
	GTextLog *Log;
	ssh_session Ssh;
	GViewI *TxtLabel;
	GProgress *Prog;
	bool Connected;

	struct IoProgress
	{
		LSsh *s;
		uint64_t Start;
		uint64_t Pos, Length;

		IoProgress(LSsh *ssh)
		{
			s = ssh;
			Start = LgiCurrentTime();
			Pos = Length = 0;
		}

		~IoProgress()
		{
			if (s->Prog)
				s->Prog->Colour(Pos == Length ? GProgress::cNormal : GProgress::cError);
			if (s->TxtLabel)
				s->TxtLabel->Name(NULL);
		}

		void SetLength(uint64_t len)
		{
			Length = len;
			if (s->Prog)
			{
				s->Prog->SetLimits(0, Length);
				s->Prog->Colour(GProgress::cPaused);
			}
		}
		
		void Value(uint64_t i)
		{
			Pos = i;
			if (s->TxtLabel)
			{
				double Sec = (double)(LgiCurrentTime()-Start)/1000.0;
				double Rate = (double)i / Sec;
				double TotalTime = Length / MAX(Rate, 1);
				double RemainingTime = TotalTime - Sec;
				int Remain = (int)RemainingTime;
				int Hrs = Remain / 3600;
				Remain -= Hrs * 3600;
				int Mins = Remain / 60;
				Remain -= Mins * 60;
				int Secs = Remain;

				GString Msg, Time;
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
			}
			if (s->Prog)
				s->Prog->Value(i);
		}
	};

	struct SshConsole : public GStream
	{
		LSsh *s;
		ssh_channel channel;

		SshConsole(LSsh *ssh)
		{
			s = ssh;
			
			channel = ssh_channel_new(s->Ssh);
			if (!channel)
				return;

			auto rc = ssh_channel_open_session(channel);
			if (rc != SSH_OK)
			{
				ssh_channel_free(channel);
				channel = NULL;
				return;
			}

			rc = ssh_channel_request_pty(channel);
			if (rc != SSH_OK)
				return;
 
			rc = ssh_channel_change_pty_size(channel, 240, 50);
			if (rc != SSH_OK)
				return;
 
			rc = ssh_channel_request_shell(channel);
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
	LSsh(GTextLog *log)
	{
		Log = log;
		Ssh = NULL;
		Prog = NULL;
		TxtLabel = NULL;
		Connected = false;
	}

	~LSsh()
	{
		ssh_free(Ssh);
		Ssh = NULL;
	}

	bool Open(const char *Host, const char *Username, const char *Password, bool PublicKey)
	{
		int Port = 22;
		Ssh = ssh_new();
		ssh_set_log_userdata(this);
		// ssh_set_log_callback(logging_callback);
		ssh_set_log_level(SSH_LOG_PROTOCOL);
		auto r = ssh_options_set(Ssh, SSH_OPTIONS_HOST, Host);
		r = ssh_options_set(Ssh, SSH_OPTIONS_PORT, &Port);

		r = ssh_connect(Ssh);
		if (r != SSH_OK)
		{
			Log->Print("%s:%i - ssh_connect failed.\n", _FL);
			return false;
		}
		// Log->Print("%s:%i - ssh_connect ok.\n", _FL);

		auto State = ssh_session_is_known_server(Ssh);
		// Log->Print("%s:%i - ssh_session_is_known_server=%i.\n", _FL, State);

		if (PublicKey)
			r = ssh_userauth_publickey_auto(Ssh, Username, Password);
		else if (Username && Password)
			r = ssh_userauth_password(Ssh, Username, Password);
		else
			Log->Print("%s:%i - No username and password.\n", _FL);

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
			GFile local(To, O_WRITE);
			if (local.IsOpen())
			{
				size_t BufLen = 1 << 20;
				GArray<char> Buf(BufLen);
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

					while (!IsCancelled() && i < Len)
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
		else Log->Print("%s:%i - ssh_scp_init failed.\n", _FL);

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return true;
	}

	bool UploadFile(const char *To, const char *From)
	{
		bool Status = false;

		// Write the file...
		auto Parts = GString(To).RSplit("/", 1);
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
				GFile in(From, O_READ);
				if (in.IsOpen())
				{
					size_t BufLen = 128<<10;
					GArray<char> Buf(BufLen);
					int64_t i = 0;
					IoProgress Meter(this);
					Meter.SetLength(length);

					Log->Print("%s:%i - Writing %s.\n", _FL, LFormatSize(length).Get());
					for (i=0; i<length; )
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
			else Log->Print("%s:%i - ssh_scp_push_file failed.\n", _FL);
		}
		else Log->Print("%s:%i - ssh_scp_init failed.\n", _FL);

		ssh_scp_close(Scp);
		ssh_scp_free(Scp);

		return Status;
	}

	GAutoPtr<GStream> CreateConsole()
	{
		GAutoPtr<GStream> con(new SshConsole(this));
		return con;
	}

	bool RunCommands(GStream *Console, const char **Cmds, const char *Prompt = DEFAULT_PROMPT)
	{
		GString Buf;
		int CmdIdx = 0;
		size_t logged = 0;

		while (Console && !IsCancelled())
		{
			char Bytes[512];
			auto Rd = Console->Read(Bytes, sizeof(Bytes)-1);
			if (Rd > 0)
			{
				Bytes[Rd] = 0;
				Buf += GString(Bytes, Rd);
				DeEscape(Buf);

				auto end = Buf.Find("\x1B");
				if (end < 0)
					end = Buf.Length();
				auto bytes = end - logged;
				if (bytes > 0)
				{
					auto has_esc = Buf.Find("\x1B", 0, logged+bytes);
					LgiAssert(has_esc < 0);

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
						GString c = GString(Cmds[CmdIdx]) + "\n";
						Console->Write(c, c.Length());
						CmdIdx++;
						Buf.Empty();
						logged = 0;
					}
					else return true; // We're done
				}
			}
			else if (Rd == 0)
				LgiSleep(10); // Wait for more data...
			else
				break;
		}

		return false;
	}
};


