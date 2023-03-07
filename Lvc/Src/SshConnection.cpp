#include "lgi/common/Lgi.h"

#include "Lvc.h"
#include "SshConnection.h"

#define TIMEOUT_PROMPT			1000
#define PROFILE_WaitPrompt		0
#define PROFILE_OnEvent			0

#define DEBUG_SSH_LOGGING		0
#if DEBUG_SSH_LOGGING
	#define SSH_LOG(...)		d->sLog.Log(__VA_ARGS__)
#else
	#define SSH_LOG(...)
#endif

//////////////////////////////////////////////////////////////////
SshConnection::SshConnection(LTextLog *log, const char *uri, const char *prompt) : LSsh(log), LEventTargetThread("SshConnection")
{
	auto Wnd = log->GetWindow();
	GuiHnd = Wnd->AddDispatch();
	Prompt = prompt;
	Host.Set(Uri = uri);
	d = NULL;

	LVariant Ret;
	LArray<LVariant*> Args;
	if (Wnd->CallMethod(METHOD_GetContext, &Ret, Args))
	{
		if (Ret.Type == GV_VOID_PTR)
			d = (AppPriv*) Ret.Value.Ptr;
	}
}

bool SshConnection::DetectVcs(VcFolder *Fld)
{
	LAutoPtr<LString> p(new LString(Fld->GetUri().sPath));
	TypeNotify.Add(Fld);
	return PostObject(GetHandle(), M_DETECT_VCS, p);
}

bool SshConnection::Command(VcFolder *Fld, LString Exe, LString Args, ParseFn Parser, ParseParams *Params)
{
	if (!Fld || !Exe || !Parser)
		return false;

	LAutoPtr<SshParams> p(new SshParams(this));
	p->f = Fld;
	p->Exe = Exe;
	p->Args = Args;
	p->Parser = Parser;
	p->Params = Params;
	p->Path = Fld->GetUri().sPath;

	return PostObject(GetHandle(), M_RUN_CMD, p);
}

LStream *SshConnection::GetConsole()
{
	if (!Connected)
	{
		auto r = Open(Host.sHost, Host.sUser, Host.sPass, true);
		Log->Print("Ssh: %s open: %i\n", Host.sHost.Get(), r);
	}
	if (Connected && !c)
	{
		c = CreateConsole();
		WaitPrompt(c);
	}
	return c;
}

class ProgressListItem : public LListItem
{
	int64_t v, maximum;

public:
	ProgressListItem(int64_t mx = 100) : maximum(mx)
	{
		v = 0;
	}

	int64_t Value() { return v; }
	void Value(int64_t val) { v = val; Update(); }
	void OnPaint(LItem::ItemPaintCtx &Ctx)
	{
		auto pDC = Ctx.pDC;
		pDC->Colour(Ctx.Back);
		pDC->Rectangle(&Ctx);
		
		auto Fnt = GetList()->GetFont();
		LDisplayString ds(Fnt, LFormatSize(v));
		Fnt->Transparent(true);
		Fnt->Colour(Ctx.Fore, Ctx.Back);
		ds.Draw(pDC, Ctx.x1 + 10, Ctx.y1 + ((Ctx.Y() - ds.Y()) >> 1));

		pDC->Colour(LProgressView::cNormal);
		int x1 = 120;
		int prog = Ctx.X() - x1;
		int x2 = (int) (v * prog / maximum);
		pDC->Rectangle(Ctx.x1 + x1, Ctx.y1 + 1, Ctx.x1 + x1 + x2, Ctx.y2 - 1);
	}
};

#if PROFILE_WaitPrompt
#define PROFILE(name) prof.Add(name)
#else
#define PROFILE(name)
#endif

LString LastLine(LString &input)
{
	#define Ws(ch) ( ((ch) == '\r') || ((ch) == '\n') || ((ch) == '\b') )
	char *e = input.Get() + input.Length();
	while (e > input.Get() && Ws(e[-1]))
		e--;
	char *s = e;
	while (s > input.Get() && !Ws(s[-1]))
		s--;
	return LString(s, e - s);
}

LString LastLine(LStringPipe &input)
{
	#define Ws(ch) ( ((ch) == '\r') || ((ch) == '\n') || ((ch) == '\b') )

	LString s, ln;
	input.Iterate([&s, &ln](auto ptr, auto bytes)
		{
			s = LString((char*)ptr, bytes) + s;
			auto end = s.Get() + s.Length();
			for (auto p = end - 1; p >= s.Get(); p--)
			{
				if (Ws(*p))
				{
					while (p < end && (*p == 0 || Ws(*p)))
						p++;
					ln = p;
					break;
				}
			}
			return ln.Get() == NULL;
		},
		true);

	// if (!ln.Get())
	// 	ln = s;
	LAssert(ln.Find("\n") < 0);
	DeEscape(ln);
	return ln;
}

