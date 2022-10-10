#include <algorithm>

#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Popup.h"
#include "lgi/common/Notifications.h"

#undef max
#define DEFAULT_MINIMUM_SIZE_PX		5
#define ACTIVE_SPACER_SIZE_PX		9

#if 0 //def _DEBUG
#define LOG(...)		if (_Debug) LgiTrace(__VA_ARGS__)
#else
#define LOG(...)
#endif

static int DefaultSpacerPx()
{
	static int px = -1;
	if (px < 0)
	{
		auto dpi = LScreenDpi();
		px = std::max(dpi.x / 17, 5);
	}
	
	return px;
};
#define DEFAULT_SPACER_PX DefaultSpacerPx()

enum LBoxMessages
{
	M_CHILDREN_CHANGED = M_USER + 0x2000
};

struct LBoxViewInfo
{
	LViewI *View = NULL;
	LCss::Len Size; // Original size before layout
};

struct LBoxPriv
{
public:
	bool Vertical = false;
	bool Dirty = false;
	LArray<LBox::Spacer> Spacers;
	LBox::Spacer *Dragging = NULL;
	LPoint DragOffset;
	LArray<LBoxViewInfo> Info;
	
	LBoxPriv()
	{
	}

	LBoxViewInfo *GetInfo(LViewI *v)
	{
		for (auto &i: Info)
			if (i.View == v)
				return &i;
		
		auto &i = Info.New();
		i.View = v;
		return &i;
	}
	
	int GetBox(LRect &r)
	{
		return Vertical ? r.Y() : r.X();
	}

	LBox::Spacer *HitTest(int x, int y)
	{
		for (int i=0; i<Spacers.Length(); i++)
		{
			LBox::Spacer &s = Spacers[i];
			LRect Pos = s.Pos;
			if (Vertical)
			{
				while (Pos.Y() < ACTIVE_SPACER_SIZE_PX)
					Pos.Inset(0, -1);
			}
			else
			{
				while (Pos.X() < ACTIVE_SPACER_SIZE_PX)
					Pos.Inset(-1, 0);
			}
				
			if (Pos.Overlap(x, y))
			{
				return &s;
			}
		}
		
		return NULL;
	}	
};

LBox::LBox(int Id, bool Vertical, const char *name)
{
	d = new LBoxPriv;
	SetId(Id);
	SetVertical(Vertical);
	if (name)
		Name(name);
	LResources::StyleElement(this);
}

LBox::~LBox()
{
	LWindow *Wnd = GetWindow();
	if (Wnd)
		Wnd->UnregisterHook(this);

	DeleteObj(d);
}

bool LBox::IsVertical()
{
	return d->Vertical;
}

void LBox::SetVertical(bool v)
{
	if (d->Vertical != v)
	{
		d->Vertical = v;
		OnPosChange();
	}
}

LBox::Spacer *LBox::GetSpacer(int idx)
{
	if (Children.Length())
	{
		while (d->Spacers.Length() < Children.Length() - 1)
		{
			Spacer &s = d->Spacers.New();
			s.SizePx = DEFAULT_SPACER_PX;
			// s.Colour.c24(DEFAULT_SPACER_COLOUR24);
		}
	}
	
	return idx >= 0 && idx < d->Spacers.Length() ? &d->Spacers[idx] : NULL;
}

LViewI *LBox::GetViewAt(int i)
{
	return Children[i];
}

bool LBox::SetViewAt(uint32_t i, LViewI *v)
{
	if (!v || i > Children.Length())
	{
		return false;
	}

	if (v->GetParent())
		v->Detach();
	
	v->Visible(true);

	bool Status;
	if (i < Children.Length())
	{
		// Remove existing view..
		LViewI *existing = Children[i];
		if (existing == v)
			return true;
		if (existing)
			existing->Detach();
		Status = AddView(v, i);
		
	}
	else
	{
		Status = AddView(v);
	}
	
	if (Status)
	{
		AttachChildren();
	}
	
	return Status;
}

