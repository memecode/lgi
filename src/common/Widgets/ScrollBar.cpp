#include "lgi/common/Lgi.h"
#include "lgi/common/ScrollBar.h"
#include "lgi/common/LgiRes.h"

#define DrawBorder(dc, r, edge) LThinBorder(dc, r, edge)

#if defined(LGI_CARBON)
	#define MAC_SKIN		1
#elif defined(LGI_COCOA)
	#define MAC_LOOK		1
#else
	#define WINXP_LOOK		1
#endif

enum ScrollZone
{
	BTN_NONE,
	BTN_SUB,
	BTN_SLIDE,
	BTN_ADD,
	BTN_PAGE_SUB,
	BTN_PAGE_ADD,
};

class LScrollBarPrivate
{
public:
	LScrollBar *Widget;
	bool Vertical;
	int64 Value, Min, Max, Page;
	LRect Sub, Add, Slide, PageSub, PageAdd;
	int Clicked;
	bool Over;
	int SlideOffset;
	int Ignore;

	LScrollBarPrivate(LScrollBar *w)
	{
		Ignore = 0;
		Widget = w;
		Vertical = true;
		Value = Min = 0;
		Max = -1;
		Page = 1;
		Clicked = BTN_NONE;
		Over = false;
	}

	bool IsVertical()
	{
		return Vertical;
	}

	int IsOver()
	{
		return Over ? Clicked : BTN_NONE;
	}

	void DrawIcon(LSurface *pDC, LRect &r, bool Add, LSystemColour c)
	{
		pDC->Colour(c);
		int IconSize = MAX(r.X(), r.Y()) * 2 / 6;
		int Cx = r.x1 + (r.X() >> 1);
		int Cy = r.y1 + (r.Y() >> 1);
		int Off = (IconSize >> 1) * (Add ? 1 : -1);
		int x = Cx + (IsVertical() ? 0 : Off);
		int y = Cy + (IsVertical() ? Off : 0);

		if (Add)
		{
			if (IsOver() == BTN_ADD)
			{
				x++;
				y++;
			}
			if (IsVertical())
			{
				// down
				for (int i=0; i<IconSize; i++, y--)
				{
					pDC->Line(x-i, y, x+i, y);
				}
			}
			else
			{
				// right
				for (int i=0; i<IconSize; i++, x--)
				{
					pDC->Line(x, y-i, x, y+i);
				}
			}
		}
		else
		{
			if (IsOver() == BTN_SUB)
			{
				x++;
				y++;
			}
			if (IsVertical())
			{
				// up
				for (int i=0; i<IconSize; i++, y++)
				{
					pDC->Line(x-i, y, x+i, y);
				}
			}
			else
			{
				// left
				for (int i=0; i<IconSize; i++, x++)
				{
					pDC->Line(x, y-i, x, y+i);
				}
			}
		}
	}

	void OnPaint(LSurface *pDC)
	{
		LColour SlideCol(L_MED);
		SlideCol.Rgb(	(255 + SlideCol.r()) >> 1,
						(255 + SlideCol.g()) >> 1,
						(255 + SlideCol.b()) >> 1);

		#if MAC_LOOK || MAC_SKIN
		
			pDC->Colour(SlideCol);
			pDC->Rectangle();
		
			if (IsValid())
			{
				LRect r = Slide;
				r.Size(3, 3);
				pDC->Colour(L_LOW);
				
				// pDC->Rectangle(&r);
				double rad = (IsVertical() ? (double)r.X() : (double)r.Y()) / 2;
				double cx = (double)r.x1 + rad;
				pDC->FilledArc(cx, r.y1 + rad, rad, 0, 180);
				pDC->Rectangle(r.x1, r.y1 + rad, r.x2, r.y2 - rad);
				pDC->FilledArc(cx, r.y2 - rad, rad, 180, 0);
			}
		
		#elif WINXP_LOOK
		
			// left/up button
			LRect r = Sub;
			DrawBorder(pDC, r, IsOver() == BTN_SUB ? DefaultSunkenEdge : DefaultRaisedEdge);
			pDC->Colour(L_MED);
			pDC->Rectangle(&r);
			DrawIcon(pDC, r, false, IsValid() ? L_BLACK : L_LOW);

			// right/down
			r = Add;
			DrawBorder(pDC, r, IsOver() == BTN_ADD ? DefaultSunkenEdge : DefaultRaisedEdge);
			pDC->Colour(L_MED);
			pDC->Rectangle(&r);
			DrawIcon(pDC, r, true, IsValid() ? L_BLACK : L_LOW);


			// printf("Paint %ix%i, %s\n", pDC->X(), pDC->Y(), Widget->GetPos().GetStr());
			if (IsValid())
			{
				// slide space
				pDC->Colour(SlideCol);
				pDC->Rectangle(&PageSub);
				pDC->Rectangle(&PageAdd);

				// slide button
				r = Slide;
				DrawBorder(pDC, r, DefaultRaisedEdge); // IsOver() == BTN_SLIDE ? SUNKEN : RAISED);
				pDC->Colour(L_MED);
				if (r.Valid()) pDC->Rectangle(&r);
			}
			else
			{
				pDC->Colour(SlideCol);
				pDC->Rectangle(&Slide);
			}
		
		#else
		
			#error "No look and feel defined."
		
		#endif
	}

