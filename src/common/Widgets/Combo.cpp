#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Combo.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Menu.h"

#define COMBO_HEIGHT			20

class LComboPrivate
{
	LAutoPtr<LDisplayString> Text;

public:
	ssize_t Current = 0;
	bool SortItems = false;
	LVariantType SubMenuType = GV_NULL;
	LRect Arrow;
	LString::Array items;
	uint64 LastKey = 0;
	LAutoString Find;
	LAutoPtr<LSubMenu> Menu;
	LCombo::SelectedState SelState = LCombo::SelectedDisable;
	bool LayoutDirty = false;

	#if defined LGI_CARBON
	ThemeButtonDrawInfo Cur;
	#endif

	LComboPrivate()
	{
		#if defined LGI_CARBON
			Cur.state = kThemeStateInactive;
			Cur.value = kThemeButtonOff;
			Cur.adornment = kThemeAdornmentNone;
		#endif
	}

	LDisplayString *GetText(const char *File, int Line)
	{
		return Text;
	}

	void SetText(LDisplayString *ds, const char *File, int Line)
	{
		Text.Reset(ds);
	}
	
	LAutoPtr<LDisplayString> *GetTextPtr()
	{
		return &Text;
	}
};

LRect LCombo::Pad(8, 4, 24, 4);

LCombo::LCombo(int id, LRect *pos) :
	ResObject(Res_ComboBox)
{
	d = new LComboPrivate;
	
	SetId(id);
	SetTabStop(true);
	if (pos)
		SetPos(*pos);

	#if WINNATIVE
		SetDlgCode(DLGC_WANTARROWS);
		SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE);
	#endif
	LResources::StyleElement(this);
}

LCombo::~LCombo()
{
	DeleteObj(d);
}

bool LCombo::Sort()
{
	return d->SortItems;
}

void LCombo::Sort(bool s)
{
	d->SortItems = s;
}

LVariantType LCombo::SubMenuType()
{
	return d->SubMenuType;
}

void LCombo::SubMenuType(LVariantType type) // GV_???
{
	d->SubMenuType = type;
}

LSubMenu *LCombo::GetMenu()
{
	return d->Menu;
}

void LCombo::SetMenu(LSubMenu *m)
{
	d->Menu.Reset(m);
}

size_t LCombo::Length()
{
	return d->items.Length();
}

const char *LCombo::operator [](ssize_t i)
{
	return d->items[i];
}

bool LCombo::Name(const char *n)
{
	if (ValidStr(n))
	{
		int Index = 0;
		for (auto i: d->items)
		{
			if (i.Equals(n))
			{
				Value(Index);
				return true;
			}
			Index++;
		}

		d->items.Add(n);
		d->Current = d->items.Length() - 1;
		d->SetText(NULL, _FL);
		Invalidate();
	}
	else
	{
		d->Current = 0;
		d->SetText(NULL, _FL);
		Invalidate();
	}

	return true;
}

const char *LCombo::Name()
{
	return d->items[d->Current];
}

void LCombo::Value(int64 i)
{
	if (d->Current != i)
	{
		d->Current = (int)i;
		d->SetText(NULL, _FL);
		Invalidate();

		SendNotify(LNotifyValueChanged);
	}
}

int64 LCombo::Value()
{
	return d->Current;
}

LMessage::Result LCombo::OnEvent(LMessage *Msg)
{
	return LView::OnEvent(Msg);
}

void LCombo::Empty()
{
	while (Delete((size_t)0))
		;
	Name(0);
}

bool LCombo::Delete()
{
	return Delete(d->Current);
}

bool LCombo::Delete(size_t i)
{
	if (!d->items.IdxCheck(i))
		return false;

	return d->items.DeleteAt(i);
}

bool LCombo::Delete(const char *p)
{
	if (!p)
		return false;
	auto idx = d->items.IndexOf(p);
	if (idx < 0)
		return false;

	d->items.DeleteAt(idx);
	d->LayoutDirty = true;
	return true;
}

bool LCombo::Insert(const char *p, int Index)
{
	if (!p)
		return false;

	if (!d->items.AddAt(Index, p))
		return false;
	
	d->LayoutDirty = true;
	Invalidate();
	return true;
}