void LBox::OnCreate()
{
	AttachChildren();
	OnPosChange();
	
	LWindow *Wnd = GetWindow();
	if (Wnd)
		Wnd->RegisterHook(this, LMouseEvents);
}

bool LBox::OnViewMouse(LView *v, LMouse &m)
{
	// This hook allows the LBox to catch clicks nearby the splits even if the splits are too small
	// to grab normally. Consider the case of a split that is 1px wide. The active region needs to
	// be a little larger than that, however a normal click would go through to the child windows
	// on either side of the split rather than to the LBox.
	if (!m.IsMove() && m.Down())
	{
		// Convert click to the local coordinates of this view
		LMouse Local = m;
		while (v && v != (LView*)this && v->GetParent())
		{
			if (dynamic_cast<LPopup*>(v))
				return true;

			LRect p = v->GetPos();
			Local.x += p.x1;
			Local.y += p.y1;
			LViewI *vi = v->GetParent();
			v = vi ? vi->GetGView() : NULL;
		}
		
		if (v == (LView*)this)
		{
			// Is the click over our spacers?
			Spacer *s = d->HitTest(Local.x, Local.y);
			if (s)
			{
				// Pass the click to ourselves and prevent the normal view from getting it.
				OnMouseClick(Local);
				return false;
			}
		}
	}
	
	return true;
}

bool LBox::Pour(LRegion &r)
{
	LRect *p = FindLargest(r);
	if (!p)
		return false;

	SetPos(*p);
	return true;
}

void LBox::OnPaint(LSurface *pDC)
{
	if (d->Dirty)
	{
		d->Dirty = false;
		OnPosChange();
	}

	LRect cli = GetClient();
	LCssTools tools(GetCss(), GetFont());
	cli = tools.PaintBorderAndPadding(pDC, cli);

	LColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));

	size_t ChildViews = Children.Length();
	if (ChildViews == 0)
	{
		pDC->Colour(cBack);
		pDC->Rectangle(&cli);
	}
	else
	{
		#if 0 // coverage check...
		pDC->Colour(LColour(255, 0, 255));
		pDC->Rectangle(&cli);
		#endif
		LRegion Painted(cli);

		for (int i=0; i<d->Spacers.Length(); i++)
		{
			Spacer &s = d->Spacers[i];
			if (s.Colour.IsValid())
				pDC->Colour(s.Colour);
			else
				pDC->Colour(cBack);
			pDC->Rectangle(&s.Pos);
			Painted.Subtract(&s.Pos);
		}

		for (auto c : Children)
			Painted.Subtract(&c->GetPos());

		for (auto r = Painted.First(); r; r = Painted.Next())
		{
			pDC->Colour(cBack);
			pDC->Rectangle(r);
		}
	}
}

struct BoxRange
{
	int MinPx, MaxPx;
	LCss::Len Size, Min, Max;
	LViewI *View;
	
	BoxRange &Init()
	{
		MinPx = MaxPx = DEFAULT_MINIMUM_SIZE_PX;
		View = NULL;
		return *this;
	}

	LString toString(const char *label, LCss::Len &l)
	{
		if (!l.IsValid())
			return "";

		LStringPipe p;
		l.ToString(p);
		return LString(", ") + label + "=" + p.NewGStr();
	}

	LString toString()
	{
		LStringPipe p;
		Size.ToString(p);

		LString s;
		s.Printf("%s/%p, %i->%i%s%s%s",
			View?View->GetClass():NULL, View,
			MinPx, MaxPx,
			toString("sz", Size).Get(), toString("min", Min).Get(), toString("max", Max).Get());
		
		return s;
	}
};