	int GetWidth()
	{
		return IsVertical() ? Widget->X() : Widget->Y();
	}

	int GetLength()
	{
		return (IsVertical() ? Widget->Y() : Widget->X())
			#if !MAC_LOOK
			- (GetWidth() * 2)
			#endif
			;
	}

	int64 GetRange()
	{
		return Max >= Min ? Max - Min + 1 : 0;
	}

	bool IsValid()
	{
		return Max >= Min;
	}

	void CalcRegions()
	{
		LRect r = Widget->GetPos();
		Vertical = r.Y() > r.X();

		int w = GetWidth();
		int len = GetLength();
		
		// Button sizes
 		#if MAC_LOOK
 			Sub.ZOff(-1, -1);
 			Add.ZOff(-1, -1);
		#else
			Sub.ZOff(w-1, w-1);
			Add.ZOff(w-1, w-1);
	
			// Button positions
			if (IsVertical())
				Add.Offset(0, Widget->GetPos().Y()-w);
			else
				Add.Offset(Widget->GetPos().X()-w, 0);
		#endif


		// Slider
		int64 Start, End;
		#if LGI_SDL
		int MinSize = w; // Touch UI needs large slide....
		#else
		int MinSize = 18;
		#endif

		// printf("Calc %i, " LPrintfInt64 ", " LPrintfInt64 "\n", IsValid(), Min, Max);

		if (IsValid())
		{
			int64 Range = GetRange();
			int64 Size = Range ? MIN((int)Page, Range) * len / Range : len;
			if (Size < MinSize) Size = MinSize;
			Start = Range > Page ? Value * (len - Size) / (Range - (int)Page) : 0;
			End = Start + Size;

			/*
			printf("Range=%i Page=%i Size=%i Start=%i End=%i\n",
				(int)Range, (int)Page,
				(int)Size, (int)Start, (int)End);
			*/

			if (IsVertical())
			{
				Slide.ZOff(w-1, (int) (End-Start-1));
				#if MAC_LOOK
				Slide.Offset(0, (int) (r.y1+Start));
				#else
				Slide.Offset(0, (int) (Sub.y2+1+Start));
				#endif

				if (Start > 1)
				{
					PageSub.x1 = Slide.x1;
					#if MAC_LOOK
					PageSub.y1 = 0;
					#else
					PageSub.y1 = Sub.y2 + 1;
					#endif
					PageSub.x2 = Slide.x2;
					PageSub.y2 = Slide.y1 - 1;
				}
				else
				{
					PageSub.ZOff(-1, -1);
				}

				if (End < Add.y1 - 2)
				{
					PageAdd.x1 = Slide.x1;
					PageAdd.x2 = Slide.x2;
					PageAdd.y1 = Slide.y2 + 1;
					#if MAC_LOOK
					PageAdd.y2 = r.Y()-1;
					#else
					PageAdd.y2 = Add.y1 - 1;
					#endif
				}
				else
				{
					PageAdd.ZOff(-1, -1);
				}
			}
			else
			{
				Slide.ZOff((int)(End-Start-1), w-1);
				Slide.Offset((int)(Sub.x2+1+Start), 0);
				
				if (Start > 1)
				{
					PageSub.y1 = Slide.y1;
					#if MAC_LOOK
					PageSub.x1 = 0;
					#else
					PageSub.x1 = Sub.x2 + 1;
					#endif
					PageSub.y2 = Slide.y2;
					PageSub.x2 = Slide.x1 - 1;
				}
				else
				{
					PageSub.ZOff(-1, -1);
				}

				if (End < Add.x1 - 2)
				{
					PageAdd.y1 = Slide.y1;
					PageAdd.y2 = Slide.y2;
					PageAdd.x1 = Slide.x2 + 1;
					#if MAC_LOOK
					PageAdd.x2 = r.X() - 1;
					#else
					PageAdd.x2 = Add.x1 - 1;
					#endif
				}
				else
				{
					PageAdd.ZOff(-1, -1);
				}
			}
		}
		else
		{
			PageAdd.ZOff(-1, -1);
			PageSub.ZOff(-1, -1);

			Slide = Widget->GetClient();
			if (IsVertical())
			{
				Slide.Size(0, Sub.y2 + 1);
			}
			else
			{
				Slide.Size(Sub.x2 + 1, 0);
			}
		}
	}

