#include <stdlib.h>

#include "Lgi.h"
#include "LgiResEdit.h"
#include "resdefs.h"
#include "GCombo.h"
#include "LgiRes_Dialog.h"
#include "LgiRes_Menu.h"

#define SetCtrlFocus(id)	{ GViewI *v = FindControl(id); if (v) v->Focus(true); }
enum Msgs
{
	M_SEARCH = M_USER + 100,
};

class Result : public LListItem
{
public:
	AppWnd *App;
	ResStringGroup *Grp;
	ResMenuItem *Menu;
	ResDialog *Dialog;
	ResDialogCtrl *Ctrl;
	ResString *Str;
	GLanguageId Lang;
	
	Result(AppWnd *app, ResString *s, GLanguageId l = 0)
	{
		App = app;
		Grp = 0;
		Menu = 0;
		Dialog = 0;
		Ctrl = 0;
		Str = s;
		Lang = l;
	}
	
	const char *GetText(int i)
	{
		switch (i)
		{
			case 0:
			{
				if (Menu) return "MenuItem";
				else if (Dialog) return "Control";
				return "String";
				break;
			}
			case 1:
			{
				return Str ? Str->GetDefine() : 0;
				break;
			}
			case 2:
			{
				if (Str)
				{
					return Str->Get(Lang);
				}
				break;
			}
			case 3:
			{
				return Lang;
				break;
			}
		}
		
		return 0;
	}	

	void OnMouseClick(GMouse &m)
	{
		if (App && m.Left() && m.Down())
			App->GotoObject(Str, Grp, Dialog, Menu, Ctrl);
	}
};

bool SearchThread::HasContent(char *s)
{
	while (s && *s)
	{
		if (((*s & 0x80) != 0) || isalpha(*s))
		{
			return true;
		}
		s++;
	}

	return false;
}

Result *SearchThread::Test(ResString *s)
{
	if (!s)
		return NULL;

	if (Params.Text)
	{
		if (Params.LimitToDefine &&
			!Params.LimitToText &&
			!Params.InLang)
		{
			if (s->GetDefine())
			{
				if (stristr(s->GetDefine(), Params.Text))
					return new Result(App, s);
			}
		}

		if (Params.LimitToDefine)
			return NULL;

		if (Params.InLang)
		{
			char *Txt = s->Get(Params.InLang);
			if (Txt && stristr(Txt, Params.Text))
				return new Result(App, s, Params.InLang);
		}
		else
		{
			for (auto t: s->Items)
			{
				if (t->GetStr() && stristr(t->GetStr(), Params.Text))
					return new Result(App, s, t->GetLang());
			}
		}

		if (Params.LimitToText)
			return NULL;
	}

	if (Params.NotInLang)
	{
		if (s->Items.Length() > 0 &&
			!s->Get(Params.NotInLang) &&
			HasContent(s->Get("en")))
			return new Result(App, s, Params.NotInLang);
	}

	if (Params.Ref != INVALID_INT)
	{
		if (s->GetRef() == Params.Ref)
			return new Result(App, s);
	}

	if (Params.CtrlId != INVALID_INT)
	{
		if (s->GetId() == Params.CtrlId)
			return new Result(App, s);
	}

	return NULL;
}

Result *SearchThread::Test(ResMenuItem *mi)
{
	Result *r = Test(mi->GetStr());
	if (r)
		return r;
	
	if (ValidStr(Params.Text) && mi->Shortcut())
	{
		if (stristr(mi->Shortcut(), Params.Text))
		{
			if ((r = new Result(App, mi->GetStr(), Params.InLang)))
			{
				r->Menu = mi;
				return r;
			}				
		}
	}
	
	return NULL;
}

SearchThread::SearchThread(AppWnd *app, LList *results) :
	App(app), Results(results), GEventTargetThread("SearchThread")
{
	App->ListObjects(Res);
	Run();
}

void SearchThread::Search(SearchParams &p)
{
	Cancel(true);
	PostEvent(M_SEARCH, (GMessage::Param)new SearchParams(p));
}