bool SshConnection::WaitPrompt(LStream *con, LString *Data, const char *Debug)
{
	LStringPipe out(4 << 10);
	auto Ts = LCurrentTime();
	auto LastReadTs = Ts;
	ProgressListItem *Prog = NULL;
	#if PROFILE_WaitPrompt
	LProfile prof("WaitPrompt", 100);
	#endif
	size_t BytesRead = 0;
	bool CheckLast = true;

	while (!LSsh::Cancel->IsCancelled())
	{
		PROFILE("read");
		
		auto buf = out.GetBuffer();
		if (!buf.ptr)
		{
			LAssert(!"Alloc failed.");
			LgiTrace("WaitPrompt.%s alloc failed.\n", Debug);
			return false;
		}
		
		auto rd = con->Read(buf.ptr, buf.len);
		if (rd < 0)
		{
			// Error case
			if (Debug)
				LgiTrace("WaitPrompt.%s rd=%i\n", Debug, rd);
			return false;
		}

		if (rd > 0)
		{
			// Got some data... keep asking for more:
			LString tmp((char*)buf.ptr, rd);
SSH_LOG("waitPrompt data:", rd, tmp);

			BytesRead += rd;
			buf.Commit(rd);
			CheckLast = true;
			LastReadTs = LCurrentTime();
			continue;
		}

		if (LCurrentTime() - LastReadTs > 4000)
		{
			auto sz = out.GetSize();
SSH_LOG("waitPrompt out:", sz, out);
			auto last = LastLine(out);

			// Does the buffer end with a ':' on a line by itself?
			// Various version control CLI's do that to pageinate data.
			// Obviously we're not going to deal with that directly, 
			// but the developer will need to know that's happened.
			if (out.GetSize() > 2)
			{
				auto last = LastLine(out);
				if (last == ":")
					return false;
			}
		}

		if (!CheckLast)
		{
			// We've already checked the buffer for the prompt... 
			LSleep(10); // Don't use too much CPU
			continue;
		}

		PROFILE("LastLine");
		CheckLast = false;
		auto last = LastLine(out);
		LgiTrace("last='%s'\n", last.Get());
		PROFILE("matchstr");
		auto result = MatchStr(Prompt, last);
SSH_LOG("waitPrompt result:", result, Prompt, last);
		if (Debug)
		{
			LgiTrace("WaitPrompt.%s match='%s' with '%s' = %i\n", Debug, Prompt.Get(), last.Get(), result);
		}
		if (result)
		{
			if (Data)
			{
				PROFILE("data process");

				auto response = out.NewLStr();
				if (response)
				{
					DeEscape(response);

					// Strip first line off the start.. it's the command...
					// And the last line... it's the prompt
					auto start = response.Get();
					auto end = response.Get() + response.Length();
					while (start < end && *start != '\n')
						start++;
					while (start < end && (*start == '\n' || *start == 0))
						start++;
					while (end > start && end[-1] != '\n')
						end--;

					Data->Set(start, end - start);
				}
SSH_LOG("waitPrompt data:", *Data);
			}

			if (Debug)
				LgiTrace("WaitPrompt.%s Prompt data=%i\n", Debug, Data?(int)Data->Length():0);
			break;
		}

		auto Now = LCurrentTime();
		if (Now - Ts >= TIMEOUT_PROMPT)
		{
			if (!Prog && d->Commits)
			{
				Prog = new ProgressListItem(1 << 20);
				d->Commits->Insert(Prog);
			}
			if (Prog)
				Prog->Value(out.GetSize());

			Log->Print("...reading: %s\n", LFormatSize(BytesRead).Get());
			BytesRead = 0;
			Ts = Now;
		}
	}

	DeleteObj(Prog);
	return true;
}