	int OnHit(int x, int y)
	{
		#if MAC_SKIN
		
		HIThemeTrackDrawInfo Info;
		LRect Client = Widget->GetClient();
		HIRect Rc = Client;
		Info.version = 0;
		Info.kind = kThemeScrollBarMedium;
		Info.bounds = Rc;
		Info.min = Min;
		Info.max = Max - Page;
		Info.value = Value;
		Info.reserved = 0;
		Info.attributes =	(Widget->Vertical() ? 0 : kThemeTrackHorizontal) |
							(Widget->Focus() ? kThemeTrackHasFocus : 0) |
							kThemeTrackShowThumb;
		Info.enableState = Widget->Enabled() ? kThemeTrackActive : kThemeTrackDisabled;
		Info.filler1 = 0;
		Info.trackInfo.scrollbar.viewsize = Page;
		Info.trackInfo.scrollbar.pressState = false;

		HIPoint pt = {(CGFloat)x, (CGFloat)y};

		ControlPartCode part;

		Boolean b = HIThemeHitTestTrack(&Info, &pt, &part);
		if (b)
		{
			switch (part)
			{
				case kAppearancePartUpButton:	return BTN_SUB;
				case kAppearancePartDownButton:	return BTN_ADD;
				case 129:						return BTN_SLIDE;
				case kControlPageUpPart:		return BTN_PAGE_SUB;
				case kControlPageDownPart:		return BTN_PAGE_ADD;
				default:
					printf("%s:%i - Unknown scroll bar hittest value: %i\n", _FL, part);
					break;
			}
		}
		
		#else
		
		if (Sub.Overlap(x, y)) return BTN_SUB;
		if (Slide.Overlap(x, y)) return BTN_SLIDE;
		if (Add.Overlap(x, y)) return BTN_ADD;
		if (PageSub.Overlap(x, y)) return BTN_PAGE_SUB;
		if (PageAdd.Overlap(x, y)) return BTN_PAGE_ADD;
		
		#endif
		
		return BTN_NONE;
	}

	int OnClick(int Btn, int x, int y)
	{
		if (IsValid())
		{
			switch (Btn)
			{
				case BTN_SUB:
				{
					SetValue(Value-1);
					break;
				}
				case BTN_ADD:
				{
					SetValue(Value+1);
					break;
				}
				case BTN_PAGE_SUB:
				{
					SetValue(Value-Page);
					break;
				}
				case BTN_PAGE_ADD:
				{
					SetValue(Value+Page);
					break;
				}
				case BTN_SLIDE:
				{
					SlideOffset = IsVertical() ? y - Slide.y1 : x - Slide.x1;
					break;
				}
			}
		}
		
		return false;
	}

