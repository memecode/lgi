#include <stdlib.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/StringLayout.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/CssTools.h"

class LTextPrivate : public LStringLayout, public LMutex
{
	LTextLabel *Ctrl;

public:
	/// When LTextLabel::Name(W) is called out of thread, the string is put
	/// here first and then a message is posted over to the GUI thread
	/// to load it into the ctrl.
	LString ThreadName;
	int PrevX;

	LTextPrivate(LTextLabel *ctrl) : LStringLayout(LAppInst->GetFontCache()), LMutex("LTextPrivate")
	{
		Ctrl = ctrl;
		PrevX = -1;
		AmpersandToUnderline = true;
	}

	bool PreLayout(int32 &Min, int32 &Max)
	{
		if (Lock(_FL))
		{
			DoPreLayout(Min, Max);
			Unlock();
		}
		else return false;
		return true;
	}

	bool Layout(LFont *Base, int Px)
	{		
		if (!Lock(_FL))
			return false;

		SetBaseFont(Base);
		bool Status = DoLayout(Px);
		Unlock();
		PrevX = Px;

		return Status;
	}
};

LTextLabel::LTextLabel(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_StaticText)
{
	d = new LTextPrivate(this);

	// This allows LStringLayout::DoLayout to do some basic layout for GetMax.
	LRect rc(0, 0, 1, 20);
	SetPos(rc);

	if (name)
		Name(name);

	if (cx < 0) cx = d->GetMax().x;
	if (cy < 0) cy = d->GetMax().y;

	LRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
}

LTextLabel::~LTextLabel()
{
	DeleteObj(d);
}

void LTextLabel::OnAttach()
{
	LResources::StyleElement(this);
	OnStyleChange();
	LView::OnAttach();
}

bool LTextLabel::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	if (p == ObjStyle)
	{
		const char *Style = Value.Str();
		if (Style)
			GetCss(true)->Parse(Style, LCss::ParseRelaxed);
		return true;
	}
	
	return false;
}

bool LTextLabel::GetWrap()
{
	return d->GetWrap();
}

void LTextLabel::SetWrap(bool b)
{
	d->SetWrap(b);
	d->Layout(GetFont(), X());
	Invalidate();
}

bool LTextLabel::Name(const char *n)
{
	LMutex::Auto Lck(d, _FL);
	if (!Lck)
		return false;

	if (InThread())
	{
		LView::Name(n);
		OnStyleChange();
		SendNotify(LNotifyTableLayoutRefresh);
	}
	else
	{
		d->ThreadName = n ? n : "";
		PostEvent(M_TEXT_UPDATE_NAME); // It's ok if this fails...
	}

	return true;
}

bool LTextLabel::NameW(const char16 *n)
{
	LMutex::Auto Lck(d, _FL);
	if (!Lck)
		return false;

	if (InThread())
	{
		LView::NameW(n);
		OnStyleChange();
		SendNotify(LNotifyTableLayoutRefresh);
	}
	else
	{
		d->ThreadName = n ? n : L"";
		PostEvent(M_TEXT_UPDATE_NAME); // It's ok if this fails...
	}

	return true;
}

void LTextLabel::SetFont(LFont *Fnt, bool OwnIt)
{
	LView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), X());
	Invalidate();
}

int64 LTextLabel::Value()
{
	auto n = Name();
	return (n) ? Atoi(n) : 0;
}

void LTextLabel::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LPrintfInt64, i);
	Name(Str);
}

void LTextLabel::OnStyleChange()
{
	if (d->Lock(_FL))
	{
		LCss::Len oldsz, newsz;
		if (d->GetFont())
			oldsz = d->GetFont()->Size();

		d->Empty();
		d->Add(LView::Name(), GetCss());
		d->DoLayout(X());
		d->Unlock();

		if (d->GetFont())
			newsz = d->GetFont()->Size();

		Invalidate();
	}
}

void LTextLabel::OnPosChange()
{
	if (d->PrevX != X())
	{
		if (d->Debug)
			LgiTrace("Layout %i, %i\n", d->PrevX, X());
		d->Layout(GetFont(), X());
	}
	else if (d->Debug)
		LgiTrace("No Layout %i, %i\n", d->PrevX, X());
}

bool LTextLabel::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
	{
		d->PreLayout(Inf.Width.Min,
					 Inf.Width.Max);
	}
	else
	{
		d->Layout(GetFont(), Inf.Width.Max);
		Inf.Height.Min = d->GetMin().y;
		Inf.Height.Max = d->GetMax().y;
	}
	
	return true;
}

void LTextLabel::OnCreate()
{
	if (d->ThreadName)
	{
		LString s;
		s.Swap(d->ThreadName);
		Name(s);
	}
}

LMessage::Result LTextLabel::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_TEXT_UPDATE_NAME:
		{
			if (d->Lock(_FL))
			{
				LString s = d->ThreadName;
				d->ThreadName.Empty();
				d->Unlock();
				if (s)
					Name(s);
			}
			break;
		}
	}
	
	return LView::OnEvent(Msg);
}

int LTextLabel::OnNotify(LViewI *Ctrl, LNotification n)
{
	if (Ctrl == (LViewI*)this &&
		n.Type == LNotifyActivate &&
		GetParent())
	{
		auto c = GetParent()->IterateViews();
		ssize_t Idx = c.IndexOf(this);
		LViewI *v = c[++Idx];
		if (v)
			v->OnNotify(Ctrl, n);
	}

	return 0;
}

void LTextLabel::OnPaint(LSurface *pDC)
{
	LCssTools Tools(this);
	LColour Back = Tools.GetBack();
	Tools.PaintContent(pDC, GetClient());
	if (Tools.GetBackImage())
		Back.Empty();

	if (d->Lock(_FL))
	{
		if (d->ThreadName)
		{
			Name(d->ThreadName);
			d->ThreadName.Empty();
		}

		LRect client = GetClient();
		LPoint pt(client.x1, client.y1);
		d->Paint(pDC, pt, Back, client, Enabled(), false);
		d->Unlock();
	}
	else if (!Back.IsTransparent())
	{
		pDC->Colour(Back);
		pDC->Rectangle();
	}

	#if 0
	pDC->Colour(Rgb24(255, 0, 0), 24);
	pDC->Box(0, 0, X()-1, Y()-1);
	#endif	
}
