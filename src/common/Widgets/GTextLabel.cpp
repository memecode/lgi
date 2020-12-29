#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextLabel.h"
#include "LStringLayout.h"
#include "GVariant.h"
#include "GNotifications.h"
#include "LgiRes.h"
#include "GCssTools.h"

class GTextPrivate : public LStringLayout, public LMutex
{
	GTextLabel *Ctrl;

public:
	/// When GTextLabel::Name(W) is called out of thread, the string is put
	/// here first and then a message is posted over to the GUI thread
	/// to load it into the ctrl.
	GString ThreadName;
	int PrevX;

	GTextPrivate(GTextLabel *ctrl) : LStringLayout(LgiApp->GetFontCache()), LMutex("GTextPrivate")
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

	bool Layout(GFont *Base, int Px)
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

GTextLabel::GTextLabel(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_StaticText)
{
	d = new GTextPrivate(this);

	// This allows LStringLayout::DoLayout to do some basic layout for GetMax.
	GRect rc(0, 0, 1, 20);
	SetPos(rc);

	if (name)
		Name(name);

	if (cx < 0) cx = d->GetMax().x;
	if (cy < 0) cy = d->GetMax().y;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
}

GTextLabel::~GTextLabel()
{
	DeleteObj(d);
}

void GTextLabel::OnAttach()
{
	LgiResources::StyleElement(this);
	OnStyleChange();
	GView::OnAttach();
}

bool GTextLabel::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	if (p == ObjStyle)
	{
		const char *Style = Value.Str();
		if (Style)
			GetCss(true)->Parse(Style, GCss::ParseRelaxed);
		return true;
	}
	
	return false;
}

bool GTextLabel::GetWrap()
{
	return d->GetWrap();
}

void GTextLabel::SetWrap(bool b)
{
	d->SetWrap(b);
	d->Layout(GetFont(), X());
	Invalidate();
}

bool GTextLabel::Name(const char *n)
{
	LMutex::Auto Lck(d, _FL);
	if (!Lck)
		return false;

	if (InThread())
	{
		GView::Name(n);
		OnStyleChange();
		SendNotify(GNotifyTableLayout_Refresh);
	}
	else
	{
		d->ThreadName = n ? n : "";
		PostEvent(M_TEXT_UPDATE_NAME); // It's ok if this fails...
	}

	return true;
}

bool GTextLabel::NameW(const char16 *n)
{
	LMutex::Auto Lck(d, _FL);
	if (!Lck)
		return false;

	if (InThread())
	{
		GView::NameW(n);
		OnStyleChange();
		SendNotify(GNotifyTableLayout_Refresh);
	}
	else
	{
		d->ThreadName = n ? n : L"";
		PostEvent(M_TEXT_UPDATE_NAME); // It's ok if this fails...
	}

	return true;
}

void GTextLabel::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), X());
	Invalidate();
}

int64 GTextLabel::Value()
{
	auto n = Name();
	return (n) ? Atoi(n) : 0;
}

void GTextLabel::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LPrintfInt64, i);
	Name(Str);
}

void GTextLabel::OnStyleChange()
{
	if (d->Lock(_FL))
	{
		GCss::Len oldsz, newsz;
		if (d->GetFont())
			oldsz = d->GetFont()->Size();

		d->Empty();
		d->Add(GView::Name(), GetCss());
		d->DoLayout(X());
		d->Unlock();
		Invalidate();

		if (d->GetFont())
			newsz = d->GetFont()->Size();
	}
}

void GTextLabel::OnPosChange()
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

bool GTextLabel::OnLayout(GViewLayoutInfo &Inf)
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

void GTextLabel::OnCreate()
{
	if (d->ThreadName)
	{
		GString s;
		s.Swap(d->ThreadName);
		Name(s);
	}
}

GMessage::Result GTextLabel::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_TEXT_UPDATE_NAME:
		{
			if (d->Lock(_FL))
			{
				GString s = d->ThreadName;
				d->ThreadName.Empty();
				d->Unlock();
				if (s)
					Name(s);
			}
			break;
		}
	}
	
	return GView::OnEvent(Msg);
}

int GTextLabel::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl == (GViewI*)this &&
		Flags == GNotify_Activate &&
		GetParent())
	{
		auto c = GetParent()->IterateViews();
		ssize_t Idx = c.IndexOf(this);
		GViewI *n = c[++Idx];
		if (n)
			n->OnNotify(n, Flags);
	}

	return 0;
}

void GTextLabel::OnPaint(GSurface *pDC)
{
	GCssTools Tools(this);
	GColour Back = Tools.GetBack();
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
		
		GRect c = GetClient();
		LPoint pt(c.x1, c.y1);
		d->Paint(pDC, pt, Back, c, Enabled(), false);
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