void LCombo::DoMenu()
{
	LPoint p(0, Y());
	PointToScreen(p);

	if (d->Menu)
	{
		int Result = d->Menu->Float(this, p.x, p.y, LSubMenu::BtnLeft);
		if (Result)
		{
			GetWindow()->OnCommand(Result, 0,
				#if !LGI_VIEW_HANDLE
				NULL
				#else
				Handle()
				#endif
				);
		}
	}
	else
	{
		auto SubMenuType = d->SubMenuType;
		LSubMenu RClick;
		int Base = 1000;
		int i=0;

		if (d->items.Length() > 500 &&
			SubMenuType == GV_NULL)
		{
			// Seems like an excessive amount of items to not have a sub menu type?
			SubMenuType = GV_STRING;
		}

		if (SubMenuType != GV_NULL)
		{
			if (SubMenuType == GV_INT32)
			{
				d->items.Sort([](auto *a, auto *b)
					{
						return a->Int() - b->Int();
					});
			}
			else if (SubMenuType == GV_DOUBLE)
			{
				d->items.Sort([](auto *a, auto *b)
					{
						auto A = a->Float();
						auto B = b->Float();
						return A > B ? 1 : A < B ? -1 : 0;
					});
			}
			else
			{
				d->items.Sort([](auto *a, auto *b)
					{
						return Stricmp(a->Get(), b->Get());
					});
			}
		}
		else if (d->SortItems)
		{
			d->items.Sort([](auto *a, auto *b)
				{
					return Stricmp(a->Get(), b->Get());
				});
		}

		auto It = d->items.begin();
		char *c = *It;
		if (c)
		{
			if (SubMenuType)
			{
				int n = 0;
				double dbl = 0;
				char16 f = 0;
				LSubMenu *m = 0;
				for (; c; c = *(++It), i++)
				{
					LUtf8Ptr u(c);
					if (SubMenuType == GV_INT32)
					{
						int ci = atoi(c);
						if (!m || n != ci)
						{
							char Name[16];
							sprintf_s(Name, sizeof(Name), "%i...", n = ci);
							m = RClick.AppendSub(Name);
						}
					}
					else if (SubMenuType == GV_DOUBLE)
					{
						double cd = atof(c);
						if (!m || dbl != cd)
						{
							char Name[16];
							sprintf_s(Name, sizeof(Name), "%f", dbl = cd);
							char *e = Name+strlen(Name)-1;
							while (e > Name && e[0] == '0') *e-- = 0;
							strcat(Name, "...");
							m = RClick.AppendSub(Name);
						}
					}
					else
					{
						if (!m || f != u)
						{
							char16 Name[16], *n = Name;
							*n++ = (f = u);
							*n++ = '.';
							*n++ = '.';
							*n++ = '.';
							*n++ = 0;
							LAutoString a(WideToUtf8(Name));
							m = RClick.AppendSub(a);
						}
					}

					switch (d->SelState)
					{
						default:
							m->AppendItem(c, Base+i, i != d->Current);
							break;
						case SelectedHide:
							if (i != d->Current)
								m->AppendItem(c, Base+i, true);
							break;
						case SelectedShow:
							m->AppendItem(c, Base+i, true);
							break;
					}
				}
			}
			else
			{
				for (; c; c = *(++It), i++)
				{
					switch (d->SelState)
					{
						default:
							RClick.AppendItem(c, Base+i, i != d->Current);
							break;
						case SelectedHide:
							if (i != d->Current)
								RClick.AppendItem(c, Base+i, true);
							break;
						case SelectedShow:
							RClick.AppendItem(c, Base+i, true);
							break;
					}
				}
			}
		}
		else
		{
			RClick.AppendItem("", Base+i, false);
		}

		int Result = RClick.Float(this, p.x, p.y, LSubMenu::BtnLeft);
		if (Result >= Base)
		{
			d->Current = Result - Base;
			d->SetText(NULL, _FL);
			Invalidate();

			SendNotify(LNotifyValueChanged);
		}
	}
}

void LCombo::OnMouseClick(LMouse &m)
{
	if (m.Down() &&
		Enabled())
	{
		Focus(true);

		if (m.Left())
		{
			DoMenu();
		}
	}
}

bool LCombo::OnKey(LKey &k)
{
	bool Status = false;

	if (k.Down())
	{
		switch (k.vkey)
		{
			case LK_SPACE:
			{
				// Open the submenu
				Status = true;
				DoMenu();
				break;
			}
			case LK_UP:
			{
				// Previous value in the list
				int64 i = Value();
				if (i > 0)
				{
					Value(i-1);
				}
				Status = true;
				break;
			}
			case LK_DOWN:
			{
				// Next value in the list...
				int i = (int)Value();
				if (i < d->items.Length()-1)
				{
					Value(i+1);
				}
				Status = true;
				break;
			}
			default:
			{
				// Search for value
				if (k.IsChar && k.c16 > ' ')
				{
					uint64 Now = LCurrentTime();
					if (d->LastKey + 2000 < Now)
					{
						d->Find.Reset();
					}

					size_t Len = d->Find ? strlen(d->Find) : 0;
					LAutoString n(new char[Len+2]);
					if (n)
					{
						if (d->Find) strcpy(n, d->Find);
						n[Len++] = k.c16;
						n[Len] = 0;
						d->Find = n;
					}

					if (Len > 0 && d->Find)
					{
						int Index = 0;
						for (auto &i: d->items)
						{
							if (strnicmp(i, d->Find, Len) == 0)
							{
								Value(Index);
								break;
							}
							Index++;
						}
					}
					d->LastKey = Now;
					Status = true;
				}
				break;
			}
		}
	}

	return Status;
}