GMessage::Result SearchThread::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_SEARCH:
		{
			GAutoPtr<SearchParams> p;
			if (ReceiveA(p, Msg))
				Params = *p;
			else
				return 0;
			break;
		}
		default:
			return 0;
	}

	Cancel(false);
	Results->Empty();

	for (auto r: Res)
	{
		if (IsCancelled() || Results->Length() > 100)
			break;

		if (r->IsStringGroup())
		{
			List<ResString>::I it = r->IsStringGroup()->GetStrs()->begin();
			for (ResString *s = *it; s && !IsCancelled(); s = *++it)
			{
				Result *Res = Test(s);
				if (Res)
				{
					Res->Grp = r->IsStringGroup();
					Results->Insert(Res);
				}
			}
		}
		else if (r->IsDialog())
		{
			List<ResDialogCtrl> Ctrls;
			r->IsDialog()->EnumCtrls(Ctrls);
			for (auto c: Ctrls)
			{
				if (IsCancelled())
					break;

				Result *Res = Test(c->GetStr());
				if (Res)
				{
					Res->Ctrl = c;
					Res->Dialog = r->IsDialog();
					Results->Insert(Res);
				}
			}
		}
		else if (r->IsMenu())
		{
			List<ResMenuItem> Items;
			r->IsMenu()->EnumItems(Items);
			for (auto c: Items)
			{
				if (IsCancelled())
					break;

				Result *Res = Test(c);
				if (Res)
				{
					Res->Menu = c;
					Results->Insert(Res);
				}
			}
		}
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////
// Search window

void FillLangs(GViewI *v, int id, List<GLanguage> &l)
{
	GCombo *c;
	if (!v->GetViewById(id, c))
		return;
	for (auto li: l)
		c->Insert(li->Name);
}

Search::Search(AppWnd *app)
{
	SetParent(App = app);

	List<GLanguage> l;
	List<Resource> Res;
	if (App->ListObjects(Res))
	{
		for (auto r: Res)
		{
			ResStringGroup *g = r->IsStringGroup();
			if (!g) continue;

			for (int i=0; i<g->GetLanguages(); i++)
			{
				GLanguage *Lang = g->GetLanguage(i);
				if (!Lang) continue;

				bool Has = false;
				for (auto i: l)
				{
					if (i == Lang)
					{
						Has = true;
						break;
					}
				}
				if (!Has)
					l.Insert(Lang);
			}
		}
	}

	#if NEW_UI
	if (LoadFromResource(IDD_FIND))
	#else
	if (LoadFromResource(IDD_SEARCH))
	#endif
	{
		MoveSameScreen(App);
		FillLangs(this, IDC_LANG, l);
		FillLangs(this, IDC_NOT_LANG, l);

		#if NEW_UI
		LList *lst;
		if (GetViewById(IDC_RESULTS, lst))
		{
			lst->AddColumn("Object", 60);
			lst->AddColumn("Define", 120);
			lst->AddColumn("Text", 300);
			lst->AddColumn("Language", 60);

			Thread.Reset(new SearchThread(App, lst));
		}
		else LgiAssert(0);
		#endif

		OnCheck();
		
		SetCtrlFocus(IDC_TEXT);
	}
}

void Search::OnCheck()
{
	SetCtrlEnabled(IDC_LANG, GetCtrlValue(IDC_IN_LANG));
	SetCtrlEnabled(IDC_NOT_LANG, GetCtrlValue(IDC_NOT_IN_LANG));
	#if !NEW_UI
	SetCtrlEnabled(IDC_DEFINE, GetCtrlValue(IDC_IN_DEFINES));
	SetCtrlEnabled(IDC_TEXT, GetCtrlValue(IDC_IN_TEXT));
	SetCtrlEnabled(IDC_REF, GetCtrlValue(IDC_IN_REF));
	SetCtrlEnabled(IDC_CTRL, GetCtrlValue(IDC_IN_CTRL));
	#endif
}

int Search::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		#if NEW_UI
		case IDC_TEXT:
		{
			if (!Thread) break;
			Text = c->Name();
			Thread->Search(*this);
			break;
		}
		case IDC_IN_TEXT:
		{
			if (!Thread) break;
			LimitToText = c->Value() != 0;
			Thread->Search(*this);
			break;
		}
		case IDC_IN_DEFINES:
		{
			if (!Thread) break;
			LimitToDefine = c->Value() != 0;
			Thread->Search(*this);
			break;
		}
		case IDC_IN_LANG:
		{
			SetCtrlFocus(IDC_LANG);
			OnCheck();
			// fall thru
		}
		case IDC_LANG:
		{
			InLang = NULL;
			if (!Thread) break;
			if (GetCtrlValue(IDC_IN_LANG))
			{
				auto FullName = GetCtrlName(IDC_LANG);
				GLanguage *l = GFindLang("", FullName);
				InLang = l ? l->Id : NULL;
			}
			Thread->Search(*this);
			break;
		}
		case IDC_NOT_IN_LANG:
		{
			SetCtrlFocus(IDC_NOT_LANG);
			OnCheck();
			// fall thru
		}
		case IDC_NOT_LANG:
		{
			NotInLang = NULL;
			if (!Thread) break;
			if (GetCtrlValue(IDC_NOT_IN_LANG))
			{
				GLanguage *l = GFindLang("", GetCtrlName(IDC_NOT_LANG));
				NotInLang = l ? l->Id : NULL;
			}
			Thread->Search(*this);
			break;
		}
		case IDC_REF:
		{
			Ref = (int) c->Value();
			Thread->Search(*this);
			break;
		}
		case IDC_CTRL:
		{
			Ref = (int) c->Value();
			Thread->Search(*this);
			break;
		}
		#else
		case IDC_IN_TEXT:
		case IDC_IN_DEFINES:
		case IDC_IN_REF:
		case IDC_IN_CTRL:
		case IDC_IN_LANG:
		case IDC_NOT_IN_LANG:
		{
			OnCheck();

			switch (c->GetId())
			{
				case IDC_IN_TEXT:
					SetCtrlFocus(IDC_TEXT);
					break;
				case IDC_IN_DEFINES:
					SetCtrlFocus(IDC_DEFINE);
					break;
				case IDC_IN_REF:
					SetCtrlFocus(IDC_REF);
					break;
				case IDC_IN_CTRL:
					SetCtrlFocus(IDC_CTRL);
					break;
				case IDC_IN_LANG:
					SetCtrlFocus(IDC_LANG);
					break;
				case IDC_NOT_IN_LANG:
					SetCtrlFocus(IDC_NOT_LANG);
					break;
			}
			break;
		}
		case IDOK:
		{
			if (GetCtrlValue(IDC_IN_TEXT))
			{
				char *t = GetCtrlName(IDC_TEXT);
				if (ValidStr(t))
					Text = NewStr(t);
			}

			if (GetCtrlValue(IDC_IN_DEFINES))
			{
				char *t = GetCtrlName(IDC_DEFINE);
				if (ValidStr(t))
					Define = NewStr(t);
			}
			
			if (GetCtrlValue(IDC_IN_LANG))
			{
				GLanguage *l = GFindLang(0, GetCtrlName(IDC_LANG));
				if (l) InLang = l->Id;
			}
			
			if (GetCtrlValue(IDC_NOT_IN_LANG))
			{
				GLanguage *l = GFindLang(0, GetCtrlName(IDC_NOT_LANG));
				if (l) NotInLang = l->Id;
			}
			
			if (GetCtrlValue(IDC_IN_REF) && GetCtrlName(IDC_REF))
			{
				Ref = GetCtrlValue(IDC_REF);
			}

			if (GetCtrlValue(IDC_IN_CTRL) && GetCtrlName(IDC_CTRL))
			{
				CtrlId = GetCtrlValue(IDC_CTRL);
			}
			// fall thru			
		}
		#endif
		case IDCANCEL:
		{
			EndModal(c->GetId() == IDOK);
			break;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// Results window
class ResultsPrivate
{
public:
	AppWnd *App;
	char *Text;
	char *Define;
	GLanguageId InLang;
	GLanguageId NotInLang;
	int RefId;
	int CtrlId;
	bool Searching;
	bool Running;
	LList *Lst;

	bool HasContent(char *s)
	{
		while (s && *s)
		{
			if (((*s & 0x80) != 0) || isalpha(*s))
			{
				return true;
			}
			s++;
		}

		return false;
	}

	Result *Test(ResString *s)
	{
		Result *r = 0;
		
		if (s)
		{
			if (ValidStr(Text))
			{
				if (InLang)
				{
					char *Txt = s->Get(InLang);
					if (Txt && stristr(Txt, Text))
					{
						r = new Result(App, s, InLang);
					}
				}
				else
				{
					for (auto t: s->Items)
					{
						if (t->GetStr() && stristr(t->GetStr(), Text))
						{
							r = new Result(App, s, t->GetLang());
							break;
						}
					}
				}
			}
			if (ValidStr(Define) && s->GetDefine())
			{
				if (stristr(s->GetDefine(), Define))
				{
					r = new Result(App, s);
				}
			}
			if (!r && NotInLang)
			{
				if (s->Items.Length() > 0 &&
					!s->Get(NotInLang) &&
					HasContent(s->Get("en")))
				{
					r = new Result(App, s, NotInLang);
				}
			}
			if (!r && RefId)
			{
				if (s->GetRef() == RefId)
				{
					r = new Result(App, s);
				}
			}
			if (!r && CtrlId)
			{
				if (s->GetId() == CtrlId)
				{
					r = new Result(App, s);
				}
			}
		}
		return r;
	}

	Result *Test(ResMenuItem *mi)
	{
		Result *r = Test(mi->GetStr());
		if (r)
			return r;
		
		if (ValidStr(Text) && mi->Shortcut())
		{
			if (stristr(mi->Shortcut(), Text))
			{
				if ((r = new Result(App, mi->GetStr(), InLang)))
				{
					r->Menu = mi;
					return r;
				}				
			}
		}
		
		return NULL;
	}
};

Results::Results(AppWnd *app, Search *params)
{
	d = new ResultsPrivate;
	d->App = app;
	d->Text = NewStr(params->Text);
	#if !NEW_UI
	d->Define = NewStr(params->Define);
	#endif
	d->InLang = params->InLang;
	d->NotInLang = params->NotInLang;
	d->RefId = params->Ref;
	d->CtrlId = params->CtrlId;
	d->Searching = false;
	d->Running = false;
	d->Lst = 0;
	
	GLgiRes r;
	GRect p;
	GAutoString n;
	if (r.LoadFromResource(IDD_RESULTS, this, &p, &n))
	{
		SetPos(p);
		MoveToCenter();
		if (n[0]) Name(n);

		if (Attach(0))
		{
			AttachChildren();
			Visible(true);

			GetViewById(IDC_LIST, d->Lst);
			
			List<Resource> Res;
			d->App->ListObjects(Res);
			d->Searching = true;
			for (auto r: Res)
			{
				if (!d || !d->Searching)
					break;

				if (r->IsStringGroup())
				{
					List<ResString>::I it = r->IsStringGroup()->GetStrs()->begin();
					for (ResString *s = *it; s && d && d->Searching; s = *++it)
					{
						Result *Res = d->Test(s);
						if (Res)
						{
							Res->Grp = r->IsStringGroup();
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
				else if (r->IsDialog())
				{
					List<ResDialogCtrl> Ctrls;
					r->IsDialog()->EnumCtrls(Ctrls);
					for (auto c: Ctrls)
					{
						if (!d || !d->Searching)
							break;

						Result *Res = d->Test(c->GetStr());
						if (Res)
						{
							Res->Ctrl = c;
							Res->Dialog = r->IsDialog();
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
				else if (r->IsMenu())
				{
					List<ResMenuItem> Items;
					r->IsMenu()->EnumItems(Items);
					for (auto c: Items)
					{
						if (!d || !d->Searching)
							break;

						Result *Res = d->Test(c);
						if (Res)
						{
							Res->Menu = c;
							d->Lst->Insert(Res);
						}
						
						LgiYield();
					}
				}
			}
			d->Searching = false;
		}
	}
	
	d->Running = true;
}

Results::~Results()
{
	DeleteArray(d->Text);
	DeleteArray(d->Define);
	DeleteObj(d);
}

void Results::OnPosChange()
{
	GRect Client = GetClient();

	GViewI *v;
	if (GetViewById(IDC_LIST, v))
	{
		GRect r = v->GetPos();
		r.x2 = Client.x2 - 12;
		r.y2 = Client.y2 - 40;
		v->SetPos(r);

		v = FindControl(IDOK);
		if (v)
		{
			GRect p = v->GetPos();
			p.Offset(Client.x2 - p.X() - 12 - p.x1, r.y2 + 10 - p.y1);
			v->SetPos(p);
		}
	}
}

int Results::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDOK:
		{
			if (d->Searching)
			{
				d->Searching = false;
			}
			else if (d->Running)
			{
				Quit();
			}
			break;
		}
	}

	return 0;
}
