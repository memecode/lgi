#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Uri.h"
#include "FtpThread.h"

static FtpThread *Ftp = 0;

class Line : public LListItem
{
	LColour c;

public:
	Line(const char *s, COLOUR col)
	{
		SetText(s);
		c.Set(col, 24);
	}

	void OnPaint(LItem::ItemPaintCtx &Ctx)
	{
		if (!Select())
			Ctx.Fore = c;
		LListItem::OnPaint(Ctx);
	}
};

class LogSock : public LSocket
{
	LStringPipe r, w;

	void Pop(LStringPipe &p, COLOUR c)
	{
		char s[256];
		while (p.Pop(s, sizeof(s)))
		{
			char *e = s + strlen(s) - 1;
			while (e > s && strchr(" \t\r\n", e[-1]))
			{
				e--;
				*e = 0;
			}
			
			if (Log)
			{
				LListItem *i;
				Log->Insert(i = new Line(s, c));
				i->ScrollTo();
			}
		}
	}

public:
	LList *Log;

	LogSock(LList *log)
	{
		Log = log;
	}

	void OnRead(char *Data, ssize_t Len)
	{
		r.Write(Data, Len);
		Pop(r, Rgb24(0, 0xc0, 0));
	}

	void OnWrite(const char *Data, ssize_t Len)
	{
		w.Write(Data, Len);
		Pop(w, Rgb24(0, 0, 0xc0));
	}

	void OnError(int ErrorCode, const char *ErrorDescription)
	{
		char s[256];
		snprintf(s, sizeof(s), "Error(%i): %s", ErrorCode, ErrorDescription);
		Log->Insert(new Line(s, Rgb24(255, 0, 0)));
	}
};

struct FtpConn
{
	LUri *Base;
	LogSock *Sock;
	IFtp Ftp;

	FtpConn(char *u)
	{
		Base = new LUri(u);
		Sock = 0;
	}

	void SetLog(LList *l)
	{
		if (Sock)
			Sock->Log = l;
	}

	bool Match (char *Uri)
	{
		LUri u(Uri);
		
		if (Base &&
			u.sHost && Base->sHost &&
			u.sUser && Base->sUser &&
			u.sPass && Base->sPass)
		{
			return	stricmp(u.sHost, Base->sHost) == 0 &&
					strcmp(u.sUser, Base->sUser) == 0 &&
					strcmp(u.sPass, Base->sPass) == 0;
		}

		return false;
	}

	bool Open(LList *Watch)
	{
		if (Base)
		{
			if
			(
				Ftp.Open
				(
					Sock = new LogSock(Watch),
					Base->sHost,
					Base->Port ? Base->Port : 21,
					Base->sUser,
					Base->sPass
				)
				==
				FO_Connected
			)
				return true;
		}
		return false;
	}
};

////////////////////////////////////////////////////////////////////////////////////
FtpCmd::FtpCmd(FtpCommand c, FtpCallback *cb)
{
	Cmd = c;
	Callback = cb;
	Uri = 0;
	File = 0;
	Watch = 0;
	Status = false;
	View = 0;
}

FtpCmd::~FtpCmd()
{
	DeleteArray(Uri);
	DeleteArray(File);
	Dir.DeleteObjects();
}

void FtpCmd::Error(const char *e)
{
	if (Watch)
	{
		Watch->Insert(new Line(e, Rgb24(255, 0, 0)));
	}
}

////////////////////////////////////////////////////////////////////////////////////
#define M_FTP_CMD			(M_USER + 0x4000)

class FtpRedir : public LWindow
{
public:
	FtpRedir()
	{
		LRect r(-20001, -20001, -20000, -20000);
		SetPos(r);
		Attach(0);
	}

	LMessage::Result OnEvent(LMessage *m)
	{
		if (m->Msg() == M_FTP_CMD)
		{
			FtpCmd *c = (FtpCmd*) m->A();
			c->Callback->OnCmdComplete(c);
			DeleteObj(c);
		}

		return LWindow::OnEvent(m);
	}
};

struct FtpThreadPriv : public LMutex
{
	bool Loop;
	LArray<FtpConn*> Conn;
	LArray<FtpCmd*> Cmds;
	FtpRedir *Redir;

	FtpThreadPriv() : LMutex("FtpThreadPriv")
	{
		Loop = true;
		Redir = new FtpRedir;
	}