	void SetValue(int64 i)
	{
		if (i < Min)
		{
			i = Min;
		}

		if (IsValid() && i > Max - Page + 1)
		{
			i = MAX(Min, Max - Page + 1);
		}
		
		if (Value != i)
		{
			Value = i;

			CalcRegions();
			Widget->Invalidate();
			
			LViewI *n = Widget->GetNotify() ? Widget->GetNotify() : Widget->GetParent();
			if (n) n->OnNotify(Widget, (int)Value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////
LScrollBar::LScrollBar()
	: ResObject(Res_ScrollBar)
{
	d = new LScrollBarPrivate(this);
}

LScrollBar::LScrollBar(int id, int x, int y, int cx, int cy, const char *name)
	:	ResObject(Res_ScrollBar)	
{
	d = new LScrollBarPrivate(this);
	SetId(id);

	if (name) Name(name);
	if (cx > cy)
	{
		SetVertical(false);
	}
	LResources::StyleElement(this);
}

LScrollBar::~LScrollBar()
{
	DeleteObj(d);
}

bool LScrollBar::Valid()
{
	return d->Max > d->Min;
}

int LScrollBar::GetScrollSize()
{
	return SCROLL_BAR_SIZE;
}

bool LScrollBar::Attach(LViewI *p)
{
	bool Status = LControl::Attach(p);
	
	#if 0
	printf("%p::Attach scroll bar to %s, Status=%i, _View=%p, Vis=%i\n",
		this, p->GetClass(),
		Status,
		_View,
		Visible());
	#endif
	
	return Status;
}

void LScrollBar::OnPaint(LSurface *pDC)
{
	#if MAC_SKIN

		#if 0
		pDC->Colour(LColour(255, 0, 255));
		pDC->Rectangle();
		#endif
	
		HIThemeTrackDrawInfo Info;
		LRect Client = GetClient();
		HIRect Rc = Client;
		Info.version = 0;
		Info.kind = kThemeScrollBarMedium;
		Info.bounds = Rc;
		Info.min = d->Min;
		Info.max = d->Max - d->Page + 1;
		Info.value = d->Value;
		Info.reserved = 0;
		Info.attributes =	(Vertical() ? 0 : kThemeTrackHorizontal) |
							(Focus() ? kThemeTrackHasFocus : 0) |
							kThemeTrackShowThumb;
		Info.enableState = Enabled() ? kThemeTrackActive : kThemeTrackDisabled;
		Info.filler1 = 0;
		Info.trackInfo.scrollbar.viewsize = d->Page;
		Info.trackInfo.scrollbar.pressState = false;
		CGContextRef Cr = pDC->Handle();
		OSStatus e = HIThemeDrawTrack(&Info, NULL, Cr, kHIThemeOrientationNormal);
		if (e)
			printf("%s:%i - HIThemeDrawTrack failed with %li\n", _FL, e);
	
	#else
	
		d->OnPaint(pDC);
	
	#endif
}

void LScrollBar::OnPosChange()
{
	d->CalcRegions();
}

void LScrollBar::OnMouseClick(LMouse &m)
{
	if (d->Max >= d->Min)
	{
		int Hit = d->OnHit(m.x, m.y);
		Capture(m.Down());
		if (m.Down())
		{
			if (Hit != d->Clicked)
			{
				d->Clicked = Hit;
				d->Over = true;
				Invalidate();
				d->OnClick(Hit, m.x, m.y);
	
				if (Hit != BTN_SLIDE)
				{
					d->Ignore = 2;
					SetPulse(50);
				}
			}
		}
		else
		{
			if (d->Clicked)
			{
				d->Clicked = BTN_NONE;
				d->Over = false;
				Invalidate();
			}
		}
	}
}

void LScrollBar::OnMouseMove(LMouse &m)
{
	if (IsCapturing())
	{
		if (d->Clicked == BTN_SLIDE)
		{
			if (d->GetLength())
			{
				int64 Range = d->GetRange();
				int Len = d->GetLength();
				int Size = d->IsVertical() ? d->Slide.Y() : d->Slide.X();
				int Px = (d->IsVertical() ? m.y : m.x) - d->GetWidth() - d->SlideOffset;
				int64 Value = Px * (Range - d->Page) / (Len - Size);
				d->SetValue(Value);
			}
		}
		else
		{
			int Hit = d->OnHit(m.x, m.y);
			bool Over = Hit == d->Clicked;

			if (Over != d->Over)
			{
				d->Over = Over;
				Invalidate();
			}
		}
	}
}

bool LScrollBar::OnKey(LKey &k)
{
	return false;
}

bool LScrollBar::OnMouseWheel(double Lines)
{
	return false;
}

bool LScrollBar::Vertical()
{
	return d->Vertical;
}

void LScrollBar::SetVertical(bool v)
{
	d->Vertical = v;
	d->CalcRegions();
	Invalidate();
}

int64 LScrollBar::Value()
{
	return d->Value;
}

void LScrollBar::Value(int64 v)
{
	d->SetValue(v);
}

LRange LScrollBar::GetRange() const
{
	return LRange(d->Min, d->Max - d->Min + 1);
}

void LScrollBar::Limits(int64 &Low, int64 &High)
{
	Low = d->Min;
	High = d->Max;
}

bool LScrollBar::SetRange(const LRange &r)
{
	SetLimits(r.Start, r.End());
	return true;
}

void LScrollBar::SetLimits(int64 Low, int64 High)
{
	if (d->Min != Low ||
		d->Max != High)
	{
		d->Min = Low;
		d->Max = High;
		d->Page = MIN(d->Page, d->GetRange());

		d->CalcRegions();

		Invalidate();
		OnConfigure();
		
		// printf("LScrollBar::SetLimits " LPrintfInt64 ", " LPrintfInt64 "\n", d->Min, d->Max);
	}
}

int64 LScrollBar::Page()
{
	return d->Page;
}

void LScrollBar::SetPage(int64 i)
{
	if (d->Page != i)
	{
		d->Page = MAX(i, 1);
		d->CalcRegions();
		Invalidate();
		OnConfigure();
	}
}

GMessage::Result LScrollBar::OnEvent(GMessage *Msg)
{
	return LView::OnEvent(Msg);
}

void LScrollBar::OnPulse()
{
	if (d->Ignore > 0)
	{
		d->Ignore--;
	}
	else
	{
		LMouse m;
		if (GetMouse(m))
		{
			int Hit = d->OnHit(m.x, m.y);
			if (Hit == d->Clicked)
			{
				d->OnClick(d->Clicked, m.x, m.y);
			}
		}
	}
}