bool SshConnection::HandleMsg(LMessage *m)
{
	if (m->Msg() != M_RESPONSE)
		return false;

	LAutoPtr<SshParams> u((SshParams*)m->A());
	if (!u || !u->c)
		return false;

	SshConnection &c = *u->c;
	AppPriv *d = c.d;
	if (!d)
		return false;

	if (u->Vcs) // Check the VCS type..
	{
		c.Types.Add(u->Path, u->Vcs);
		for (auto f: c.TypeNotify)
		{
			if (d->Tree->HasItem(f))
				f->OnVcsType(u->Output);
			else
				LgiTrace("%s:%i - Folder no longer in tree (recently deleted?).\n", _FL);
		}
	}
	else
	{
		if (d && d->Tree->HasItem(u->f))
			u->f->OnSshCmd(u);
		else
			LgiTrace("%s:%i - Folder no longer in tree (recently deleted?).\n", _FL);
	}

	return true;
}

LString PathFilter(LString s)
{
	auto parts = s.SplitDelimit("/");
	if
	(
		(parts[0].Equals("~") || parts[0].Equals("."))
		&&
		s(0) == '/'
	)
	{
		return s(1, -1).Replace(" ", "\\ ");
	}

	return s.Replace(" ", "\\ ");
}

#if PROFILE_OnEvent
#define PROF(name) prof.Add(name)
#else
#define PROF(name)
#endif

LMessage::Result SshConnection::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_DETECT_VCS:
		{
			LAutoPtr<LString> p;
			if (!ReceiveA(p, Msg))
			{
				LAssert(!"Incorrect param.");
				break;
			}

			LAutoPtr<SshParams> r(new SshParams(this));

			LString ls, out;
			LString::Array lines;
			VersionCtrl Vcs = VcNone;
			LString path = PathFilter(*p);
			LStream *con = GetConsole();
			if (!con)
			{
				r->Output = "Error: Failed to get console.";
				r->Vcs = VcError;
			}
			else
			{
				ls.Printf("find %s -maxdepth 1 -printf \"%%f\n\"\n", path.Get());
SSH_LOG("detectVcs:", ls);
				con->Write(ls, ls.Length());
				auto pr = WaitPrompt(con, &out);
				lines = out.SplitDelimit("\r\n");

				for (auto ln: lines)
				{
					if (ln.Equals(".svn"))
						Vcs = VcSvn;
					else if (ln.Equals("CVS"))
						Vcs = VcCvs;
					else if (ln.Equals(".hg"))
						Vcs = VcHg;
					else if (ln.Equals(".git"))
						Vcs = VcGit;
				}
			}

			r->Path = *p;

			printf("r->Output=%s\n", r->Output.Get());
			if (Vcs == VcError)
				;
			else if (Vcs != VcNone)
			{
				r->Vcs = Vcs;
				r->ExitCode = 0;
			}
			else
			{
				r->Vcs = VcError;
				r->Output.Printf("Error: no VCS detected.\n%s\n%s",
								ls.Get(),
								lines.Length() ? lines.Last().Get() : "#nodata");
			}

			PostObject(GuiHnd, M_RESPONSE, r);
			break;
		}
		case M_RUN_CMD:
		{
			#if PROFILE_OnEvent
			LProfile prof("OnEvent");
			#endif

			LAutoPtr<SshParams> p;
			if (!ReceiveA(p, Msg))
				break;

			PROF("get console");
			LString path = PathFilter(p->Path);
			LStream *con = GetConsole();
			if (!con)
				break;

			auto Debug = p->Params && p->Params->Debug;

			PROF("cd");
			LString cmd;
			cmd.Printf("cd %s\n", path.Get());
SSH_LOG(">>>> cd:", path);
			auto wr = con->Write(cmd, cmd.Length());
			PROF("cd wait");
			auto pr = WaitPrompt(con, NULL, Debug?"Cd":NULL);

			PROF("cmd");
			cmd.Printf("%s %s\n", p->Exe.Get(), p->Args.Get());
SSH_LOG(">>>> cmd:", cmd);
			if (Log)
				Log->Print("%s", cmd.Get());
			wr = con->Write(cmd, cmd.Length());
			PROF("cmd wait");
			pr = WaitPrompt(con, &p->Output, Debug?"Cmd":NULL);
			
			PROF("result");
			LString result;
			cmd = "echo $?\n";
SSH_LOG(">>>> result:", cmd);
			wr = con->Write(cmd, cmd.Length());
			PROF("result wait");
			pr = WaitPrompt(con, &result, Debug?"Echo":NULL);
			if (pr)
			{
				p->ExitCode = (int)result.Int();
				if (Log)
					Log->Print("... result=%i\n", p->ExitCode);
			}
			else if (Log)
				Log->Print("... result=failed\n");

			PostObject(GuiHnd, M_RESPONSE, p);
			break;
		}
		default:
		{
			LAssert(!"Unhandled msg.");
			break;
		}
	}
	return 0;
}