	~FtpThreadPriv()
	{
		DeleteObj(Redir);
	}

	FtpConn *GetConn(char *Uri, LList *Watch = 0)
	{
		// Look for existing connection
		for (int i=0; i<Conn.Length(); i++)
		{
			if (Conn[i]->Match(Uri))
			{
				Conn[i]->SetLog(Watch);
				return Conn[i];
			}
		}

		// Create new connection
		FtpConn *n = new FtpConn(Uri);
		if (n)
		{
			if (n->Open(Watch))
			{
				Conn.Add(n);
			}
			else DeleteObj(n);

			return n;
		}

		return 0;
	}

	void FreeConn(FtpConn *c)
	{
		Conn.Delete(c);
		DeleteObj(c);
	}
};

FtpThread::FtpThread() : LThread("FtpThread")
{
	d = new FtpThreadPriv;
	Run();
}

FtpThread::~FtpThread()
{
	d->Loop = false;

	while (!IsExited())
	{
		LSleep(1);
	}
	
	DeleteObj(d);
}

void FtpThread::Post(FtpCmd *Cmd)
{
	LMutex::Auto Lock(d, _FL);
	d->Cmds.Add(Cmd);
}

int FtpThread::Main()
{
	while (d->Loop)
	{
		FtpCmd *c = 0;
		if (d->LockWithTimeout(50, _FL))
		{
			if (d->Cmds.Length())
			{
				c = d->Cmds[0];
				d->Cmds.DeleteAt(0, true);
			}
			d->Unlock();
		}

		if (c)
		{
			switch (c->Cmd)
			{
				case FtpList:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							if (Conn->Base->sPath)
							{
								if (!Conn->Ftp.SetDir(Conn->Base->sPath))
								{
									c->Error("Couldn't set path.");
								}
							}

							c->Status = Conn->Ftp.ListDir(c->Dir);
							if (!c->Status)
								c->Error("Failed to list folder.");
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				case FtpRead:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							LUri u(c->Uri);
							if (u.sPath)
							{
								char p[256];
								strcpy_s(p, sizeof(p), u.sPath);
								char *d = strrchr(p, '/');
								if (d) *d = 0;

								if (!Conn->Ftp.SetDir(p))
								{
									c->Error("Couldn't set path.");
									break;
								}
							}

							char r[256], *d = strrchr(c->Uri, '/');
							LMakePath(r, sizeof(r), LGetSystemPath(LSP_TEMP), d + 1);
							if (LFileExists(r))
							{
								FileDev->Delete(r, false);
							}
							
							IFtpEntry e;
							e.Size = -1;
							e.Name = d + 1;
							c->Status = Conn->Ftp.DownloadFile(r, &e);
							if (c->Status)
							{
								c->File = NewStr(r);
							}
							else
							{
								c->Error("Download failed.");
							}
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				case FtpWrite:
				{
					if (c->Uri)
					{
						FtpConn *Conn = d->GetConn(c->Uri, c->Watch);
						if (Conn)
						{
							LUri u(c->Uri);
							if (u.sPath)
							{
								char p[256];
								strcpy_s(p, sizeof(p), u.sPath);
								char *d = strrchr(p, '/');
								if (d) *d = 0;

								if (!Conn->Ftp.SetDir(p))
								{
									c->Error("Couldn't set path.");
									break;
								}
							}

							if (LFileExists(c->File))
							{
								char *d = strrchr(c->File, DIR_CHAR);
								c->Status = Conn->Ftp.UploadFile(c->File, d + 1);
								if (!c->Status)
								{
									c->Error("Upload failed.");
								}
							}
							else c->Error("Source file doesn't exist");
						}
						else c->Error("Failed to connect.");
					}
					break;
				}
				default:
				{
					LAssert(!"Not implemented.");
					break;
				}
			}

			if (c->Callback)
			{
				d->Redir->PostEvent(M_FTP_CMD, (LMessage::Param)c);
			}
			else DeleteObj(c);
		}
		else LSleep(1);
	}

	return 0;
}

FtpThread *GetFtpThread()
{
	if (!Ftp)
		Ftp = new FtpThread;
	return Ftp;
}

void ShutdownFtpThread()
{
	DeleteObj(Ftp);
}