void LBox::OnPosChange()
{
	LCssTools tools(GetCss(), GetFont());
	LRect client = GetClient();
	if (!client.Valid())
		return;

	LRect content = tools.ApplyBorder(client);
	content = tools.ApplyPadding(content);
	GetSpacer(0);

	auto views = IterateViews();
	int Cur = content.x1, Idx = 0;
	int AvailablePx = d->GetBox(content);
	if (AvailablePx <= 0)
	{
		LOG("%s:%i - No available px.\n", _FL);
		return;
	}

	LArray<BoxRange> Sizes;

	int SpacerPx = 0;

	int FixedPx = 0;
	LArray<int> FixedChildren;
	int MinPx = 0;
	
	int PercentPx = 0;
	LArray<int> PercentChildren;
	float PercentCount = 0.0f;
	
	LArray<int> AutoChildren;

	LOG("%s:%i - %i views, %i avail px\n", _FL, (int)views.Length(), AvailablePx);

	// Do first pass over children and find their sizes
	for (LViewI *c: views)
	{
		LCss *css = c->GetCss();
		BoxRange &box = Sizes[Idx].Init();
		auto vi = d->GetInfo(c);
		
		box.View = c;
		
		// Get any available CSS size
		if (css)
		{
			if (IsVertical())
			{
				box.Size = css->Height();
				box.Min = css->MinHeight();
				box.Max = css->MaxHeight();
			}
			else
			{
				box.Size = css->Width();
				box.Min = css->MinWidth();
				box.Max = css->MaxWidth();
			}
		}

		// Work out some min and max values		
		if (box.Size.IsValid())
		{
			if (!vi->Size.IsValid())
				vi->Size = box.Size;

			if (box.Size.Type == LCss::LenPercent)
			{
				box.MaxPx = box.Size.ToPx(AvailablePx, GetFont());
				PercentPx += box.MaxPx;
				PercentCount += box.Size.Value;
				PercentChildren.Add(Idx);
			}
			else if (box.Size.IsDynamic())
			{
				AutoChildren.Add(Idx);
			}
			else
			{
				// Fixed children get first crack at the space
				box.MaxPx
					= box.Size.ToPx(AvailablePx, GetFont());
				FixedPx += box.MaxPx;
				FixedChildren.Add(Idx);
			}
		}
		else
		{
			AutoChildren.Add(Idx);

			if (vi)
			{
				if (IsVertical())
					vi->Size = LCss::Len(LCss::LenPx, c->Y());
				else
					vi->Size = LCss::Len(LCss::LenPx, c->X());
			}
		}

		MinPx += box.MinPx;

		LOG("        %s\n", box.toString().Get());

		// Allocate area for spacers in the Fixed portion
		if (Idx < Children.Length() - 1)
		{
			Spacer &s = d->Spacers[Idx];
			SpacerPx += s.SizePx;
		}

		Idx++;
	}

	LOG("    FixedChildren=" LPrintfSizeT " AutoChildren=" LPrintfSizeT "\n",
		FixedChildren.Length(), AutoChildren.Length());

	// Convert all the percentage sizes to px
	int RemainingPx = AvailablePx - SpacerPx - FixedPx;
	LOG("    RemainingPx=%i (%i - %i - %i)\n", RemainingPx, AvailablePx, SpacerPx, FixedPx);
	if (RemainingPx < 0)
	{
		// De-allocate space from the fixed size views...
		int FitPx = AvailablePx - SpacerPx - MinPx;
		double Ratio = (double)FitPx / FixedPx;
		LOG("    Dealloc FitPx=%i Ratio=%.2f\n", FitPx, Ratio);
		for (size_t i=0; i<views.Length(); i++)
		{
			auto c = views[i];
			LCss *css = c->GetCss();
			BoxRange &box = Sizes[i];
			if (css)
			{
				if (!box.Size.IsDynamic())
				{
					int OldPx = box.MaxPx;
					int NewPx = (int)(OldPx * Ratio);
					LAssert(NewPx == 0 || NewPx < OldPx);
					box.MaxPx = NewPx;					
					box.Size = LCss::Len(LCss::LenPx, NewPx);					
					LOG("        %s: px=%i->%i\n", c->GetClass(), OldPx, NewPx);
					FixedPx -= OldPx - NewPx;
				}
			}			
		}

		RemainingPx = AvailablePx - SpacerPx - FixedPx;
	}

	LOG("    RemainingPx=%i (%i - %i - %i)\n", RemainingPx, AvailablePx, SpacerPx, FixedPx);
	for (int i=0; i<Sizes.Length(); i++)
	{
		BoxRange &box = Sizes[i];
		if (box.Size.Type == LCss::LenPercent)
		{
			LOG("        PercentSize: %s, %i > %i\n", box.toString().Get(), PercentPx, RemainingPx);
			if (PercentPx > RemainingPx)
			{
				if (AutoChildren.Length() > 0 || PercentChildren.Length() > 1)
				{
					// Well... ah... we better leave _some_ space for them.
					auto AutoPx = 16 * AutoChildren.Length();
					float Ratio = ((float)RemainingPx - AutoPx) / PercentPx;
					int Px = (int) (box.MaxPx * Ratio);
					box.Size.Type = LCss::LenPx;
					box.Size.Value = (float) Px;
					RemainingPx -= Px;
				}
				else
				{
					// We can just take all the space...
					box.Size.Type = LCss::LenPx;
					box.Size.Value = (float) RemainingPx;
					RemainingPx = 0;
				}
			}
			else
			{
				box.Size.Type = LCss::LenPx;
				box.Size.Value = (float) box.MaxPx;
				RemainingPx -= box.MaxPx;
			}
		}
	}

	// Convert auto children to px
	auto AutoPx = AutoChildren.Length() > 0 ? RemainingPx / AutoChildren.Length() : 0;
	LOG("    AutoPx=%i, %i / " LPrintfSizeT "\n", AutoPx, RemainingPx, AutoChildren.Length());
	while (AutoChildren.Length())
	{
		auto i = AutoChildren[0];
		BoxRange &box = Sizes[i];
		AutoChildren.DeleteAt(0, true);

		LOG("        Auto: %s\n", box.toString().Get());
		
		box.Size.Type = LCss::LenPx;
		if (AutoChildren.Length() > 0)
		{
			box.Size.Value = (float) AutoPx;
			RemainingPx -= (int)AutoPx;
		}
		else
		{
			box.Size.Value = (float) RemainingPx;
			RemainingPx = 0;
		}

		LOG("        AutoAlloc: %s\n", box.toString().Get());
	}
	
	for (int i=0; i<Sizes.Length(); i++)
	{
		BoxRange &box = Sizes[i];
		LAssert(box.Size.Type == LCss::LenPx);
		int Px = (int) (box.Size.Value + 0.01);

		LOG("    Px[%i]=%i box=%s\n", i, Px, box.toString().Get());

		// Allocate space for view
		LRect viewPos = content;
		if (d->Vertical)
		{
			viewPos.y1 = Cur;
			viewPos.y2 = Cur + Px - 1;
		}
		else
		{
			viewPos.x1 = Cur;
			viewPos.x2 = Cur + Px - 1;
		}
		box.View->SetPos(viewPos);
		LOG("    View[%i] = %s.\n", i, viewPos.GetStr());
		#ifdef WIN32
		// This forces the update, otherwise the child's display lags till the
		// mouse is released *rolls eyes*
		box.View->Invalidate((LRect*)NULL, true);
		#endif
		Cur += Px;
		
		// Allocate area for spacer
		if (i < Sizes.Length() - 1)
		{
			Spacer &s = d->Spacers[i];
			s.Pos = content;
			if (d->Vertical)
			{
				s.Pos.y1 = Cur;
				s.Pos.y2 = Cur + s.SizePx - 1;
			}
			else
			{
				s.Pos.x1 = Cur;
				s.Pos.x2 = Cur + s.SizePx - 1;
			}
			Cur += s.SizePx;
		}
	}
}

