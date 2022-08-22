#include "lgi/common/Lgi.h"

#include "Lvc.h"
#include "SshConnection.h"

#define TIMEOUT_PROMPT			1000

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

bool SshConnection::WaitPrompt(LStream *con, LString *Data, const char *Debug)
{
	char buf[1024];
	LString out;
	auto Ts = LCurrentTime();
	int64 Count = 0, Total = 0;
	ProgressListItem *Prog = NULL;

	while (!LSsh::Cancel->IsCancelled())
	{
		auto rd = con->Read(buf, sizeof(buf));
		if (rd < 0)
		{
			if (Debug)
				LgiTrace("WaitPrompt.%s rd=%i\n", Debug, rd);
			return false;
		}

		if (rd == 0)
		{
// SSH_LOG("waitPrompt no data.");
			LSleep(100);
			continue;
		}

		#if 0
		// De-null the data... FFS
		char *i, *o;
		for (i = buf, o = buf; i - buf < rd; i++) { if (*i) *o++ = *i; }
		rd = o - buf;
		#endif

		// Add new data to 'out'...
		LString newData(buf, rd);

		out += newData;

		Count += rd;
		Total += rd;

SSH_LOG("waitPrompt rd:", out);
		DeEscape(out);
SSH_LOG("waitPrompt DeEscape:", out);

		auto lines = out.SplitDelimit("\r\n\b");
		auto last = lines.Last();
		auto result = MatchStr(Prompt, last);
SSH_LOG("waitPrompt result:", result, Prompt, last, out);
		if (Debug)
		{
			LgiTrace("WaitPrompt.%s match='%s' with '%s' = %i\n", Debug, Prompt.Get(), last.Get(), result);
		}
		if (result)
		{
			if (Data)
			{
				auto start = out.Get();
				auto end = start + out.Length();
				auto second = start;
				while (second < end && second[-1] != '\n')
					second++;
				auto last = end;
				while (last > start && last[-1] != '\n')
					last--;

				*Data = out(second - start, last - start);
SSH_LOG("waitPrompt data:", *Data);
			}

			if (Debug)
				LgiTrace("WaitPrompt.%s Prompt line=%i data=%i\n", Debug, (int)lines.Length(), Data?(int)Data->Length():0);
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
				Prog->Value(Total);

			Log->Print("...reading: %s\n", LFormatSize(Count).Get());
			Count = 0;
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
				f->OnVcsType();
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

LMessage::Result SshConnection::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_DETECT_VCS:
		{
			LAutoPtr<LString> p;
			if (!ReceiveA(p, Msg))
				break;

			LString path = PathFilter(*p);
			LStream *con = GetConsole();
			if (!con)
				break;

			LString ls, out;
			ls.Printf("find %s -maxdepth 1 -printf \"%%f\n\"\n", path.Get());
SSH_LOG("detectVcs:", ls);
			con->Write(ls, ls.Length());
			auto pr = WaitPrompt(con, &out);
			auto lines = out.SplitDelimit("\r\n");

			VersionCtrl Vcs = VcNone;
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

			if (Vcs)
			{
				LAutoPtr<SshParams> r(new SshParams(this));
				r->Path = *p;
				r->Vcs = Vcs;
				PostObject(GuiHnd, M_RESPONSE, r);
			}
			else
			{
				Log->Print("Error: no VCS detected.\n%s\n%s\n", ls.Get(), lines.Last().Get());
			}
			break;
		}
		case M_RUN_CMD:
		{
			LAutoPtr<SshParams> p;
			if (!ReceiveA(p, Msg))
				break;

			LString path = PathFilter(p->Path);
			LStream *con = GetConsole();
			if (!con)
				break;

			auto Debug = p->Params && p->Params->Debug;

			LString cmd;
			cmd.Printf("cd %s\n", path.Get());
SSH_LOG(">>>> cd:", path);
			auto wr = con->Write(cmd, cmd.Length());
			auto pr = WaitPrompt(con, NULL, Debug?"Cd":NULL);

			cmd.Printf("%s %s\n", p->Exe.Get(), p->Args.Get());
SSH_LOG(">>>> cmd:", cmd);
			if (Log)
				Log->Print("%s", cmd.Get());
			wr = con->Write(cmd, cmd.Length());
			pr = WaitPrompt(con, &p->Output, Debug?"Cmd":NULL);
			
			LString result;
			cmd = "echo $?\n";
SSH_LOG(">>>> result:", cmd);
			wr = con->Write(cmd, cmd.Length());
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