bool LCombo::OnLayout(LViewLayoutInfo &Inf)
{
	auto fnt = GetFont();

	if (!Inf.Width.Max)
	{
		// Width calc
		int mx = 0;
		for (auto &s: d->items)
		{
			LDisplayString ds(fnt, s);
			mx = MAX(mx, ds.X());
		}
		Inf.Width.Min =
			Inf.Width.Max =
			mx + 36;
	}
	else if (!Inf.Height.Max)
	{
		Inf.Height.Min =
			Inf.Height.Max =
			fnt->GetHeight() + 8;
	}

	return true;
}

void LCombo::OnFocus(bool f)
{
	Invalidate();
}

void LCombo::SetFont(LFont *Fnt, bool OwnIt)
{
	LView::SetFont(Fnt, OwnIt);
	d->SetText(new LDisplayString(GetFont(), Name()), _FL);
	Invalidate();
}

void LCombo::OnPosChange()
{
	LDisplayString *ds = d->GetText(_FL);
	if (ds && ds->IsTruncated())
		d->SetText(NULL, _FL);
}

void LCombo::OnPaint(LSurface *pDC)
{
	if (d->LayoutDirty)
	{
		d->LayoutDirty = false;
		SendNotify(LNotifyTableLayoutRefresh);
	}
	
	if (!d->GetText(_FL))
	{
		auto n = Name();
		if (n)
			d->SetText(new LDisplayString(GetFont(), n), _FL);
	}

	LColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));

	if (LApp::SkinEngine &&
		TestFlag(LApp::SkinEngine->GetFeatures(), GSKIN_COMBO))
	{
		LSkinState State;
		State.pScreen = pDC;
		State.ptrText = d->GetTextPtr();
		State.Enabled = Enabled();
		LApp::SkinEngine->OnPaint_LCombo(this, &State);
	}
	else
	{
		LRect r(0, 0, X()-1, Y()-1);

		// draw shadowed border
		pDC->Colour(L_SHADOW);
		pDC->Line(r.x1+2, r.y2, r.x2, r.y2);
		pDC->Line(r.x2, r.y1+2, r.x2, r.y2);

		pDC->Colour(L_MED);
		pDC->Set(r.x1, r.y2);
		pDC->Set(r.x1+1, r.y2);
		pDC->Set(r.x2, r.y1);
		pDC->Set(r.x2, r.y1+1);

		r.x2--;
		r.y2--;
		pDC->Colour(L_LOW);
		pDC->Box(&r);
		r.Inset(1, 1);
		pDC->Colour(L_HIGH);
		pDC->Line(r.x1, r.y1, r.x2, r.y1);
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2);
		r.x1++;
		r.y1++;

		// fill the background
		pDC->Colour(cBack);
		pDC->Rectangle(&r);

		// draw the drop down arrow
		d->Arrow = r;
		d->Arrow.x1 = d->Arrow.x2 - 10;
		pDC->Colour(Enabled() ? L_SHADOW : L_LOW);
		int Cx = r.x2 - 5;
		int Cy = r.y1 + (r.Y()/2) + 2;
		for (int i=0; i<4; i++)
		{
			pDC->Line(Cx-i, Cy, Cx+i, Cy);
			Cy--;
		}
		r.x2 = d->Arrow.x1 - 1;
		pDC->Colour(L_LOW);
		pDC->Line(r.x2, r.y1+1, r.x2, r.y2-1);
		r.x2--;

		// draw the text
		r.Inset(1, 1);
		if (r.Valid())
		{
			LDisplayString *ds = d->GetText(_FL);
			if (ds)
			{
				if (Enabled())
				{
					bool f = Focus();
					LSysFont->Colour(f ? L_FOCUS_SEL_FORE : L_TEXT, f ? L_FOCUS_SEL_BACK : L_MED);
					LSysFont->Transparent(false);
					ds->Draw(pDC, r.x1, r.y1, &r);
				}
				else
				{
					LSysFont->Transparent(false);
					LSysFont->Colour(LColour(L_LIGHT), cBack);
					ds->Draw(pDC, r.x1+1, r.y1+1, &r);

					LSysFont->Transparent(true);
					LSysFont->Colour(L_LOW);
					ds->Draw(pDC, r.x1, r.y1, &r);
				}
			}
			else
			{
				pDC->Colour(Focus() ? L_FOCUS_SEL_BACK : L_MED);
				pDC->Rectangle(&r);
			}
		}
	}
}

void LCombo::OnAttach()
{
}

LCombo::SelectedState LCombo::GetSelectedState()
{
	return d->SelState;
}

void LCombo::SetSelectedState(SelectedState s)
{
	d->SelState = s;
}