void LBox::OnMouseClick(LMouse &m)
{
	#if 0
	{
		LString::Array a;
		for (LViewI *p = this; p; p = p->GetParent())
			a.New() = p->GetClass();
		m.Trace(LString("LBox::OnMouseClick-") + LString(".").Join(a));
	}
	#endif

	if (m.Down())
	{
		d->Dragging = d->HitTest(m.x, m.y);
		if (d->Dragging)
		{
			d->DragOffset.x = m.x - d->Dragging->Pos.x1;
			d->DragOffset.y = m.y - d->Dragging->Pos.y1;
			Capture(d->Dragging != NULL);
		}
	}
	else if (IsCapturing())
	{
		Capture(false);
		d->Dragging = NULL;
	}
}

bool IsValidLen(LCss *c, LCss::PropType p)
{
	if (!c || c->GetType(p) != LCss::TypeLen) return false;
	LCss::Len *l = (LCss::Len*)c->PropAddress(p);
	if (!l) return false;
	return l->IsValid();
}

void LBox::OnMouseMove(LMouse &m)
{
	if (!d->Dragging || !IsCapturing())
		return;

	#if 0
	{
		LString::Array a;
		for (LViewI *p = this; p; p = p->GetParent())
			a.New().Printf("%s/%p", p->GetClass(), p);
		m.Trace(LString("LBox::OnMouseMove-") + LString(".").Join(a));
	}
	#endif

	if (!m.Down())
	{
		// Something else got the up click?
		Capture(false);
		return;
	}

	int DragIndex = (int) (d->Dragging - &d->Spacers[0]);
	if (DragIndex < 0 || DragIndex >= d->Spacers.Length())
	{
		LAssert(0);
		return;
	}

	LViewI *Prev = Children[DragIndex];
	if (!Prev)
	{
		LAssert(0);
		return;
	}

	LViewI *Next = DragIndex < Children.Length() ? Children[DragIndex+1] : NULL;

	LCssTools tools(GetCss(), GetFont());
	LRect Content = tools.ApplyMargin(GetClient());
	int ContentPx = d->GetBox(Content);

	LRect SplitPos = d->Dragging->Pos;

	LCss *PrevStyle = Prev->GetCss();
	LCss::PropType Style = d->Vertical ? LCss::PropHeight : LCss::PropWidth;
	bool EditPrev = !Next || IsValidLen(PrevStyle, Style);
	LViewI *Edit = EditPrev ? Prev : Next;
	LAssert(Edit != NULL);
	LRect ViewPos = Edit->GetPos();
	auto *EditCss = Edit->GetCss(true);

	if (d->Vertical)
	{
		// Work out the minimum height of the view
		LCss::Len MinHeight = EditCss->MinHeight();
		int MinPx = MinHeight.IsValid() ? MinHeight.ToPx(ViewPos.Y(), Edit->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

		int Offset = m.y - d->DragOffset.y - SplitPos.y1;
		if (Offset)
		{
			// Slide up and down the Y axis

			// Limit to the min size
			LRect r = ViewPos;
			if (EditPrev)
			{
				r.y2 += Offset;
				if (r.Y() < MinPx)
				{
					int Diff = MinPx - r.Y();
					Offset += Diff;
					r.y2 += Diff;
				}
			}
			else
			{
				r.y1 += Offset;
				if (r.Y() < MinPx)
				{
					int Diff = MinPx - r.Y();
					Offset -= Diff;
					r.y1 -= Diff;
				}
			}

			if (Offset)
			{
				SplitPos.Offset(0, Offset);
		
				// Save the new height of the view
				LCss::Len Ht = EditCss->Height();
				if (Ht.Type == LCss::LenPercent && ContentPx > 0)
				{
					Ht.Value = (float)r.Y() * 100 / ContentPx;
				}
				else
				{
					Ht.Type = LCss::LenPx;
					Ht.Value = (float)r.Y();
				}

				EditCss->Height(Ht);
			}
		}
	}
	else
	{
		// Work out the minimum width of the view
		LCss::Len MinWidth = EditCss->MinWidth();
		int MinPx = MinWidth.IsValid() ? MinWidth.ToPx(ViewPos.X(), Edit->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

		int Offset = m.x - d->DragOffset.x - SplitPos.x1;
		if (Offset)
		{
			// Slide along the X axis

			// Limit to the min size
			LRect r = ViewPos;
			if (EditPrev)
			{
				r.x2 += Offset;
				int rx = r.X();
				if (r.X() < MinPx)
				{
					int Diff = MinPx - rx;
					Offset += Diff;
					r.x2 += Diff;
				}
			}
			else
			{
				r.x1 += Offset;
				int rx = r.X();
				if (r.X() < MinPx)
				{
					int Diff = MinPx - rx;
					Offset -= Diff;
					r.x1 -= Diff;
				}
			}

			if (Offset)
			{
				SplitPos.Offset(Offset, 0);
		
				// Save the new height of the view
				LCss::Len Wid = EditCss->Width();
				if (Wid.Type == LCss::LenPercent && ContentPx > 0)
				{
					Wid.Value = (float)r.X() * 100 / ContentPx;
				}
				else
				{
					Wid.Type = LCss::LenPx;
					Wid.Value = (float)r.X();
				}

				EditCss->Width(Wid);
			}
		}
	}
	
	OnPosChange();
	Invalidate((LRect*)NULL, true);
}

int LBox::OnNotify(LViewI *Ctrl, LNotification n)
{
	if (n.Type == LNotifyTableLayoutRefresh)
	{
		d->Dirty = true;
		
		#if LGI_VIEW_HANDLE
		if (Handle())
		#endif
			PostEvent(M_CHILDREN_CHANGED);
	}
		
	return LView::OnNotify(Ctrl, n);
}

void LBox::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	#if 0
	LgiTrace("LBox(%s)::OnChildrenChanged(%s, %i)\n", Name(), Wnd ? Wnd->GetClass() : NULL, Attaching);
	for (int i=0; i<Children.Length(); i++)
		LgiTrace("	[%i]=%s hnd=%p vis=%i\n", i, Children[i]->GetClass(), Children[i]->Handle(), Children[i]->Visible());
	#endif
	
	d->Dirty = true;
	#if LGI_VIEW_HANDLE
	if (Handle())
	#endif
		PostEvent(M_CHILDREN_CHANGED);
}

int64 LBox::Value()
{
	LViewI *v = Children.Length() ? Children[0] : NULL;
	if (!v) return 0;

	LCss *css = v->GetCss();
	if (!css) return 0;

	LCss::Len l = d->Vertical ? css->Height() : css->Width();
	if (l.Type != LCss::LenPx)
		return 0;

	return (int64)l.Value;
}

void LBox::Value(int64 i)
{
	LViewI *v = Children.Length() ? Children[0] : NULL;
	if (!v) return;

	LCss *css = v->GetCss(true);
	if (!css) return;

	if (d->Vertical)
		css->Height(LCss::Len(LCss::LenPx, (float)i));
	else
		css->Width(LCss::Len(LCss::LenPx, (float)i));
	OnPosChange();
}

LCursor LBox::GetCursor(int x, int y)
{
	Spacer *Over = d->HitTest(x, y);
	if (Over)
		return (d->Vertical) ? LCUR_SizeVer : LCUR_SizeHor;
	else
		return LCUR_Normal;
}

bool LBox::OnLayout(LViewLayoutInfo &Inf)
{
	Inf.Width.Min = -1;
	Inf.Width.Max = -1;
	Inf.Height.Min = -1;
	Inf.Height.Max = -1;
	return true;
}

bool LBox::Serialize(LDom *Dom, const char *OptName, bool Write)
{
	if (Write)
	{
	}
	else
	{
	}

	LAssert(0);
	
	return false;
}

bool LBox::SetSize(int ViewIndex, LCss::Len Size)
{
	LViewI *v = Children[ViewIndex];
	if (!v)
		return false;

	LCss *c = v->GetCss(true);
	if (!c)
		return false;
	
	c->Width(Size);
	return true;
}

LMessage::Result LBox::OnEvent(LMessage *Msg)
{
	if (Msg->Msg() == M_CHILDREN_CHANGED)
	{
		if (d->Dirty)
		{
			d->Dirty = false;
			OnPosChange();
		}
	}
	
	return LView::OnEvent(Msg);
}
