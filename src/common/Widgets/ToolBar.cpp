/*
**	FILE:			GToolbar.cpp
**	AUTHOR:			Matthew Allen
**	DATE:				18/10/2001
**	DESCRIPTION:	Toolbar classes
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/Variant.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Palette.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/ToolBar.h"
#include "lgi/common/ToolTip.h"
#include "lgi/common/Menu.h"

#define ToolBarHilightColour LC_HIGH

#ifdef WIN32

HPALETTE GetSystemPalette();
bool BltBmpToBmp(HBITMAP hDest, int xDst, int yDst, int cx, int cy, HBITMAP hSrc, int xSrc, int ySrc, DWORD dwRop);
bool BltBmpToDc(HDC DestDC, int xDst, int yDst, int cx, int cy, HBITMAP hSrc, int xSrc, int ySrc,  DWORD dwRop);
bool BltDcToBmp(HBITMAP hDest, int xDst, int yDst, int cx, int cy, HDC SrcDC, int xSrc, int ySrc, DWORD dwRop);

#define AttachButton(b)	AddView(b);

#else

#define AttachButton(b) b->Attach(this);

#endif

enum IconCacheType
{
	IconNormal,
	IconHilight,
	IconDisabled
};

COLOUR Map(LSurface *pDC, COLOUR c);

////////////////////////////////////////////////////////////////////////
LImageList *LLoadImageList(const char *File, int x, int y)
{
	if (x < 0 || y < 0)
	{
		// Detect dimensions in the filename.
		auto leaf = LString(File).Split(DIR_STR).Last();
		auto parts = leaf.RSplit(".", 1);
		auto last = parts[0].Split("-").Last();
		auto dim = last.Split("x");
		if (dim.Length() == 1)
		{
			auto i = dim[0].Strip().Int();
			if (i > 0)
				x = y = (int)i;
		}
		else if (dim.Length() == 2)
		{
			auto X = dim[0].Strip().Int(), Y = dim[1].Strip().Int();
			if (X > 0 && Y > 0)
			{
				x = (int)X;
				y = (int)Y;
			}
		}
	}		

	auto Path = LFileExists(File) ? LString(File) : LFindFile(File);
	if (!Path)
	{
		LgiTrace("%s:%i - Couldn't find '%s'\n", _FL, File);
		return NULL;
	}

	LAutoPtr<LSurface> pDC(GdcD->Load(Path));
	if (!pDC)
	{
		LgiTrace("%s:%i - Couldn't load '%s'\n", _FL, Path.Get());
		return NULL;
	}

	return new LImageList(x, y, pDC);
}

LToolBar *LgiLoadToolbar(LViewI *Parent, const char *File, int x, int y)
{
	LToolBar *Toolbar = new LToolBar;
	if (Toolbar)
	{
		LString FileName = LFindFile(File);
		if (FileName)
		{
			bool Success = FileName && Toolbar->SetBitmap(FileName, x, y);
			if (!Success)
			{
				LgiMsg(Parent,
						"Can't load '%s' for the toolbar.\n"
						"This is probably because libpng/libjpeg is missing.",
						"LgiLoadToolbar",
						MB_OK,
						File);
			}
		}
		else
		{
			LgiMsg(Parent,
					"Can't find the graphic '%s' for the toolbar.\n"
					"You can find it in this program's archive.",
					"LgiLoadToolbar",
					MB_OK,
					File);
		}
	}

	return Toolbar;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ImgLst_Empty	0x40000000
#define IgmLst_Add		0x80000000

class LImageListPriv
{
public:
	LImageList *ImgLst;
	int Sx, Sy;
	uint8_t DisabledAlpha;

	struct CacheDC : public LMemDC
	{
		bool Disabled;
		LColour Back;
	};

	LArray<CacheDC*> Cache;
	LArray<LRect> Bounds;

	CacheDC *GetCache(LColour Back, bool Disabled)
	{
		if (Back.IsTransparent())
			return NULL;

		for (int i=0; i<Cache.Length(); i++)
		{
			CacheDC *dc = Cache[i];
			if (dc->Back == Back &&
				dc->Disabled == Disabled)
				return dc;
		}
		
		CacheDC *dc = new CacheDC;
		if (dc)
		{
			dc->Disabled = Disabled;
			dc->Back = Back;
			
			bool Status = dc->Create(ImgLst->X(), ImgLst->Y(), GdcD->GetColourSpace());
			if (Status)
			{
				dc->Colour(dc->Back);
				dc->Rectangle();
				dc->Op(GDC_ALPHA);
				
				if (Disabled)
				{
					LMemDC tmp(ImgLst->X(), ImgLst->Y(), System32BitColourSpace, LSurface::SurfaceRequireExactCs);
					tmp.Colour(0, 32);
					tmp.Rectangle();
					tmp.Op(GDC_ALPHA);
					tmp.Blt(0, 0, ImgLst);
					tmp.SetConstantAlpha(DisabledAlpha);
					
					dc->Blt(0, 0, &tmp);
				}
				else
				{
					dc->Blt(0, 0, ImgLst);
				}
				
				Cache.Add(dc);
			}
			else
			{
				delete dc;
				LAssert(!"Create memdc failed.");
			}
		}
		
		return dc;
	}

	LImageListPriv(LImageList *imglst, int x, int y)
	{
		ImgLst = imglst;
		Sx = x;
		Sy = y;
		DisabledAlpha = 40;
	}

	~LImageListPriv()
	{
		Cache.DeleteObjects();
	}
};

static bool HasPad(LColourSpace cs)
{
	if (cs == CsRgbx32 ||
		cs == CsBgrx32 ||
		cs == CsXrgb32 ||
		cs == CsXbgr32)
		return true;
	return false;
}

LImageList::LImageList(int x, int y, LSurface *pDC)
{
	d = new LImageListPriv(this, x, y);

	uint32_t Transparent = 0;

	if (pDC &&
		Create(pDC->X(), pDC->Y(), System32BitColourSpace, LSurface::SurfaceRequireExactCs))
	{
		Colour(Transparent, 32);
		Rectangle();
		
		int Old = Op(GDC_ALPHA);
		Blt(0, 0, pDC);
		Op(Old);
		
		#if 0
		printf("Toolbar input image is %s, has_alpha=%i, has_pad=%i\n",
			GColourSpaceToString(pDC->GetColourSpace()),
			pDC->HasAlpha(),
			HasPad(pDC->GetColourSpace()));
		#endif
				
		if (pDC->GetBits() < 32 || HasPad(pDC->GetColourSpace()))
		{
			// auto InCs = pDC->GetColourSpace();
			
			if (!pDC->HasAlpha())
			{
				// No source alpha, do colour keying to create the alpha channel
				REG uint32_t *p = (uint32_t*)(*this)[0];
				if (p)
				{
					uint32_t key = *p;
					for (int y=0; y<Y(); y++)
					{
						p = (uint32_t*) (*this)[y];
						REG uint32_t *e = p + X();
						while (p < e)
						{
							if (*p == key)
							{
								*p = Transparent;
							}
							p++;
						}
					}
				}
			}
		}
		else
		{
			// Clear all the RGB values for transparent pixels.
			REG System32BitPixel *p;
			for (int y=0; y<Y(); y++)
			{
				p = (System32BitPixel*) (*this)[y];
				if (!p)
					continue;
				REG System32BitPixel *e = p + X();
				while (p < e)
				{
					if (p->a == 0)
					{
						p->r = 0;
						p->g = 0;
						p->b = 0;
					}
					p++;
				}
			}
		}
		
		#if 0
		static int Idx = 0;
		char s[256];
		sprintf_s(s, sizeof(s), "imglst_%i.bmp", Idx++);
		WriteDC(s, this);
		#endif
	}
}

LImageList::~LImageList()
{
	DeleteObj(d);
}

void LImageList::Draw(LSurface *pDC, int Dx, int Dy, int Image, LColour Background, bool Disabled)
{
	if (!pDC)
		return;

	LRect rSrc;
	rSrc.ZOff(d->Sx-1, d->Sy-1);
	rSrc.Offset(Image * d->Sx, 0);

	LImageListPriv::CacheDC *Cache = d->GetCache(Background, Disabled);
	if (!Cache && Background.IsValid())
	{
		LRect rDst;
		rDst.ZOff(d->Sx-1, d->Sy-1);
		rDst.Offset(Dx, Dy);

		pDC->Colour(Background);
		pDC->Rectangle(&rDst);
		pDC->Colour(LColour(255, 0, 0));
		pDC->Line(rDst.x1, rDst.y1, rDst.x2, rDst.y2);
		pDC->Line(rDst.x2, rDst.y1, rDst.x1, rDst.y2);
		return;
	}

	if (pDC->SupportsAlphaCompositing())
	{
		int Old = pDC->Op(GDC_ALPHA, Disabled ? d->DisabledAlpha : -1);
		pDC->Blt(Dx, Dy, this, &rSrc);
		pDC->Op(Old);
	}
	else if (Cache)
	{
		pDC->Blt(Dx, Dy, Cache, &rSrc);
	}
	else LAssert(!"Impl me.");
}

int LImageList::TileX()
{
	return d->Sx;
}

int LImageList::TileY()
{
	return d->Sy;
}

int LImageList::GetItems()
{
	return X() / d->Sx;
}

void LImageList::Update(int Flags)
{
}

uint8_t LImageList::GetDisabledAlpha()
{
	return d->DisabledAlpha;
}

void LImageList::SetDisabledAlpha(uint8_t alpha)
{
	d->DisabledAlpha = alpha;
}

LRect LImageList::GetIconRect(int Idx)
{
	LRect r(0, 0, -1, -1);
	
	if (Idx >= 0 && Idx < GetItems())
	{
		r.ZOff(d->Sx-1, d->Sy-1);
		r.Offset(Idx * d->Sx, 0);
	}
	
	return r;
}

LRect *LImageList::GetBounds()
{
	if (!d->Bounds.Length() && (*this)[0])
	{
		int Items = GetItems();
		if (d->Bounds.Length(Items))
		{
			for (int i=0; i<Items; i++)
			{
				d->Bounds[i].ZOff(d->Sx - 1, d->Sy - 1);
				d->Bounds[i].Offset(i * d->Sx, 0);

				LFindBounds(this, &d->Bounds[i]);

				d->Bounds[i].Offset(-i * d->Sx, 0);
			}
		}
	}

	return &d->Bounds[0];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
class LToolBarPrivate
{
public:
	int Bx, By;
	int Sx, Sy;
	bool Vertical;
	bool Text;
	int LastIndex;
	bool OwnImgList;
	LImageList *ImgList;
	LFont *Font;
	GToolTip *Tip;
	
	// Customization menu
	GDom *CustomDom;
	const char *CustomProp;

	// bitmap cache
	LAutoPtr<LMemDC> IconCache;

	LToolBarPrivate()
	{
		Bx = By = 16;
		Sx = Sy = 10;
		Vertical = false;
		Text = false;
		Font = 0;
		Tip = 0;
		CustomProp = 0;
		CustomDom = 0;
	}

	bool ShowTextLabels()
	{
		return (Text || !ImgList) && Font;
	}

	void FixSeparators(LToolBar *Tb)
	{
		// Fix up separators so that no 2 separators are next to each other. I.e.
		// all the buttons between them are switched off.
		LToolButton *Last = 0;
		bool HasVis = false;
		for (LViewI *v: Tb->IterateViews())
		{
			LToolButton *Btn = dynamic_cast<LToolButton*>(v);
			if (Btn)
			{
				if (Btn->Separator())
				{
					Btn->Visible(HasVis);
					if (HasVis)
					{
						Last = Btn;
					}
					HasVis = false;
				}
				else
				{
					HasVis |= Btn->Visible();
				}
			}
		}
		
		if (Last)
		{
			Last->Visible(HasVis);
		}
	}

	void Customizable(LToolBar *Tb)
	{
		LVariant v;
		if (CustomDom)
		{
			CustomDom->GetValue(CustomProp, v);
		}
			
		char *o;
		if ((o = v.Str()))
		{
			GToken t(o, ",");
			if (t.Length() >= 1)
			{
				Text = stricmp(t[0], "text") == 0;
				
				// Make all controls not visible.
				for (auto v: Tb->IterateViews())
				{
					LToolButton *Btn = dynamic_cast<LToolButton*>(v);
					if (Btn) v->Visible(false);
				}
				
				// Set sub-set of ctrls visible according to saved ID list
				for (int i=1; i<t.Length(); i++)
				{
					int Id = atoi(t[i]);
					if (Id > 0)
						Tb->SetCtrlVisible(Id, true);
				}
				
				FixSeparators(Tb);
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
struct LToolButtonPriv
{
	LArray<LDisplayString*> Text;
};

LToolButton::LToolButton(int Bx, int By)
{
	d = new LToolButtonPriv;
	Type = TBT_PUSH;
	SetId(IDM_NONE);
	Down = false;
	Clicked = false;
	Over = false;
	ImgIndex = -1;
	NeedsRightClick = false;

	LRect r(0, 0, Bx+1, By+1);
	SetPos(r);
	SetParent(0);
	TipId = -1;

	_BorderSize = 0;
	LResources::StyleElement(this);
}

LToolButton::~LToolButton()
{
	d->Text.DeleteObjects();
	delete d;
}

bool LToolButton::Name(const char *n)
{
	bool s = LView::Name(n);
	d->Text.DeleteObjects();
	return s;
}

void LToolButton::Layout()
{
	auto Parent = GetParent();
	LToolBar *ToolBar = dynamic_cast<LToolBar*>(Parent);
	if (!ToolBar)
		return;

	// Text
	auto s = Name();
	if (!ToolBar->d->ShowTextLabels() || !s)
		return;

	// Write each word centered on a different line
	char Buf[256];
	strcpy_s(Buf, sizeof(Buf), s);

	GToken t(Buf, " ");
	if (t.Length() < 3)
	{
		if (t.Length() > 0)
			d->Text.Add(new LDisplayString(ToolBar->d->Font, t[0]));
		if (t.Length() > 1)
			d->Text.Add(new LDisplayString(ToolBar->d->Font, t[1]));
	}
	else if (t.Length() == 3)
	{
		sprintf_s(Buf, sizeof(Buf), "%s %s", t[0], t[1]);
		LDisplayString *d1 = new LDisplayString(ToolBar->d->Font, Buf);
		sprintf_s(Buf, sizeof(Buf), "%s %s", t[1], t[2]);
		LDisplayString *d2 = new LDisplayString(ToolBar->d->Font, Buf);
		if (d1 && d2)
		{
			if (d1->X() < d2->X())
			{
				DeleteObj(d2);
				d->Text.Add(d1);
				d->Text.Add(new LDisplayString(ToolBar->d->Font, t[2]));
			}
			else
			{
				DeleteObj(d1);
				d->Text.Add(new LDisplayString(ToolBar->d->Font, t[0]));
				d->Text.Add(d2);
			}
		}
	}
}

void LToolButton::OnPaint(LSurface *pDC)
{
	LToolBar *Par = dynamic_cast<LToolBar*>(GetParent());
	bool e = Enabled();

	if (Par)
	{
		LRect p = GetClient();
		
		#if 0 // def _DEBUG
		pDC->Colour(LColour(255, 0, 255));
		pDC->Rectangle();
		#endif

		LCssTools Tools(this);
		LColour cBack = Tools.GetBack();
		auto BackImg = Tools.GetBackImage();
		bool Hilight = e && Over;
		if (Hilight)
			cBack = cBack.Mix(LColour::White);

		// Draw Background
		if (GetId() >= 0)
		{
			// Draw border
			LColour Background;
			if (Down) // Sunken if the button is pressed
				LThinBorder(pDC, p, DefaultSunkenEdge);

			if (BackImg)
			{
				LDoubleBuffer Buf(pDC);
				Tools.PaintContent(pDC, p);
				if (Hilight)
				{
					// Draw translucent white over image...
					pDC->Op(GDC_ALPHA);
					pDC->Colour(LColour(255, 255, 255, 128));
					pDC->Rectangle(&p);
				}
			}
			else
			{
				Background = cBack;
				pDC->Colour(Background);
				pDC->Box(&p);
				p.Size(1, 1);
			}

			LRect IconPos;
			if (Par->d->ImgList)
				IconPos.Set(0, 0, Par->d->ImgList->TileX()-1, Par->d->ImgList->TileY()-1);
			else
				IconPos.ZOff(Par->d->Bx-1, Par->d->By-1);
			LRegion Unpainted(p);
			
			// Center the icon
			if (IconPos.X() < p.X() - 1)
				IconPos.Offset((p.X() - IconPos.X()) >> 1, 0);
			// Offset it if the button is pressed
			if (Down)
				IconPos.Offset(2, 2);

			// Draw any icon.
			if (ImgIndex >= 0)
			{
				if (Par->d->ImgList)
				{
					// Draw cached
					if (BackImg)
					{
						Par->d->ImgList->SetDisabledAlpha(0x60);
					}
					else if (pDC->SupportsAlphaCompositing())
					{
						pDC->Colour(Background);
						pDC->Rectangle(&IconPos);
					}
					
					Par->d->ImgList->Draw(pDC, IconPos.x1, IconPos.y1, ImgIndex, Background, !e);

					Unpainted.Subtract(&IconPos);

					if (!BackImg)
					{
						// Fill in the rest of the area
						pDC->Colour(Background);
						for (LRect *r = Unpainted.First(); r; r = Unpainted.Next())
							pDC->Rectangle(r);
					}
				}
				else
				{
					// Draw a red cross indicating no icons.
					pDC->Colour(Background);
					pDC->Rectangle(&p);
					pDC->Colour(LColour::Red);
					pDC->Line(IconPos.x1, IconPos.y1, IconPos.x2, IconPos.y2);
					pDC->Line(IconPos.x2, IconPos.y1, IconPos.x1, IconPos.y2);
				}
			}
			else if (!BackImg)
			{
				Tools.PaintContent(pDC, p);
			}

			// Text
			if (Par->d->ShowTextLabels())
			{
				if (Name() && !d->Text.Length())
				{
					Layout();
				}

				if (d->Text.Length())
				{
					// Write each word centered on a different line
					int Ty = Down + Par->d->By + 2;
					LColour a = Tools.GetFore();
					LColour b = Tools.GetBack();
					if (!e)
						a = b.Mix(a);

					Par->d->Font->Colour(a, b);
					for (int i=0; i<d->Text.Length(); i++)
					{
						LDisplayString *Ds = d->Text[i];
						Ds->Draw(pDC, Down + ((X()-Ds->X())/2), Ty);
						Ty += Ds->Y();
					}
				}
			}
		}
		else
		{
			// Separator
			int Px = X()-1;
			int Py = Y()-1;

			if (BackImg)
				Tools.PaintContent(pDC, p);
			else
			{
				pDC->Colour(cBack);
				pDC->Rectangle();
			}

			LColour cLow = cBack.Mix(LColour::Black);
			LColour cHigh = cBack.Mix(LColour::White, 0.8f);

			if (X() > Y())
			{
				int c = Y()/2-1;
				pDC->Colour(cLow);
				pDC->Line(2, c, Px-2, c);
				pDC->Colour(cHigh);
				pDC->Line(2, c+1, Px-2, c+1);
			}
			else
			{
				int c = X()/2-1;
				pDC->Colour(cLow);
				pDC->Line(c, 2, c, Py-2);
				pDC->Colour(cHigh);
				pDC->Line(c+1, 2, c+1, Py-2);
			}
		}
	}

	#if 0 // def _DEBUG
	pDC->Colour(LColour(255, 0, 255));
	pDC->Box();
	#endif
}

void LToolButton::Image(int i)
{
	if (ImgIndex != i)
	{
		ImgIndex = i;
		Invalidate();
	}
}

void LToolButton::Value(int64 b)
{
	switch (Type)
	{
		case TBT_PUSH:
		{
			// do nothing... can't set value
			break;
		}

		case TBT_TOGGLE:
		{
			if (Value() != b)
			{
				Down = b != 0;
				Invalidate();
				SendNotify(LNotifyValueChanged);
			}
			break;
		}

		case TBT_RADIO:
		{
			if (GetParent() && b)
			{
				// Clear any other radio buttons that are down
				auto it = GetParent()->IterateViews();
				ssize_t CurIdx = it.IndexOf(this);
				if (CurIdx >= 0)
				{						
					for (ssize_t i=CurIdx-1; i>=0; i--)
					{
						LToolButton *But = dynamic_cast<LToolButton*>(it[i]);
						if (But->Separator())
							break;
						if (But->Type == TBT_RADIO && But->Down)
							But->Value(false);
					}

					for (size_t i=CurIdx+1; i<it.Length(); i++)
					{
						LToolButton *But = dynamic_cast<LToolButton*>(it[i]);
						if (But->Separator())
							break;
						if (But->Type == TBT_RADIO && But->Down)
							But->Value(false);
					}
				}
			}

			Down = b != 0;
			GetParent()->Invalidate();
			SendNotify(LNotifyValueChanged);
			break;
		}
	}
}

void LToolButton::SendCommand()
{
	LToolBar *t = dynamic_cast<LToolBar*>(GetParent());
	if (t) t->OnButtonClick(this);
}

void LToolButton::OnMouseClick(LMouse &m)
{
	LToolBar *ToolBar = dynamic_cast<LToolBar*>(GetParent());

	#if 0
	printf("tool button click %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
		m.x, m.y,
		m.Down(), m.Left(), m.Right(), m.Middle(),
		m.Ctrl(), m.Alt(), m.Shift(), m.Double());
	#endif

	if (m.IsContextMenu())
	{
		if (!NeedsRightClick &&
			ToolBar &&
			ToolBar->IsCustomizable())
		{		
			m.ToScreen();
			ToolBar->ContextMenu(m);
		}
	}
	else if (m.Left())
	{
		// left click action...
		if (GetId() >= 0 && Enabled())
		{
			switch (Type)
			{
				case TBT_PUSH:
				{
					bool Old = Down;
	
					Clicked = m.Down();
					Capture(m.Down());
					
					if (Old && IsOver(m))
					{
						// char *n = Name();
						if (m.Left())
						{
							SendCommand();
						}
						
						SendNotify(LNotifyActivate);
					}

					Down = m.Down();
					if (Old != Down)
					{
						Invalidate();
					}
					break;
				}

				case TBT_TOGGLE:
				{
					if (m.Down())
					{
						if (m.Left())
						{
							Value(!Down);
							SendCommand();
						}
						SendNotify(LNotifyActivate);
					}
					break;
				}

				case TBT_RADIO:
				{
					if (m.Down())
					{
						if (!Down && m.Left())
						{
							Value(true);
							SendCommand();
						}
						SendNotify(LNotifyActivate);
					}
					break;
				}
			}
		}
	}
}

void LToolButton::OnMouseEnter(LMouse &m)
{
	if (!Separator() && Enabled())
	{
		Over = true;
		Invalidate();
	}

	if (Clicked)
	{
		Value(true);
		Invalidate();
	}
	else
	{
		LToolBar *Bar = dynamic_cast<LToolBar*>(GetParent());
		if (Bar)
		{
			Bar->OnMouseEnter(m);
			if (!Bar->TextLabels() &&
				Bar->d->Tip &&
				TipId < 0)
			{
				TipId = Bar->d->Tip->NewTip(Name(), GetPos());
			}
		}

		if (GetParent())
		{
			LToolBar *ToolBar = dynamic_cast<LToolBar*>(GetParent());
			if (ToolBar) ToolBar->PostDescription(this, Name());
		}
	}
}

void LToolButton::OnMouseMove(LMouse &m)
{
}

void LToolButton::OnMouseExit(LMouse &m)
{
	if (Over)
	{
		Over = false;
		Invalidate();
	}

	if (Clicked)
	{
		Value(false);
		Invalidate();
	}
	else if (GetParent())
	{
		LToolBar *ToolBar = dynamic_cast<LToolBar*>(GetParent());
		if (ToolBar) ToolBar->PostDescription(this, "");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
LToolBar::LToolBar()
{
	d = new LToolBarPrivate;
	Name("LGI_Toolbar");
	_BorderSize = 1;
	_IsToolBar = 1;

	// Setup tool button font
	LFontType SysFontType;
	if (SysFontType.GetSystemFont("Small"))
	{
		d->Font = SysFontType.Create();
		if (d->Font)
		{
			d->Font->PointSize(MIN(d->Font->PointSize(), LSysFont->PointSize()));
			d->Font->Colour(L_TEXT);
			d->Font->Bold(false);
			d->Font->Transparent(true);
		}
	}

	d->LastIndex = 0;
	d->OwnImgList = false;
	d->ImgList = 0;

	GetCss(true)->BackgroundColor(LColour(L_MED).Mix(LColour::Black, 0.05f));
	LResources::StyleElement(this);
}

LToolBar::~LToolBar()
{
	DeleteObj(d->Tip);

	if (d->OwnImgList)
		DeleteObj(d->ImgList);

	DeleteObj(d->Font);
	DeleteObj(d);
}

void LToolBar::OnCreate()
{
	#ifndef WIN32
	AttachChildren();
	#endif
}

int LToolBar::GetBx()
{
	return d->Bx;
}

int LToolBar::GetBy()
{
	return d->By;
}

void LToolBar::ContextMenu(LMouse &m)
{
	if (IsCustomizable())
	{
		LSubMenu *Sub = new LSubMenu;
		if (Sub)
		{
			int n = 1;
			for (auto it = Children.begin(); it != Children.end(); it++, n++)
			{
				LViewI *v = *it;

				LToolButton *Btn = dynamic_cast<LToolButton*>(v);
				if (Btn && Btn->Separator())
				{
					Sub->AppendSeparator();
				}
				else
				{
					auto Item = Sub->AppendItem(v->Name(), n, true);
					if (Item)
					{
						Item->Checked(v->Visible());
					}
				}
			}
			Sub->AppendSeparator();
			auto Txt = Sub->AppendItem(LLoadString(L_TOOLBAR_SHOW_TEXT, "Show Text Labels"), 1000, true);
			Txt->Checked(d->Text);

			bool Save = false;
			int Pick = Sub->Float(this, m);
			switch (Pick)
			{
				case 1000:
				{
					d->Text = !d->Text;
					Save = true;
					SendNotify(LNotifyTableLayoutRefresh);
					break;
				}
				default:
				{
					LViewI *Ctrl = Children[Pick - 1];
					if (Ctrl)
					{
						Ctrl->Visible(!Ctrl->Visible());
						Save = true;
					}
					break;
				}
			}
			
			DeleteObj(Sub);
			
			if (Save)
			{
				LStringPipe p(256);
				p.Push((char*) (d->Text ? "text" : "no"));
				for (auto v: Children)
				{
					if (v->Visible())
					{
						p.Print(",%i", v->GetId());
					}
				}
				char *o = p.NewStr();
				if (o)
				{
					if (d->CustomDom)
					{
						LVariant v(o);
						d->CustomDom->SetValue(d->CustomProp, v);
					}
					DeleteArray(o);
				}
				
				d->FixSeparators(this);

				for (auto v: Children)
				{
					LToolButton *b = dynamic_cast<LToolButton*>(v);
					if (b && b->TipId >= 0)
					{
						d->Tip->DeleteTip(b->TipId);
						b->TipId = -1;
					}
				}

				GetWindow()->PourAll();
			}
		}
	}
}

bool LToolBar::IsCustomizable()
{
	return d->CustomDom != 0 && d->CustomProp;
}

void LToolBar::Customizable(GDom *Store, const char *Option)
{
	d->CustomDom = Store;
	d->CustomProp = Option;
	d->Customizable(this);
}

bool LToolBar::IsVertical()
{
	return d->Vertical;
}

void LToolBar::IsVertical(bool v)
{
	d->Vertical = v;
}

bool LToolBar::TextLabels()
{
	return d->Text;
}

void LToolBar::TextLabels(bool i)
{
	d->Text = i;
}

LFont *LToolBar::GetFont()
{
	return d->Font;
}

bool LToolBar::OnLayout(LViewLayoutInfo &Inf)
{
	if (Inf.Width.Min == 0)
	{
		// Calc width
		LRegion r(0, 0, 10000, 10000);
		Pour(r);
		Inf.Width.Min = X();
		Inf.Width.Max = X();
	}
	else
	{
		// Calc height
		Inf.Height.Min = Y();
		Inf.Height.Max = Y();
	}
	return true;
}

#define GetBorderSpacing()	GetCss() && GetCss()->BorderSpacing().IsValid() ? \
							GetCss()->BorderSpacing().ToPx(X(), GetFont()) : \
							1

bool LToolBar::Pour(LRegion &r)
{
	int BorderSpacing = GetBorderSpacing();
	int EndX = 0;
	int EndY = 0;
	int MaxDim = 0;
	LCssTools Tools(this);
	LRect Border = Tools.GetBorder(r);
	LRect Padding = Tools.GetPadding(r);
	int PosX = BorderSpacing + Border.x1 + Padding.x1;
	int PosY = BorderSpacing + Border.y1 + Padding.y1;

	LRect ButPos;
	for (auto But: Children)
	{
		if (But->Visible())
		{
			int Tx = 0, Ty = 0;
			LToolButton *Btn = dynamic_cast<LToolButton*>(But);

			if (d->ShowTextLabels())
			{
				if (Btn)
				{
					if (Btn->d->Text.Length() == 0)
					{
						Btn->Layout();
					}
					
					for (int i=0; i<Btn->d->Text.Length(); i++)
					{
						LDisplayString *Ds = Btn->d->Text[i];
						Tx = MAX(Ds->X() + 4, Tx);
						Ty += Ds->Y();
					}
				}
			}

			ButPos = But->GetPos();
			if (Btn)
			{
				if (Btn->Separator())
				{
					// This will be stretched out later by the code that makes
					// everything the same height.
					ButPos.ZOff(BORDER_SEPARATOR+1, BORDER_SEPARATOR+1);
				}
				else
				{
					if (Btn->Image() >= 0)
					{
						// Set initial size to the icon size
						ButPos.ZOff(d->Bx + 2, d->By + 2);
					}
					else
					{
						// Otherwise default to text size
						if (d->Vertical)
							ButPos.ZOff(0, 7);
						else
							ButPos.ZOff(7, 0);
					}
					
					Tx += 4;
					if (ButPos.X() < Tx)
					{
						// Make button wider for text label
						ButPos.x2 = Tx - 1;
					}
					
					ButPos.y2 += Ty;
				}
			}
			
			if (d->Vertical)
				MaxDim = MAX(MaxDim, ButPos.X());
			else
				MaxDim = MAX(MaxDim, ButPos.Y());

			ButPos.Offset(PosX - ButPos.x1, PosY - ButPos.y1);

			if (But->GetId() == IDM_BREAK)
			{
				ButPos.ZOff(0, 0);
				if (d->Vertical)
				{
					PosX = MaxDim;
					PosY = BORDER_SHADE + BorderSpacing;
				}
				else
				{
					PosX = BORDER_SHADE + BorderSpacing;
					PosY = MaxDim;
				}
			}
			else
			{
				if (d->Vertical)
					PosY = ButPos.y2 + BorderSpacing;
				else
					PosX = ButPos.x2 + BorderSpacing;
			}

			But->SetPos(ButPos);
		}
		else
		{
			LRect p(-100, -100, -90, -90);
			But->SetPos(p);
		}
	}

	for (auto w: Children)
	{
		LRect p = w->GetPos();

		if (d->Vertical)
		{
			if (w->X() < MaxDim)
			{
				p.x2 = p.x1 + MaxDim - 1;
				w->SetPos(p);
			}
		}
		else
		{
			if (w->Y() < MaxDim)
			{
				p.y2 = p.y1 + MaxDim - 1;
				w->SetPos(p);
			}
		}

		EndX = MAX(EndX, p.x2);
		EndY = MAX(EndY, p.y2);
	}

	d->Sx = EndX + BorderSpacing;
	d->Sy = EndY + BorderSpacing;

	d->Sx += Border.x2 + Padding.x2;
	d->Sy += Border.y2 + Padding.y2;

	LRect n;
	n.ZOff(MAX(7, d->Sx), MAX(7, d->Sy));

	LRect *Best = FindLargestEdge(r, GV_EDGE_TOP);
	if (Best)
	{
		n.Offset(Best->x1, Best->y1);
		n.Bound(Best);
		SetPos(n, true);
		
		// _Dump();
		return true;
	}

	return false;
}

void LToolBar::OnButtonClick(LToolButton *Btn)
{
	LViewI *w = (GetNotify()) ? GetNotify() : GetParent();
	if (w && Btn)
	{
		int Id = Btn->GetId();
        w->PostEvent(M_COMMAND, (LMessage::Param) Id,
			#ifdef __GTK_H__
			0
			#else
        	(LMessage::Param) Handle()
        	#endif
        	);
	}
}

int LToolBar::PostDescription(LView *Ctrl, const char *Text)
{
	if (GetParent())
	{
        return GetParent()->PostEvent(M_DESCRIBE, (LMessage::Param) Ctrl, (LMessage::Param) Text);
	}
	return 0;
}

LMessage::Result LToolBar::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_CHANGE:
		{
			if (GetParent())
				return GetParent()->OnEvent(Msg);
			break;
		}
	}

	return LView::OnEvent(Msg);
}

void LToolBar::OnPaint(LSurface *pDC)
{
	LRect c = GetClient();
	LCssTools Tools(this);
	Tools.PaintBorder(pDC, c);
	Tools.PaintPadding(pDC, c);
	Tools.PaintContent(pDC, c);
}

void LToolBar::OnMouseClick(LMouse &m)
{
}

void LToolBar::OnMouseEnter(LMouse &m)
{
	if (!d->Tip)
	{
		d->Tip = new GToolTip;
		if (d->Tip)
		{
			d->Tip->Attach(this);
		}
	}
}

void LToolBar::OnMouseExit(LMouse &m)
{
}

void LToolBar::OnMouseMove(LMouse &m)
{
}

bool LToolBar::SetBitmap(char *File, int bx, int by)
{
	bool Status = false;

	LSurface *pDC = GdcD->Load(File);
	if (pDC)
	{
		Status = SetDC(pDC, bx, by);
		DeleteObj(pDC);
	}

	return Status;
}

bool LToolBar::SetDC(LSurface *pNewDC, int bx, int by)
{
	if (d->OwnImgList)
	{
		DeleteObj(d->ImgList);
	}

	d->Bx = bx;
	d->By = by;

	if (pNewDC)
	{
		d->ImgList = new LImageList(bx, by, pNewDC);
		if (d->ImgList)
		{
			d->OwnImgList = true;
			return true;
		}
	}
	return false;
}

LImageList *LToolBar::GetImageList()
{
	return d->ImgList;
}

bool LToolBar::SetImageList(LImageList *l, int bx, int by, bool Own)
{
	if (d->OwnImgList)
		DeleteObj(d->ImgList);

	d->OwnImgList = Own;
	d->Bx = bx;
	d->By = by;
	d->ImgList = l;

	return d->ImgList != 0;
}

LToolButton *LToolBar::AppendButton(const char *Tip, int Id, int Type, int Enabled, int IconId)
{
	// bool HasIcon = IconId != TOOL_ICO_NONE;

	LToolButton *But = new LToolButton(d->Bx, d->By);
	if (But)
	{
		But->Name(Tip);
		But->SetId(Id);
		But->Type = Type;
		But->Enabled(Enabled != 0);

		if (IconId >= 0)
		{
			But->ImgIndex = IconId;
		}
		else if (IconId == TOOL_ICO_NEXT)
		{
			But->ImgIndex = d->LastIndex++;
		}
		else if (IconId == TOOL_ICO_NONE)
		{
			But->ImgIndex = -1;
		}

		AttachButton(But);
	}

	return But;
}

bool LToolBar::AppendSeparator()
{
	LToolButton *But = new LToolButton(d->Bx, d->By);
	if (But)
	{
		But->SetId(IDM_SEPARATOR);
		AttachButton(But);
		return true;
	}
	return false;
}

bool LToolBar::AppendBreak()
{
	LToolButton *But = new LToolButton(d->Bx, d->By);
	if (But)
	{
		But->SetId(IDM_BREAK);
		But->SetParent(this);
		AttachButton(But);
		return true;
	}
	return false;
}

bool LToolBar::AppendControl(LView *Ctrl)
{
	bool Status = false;
	if (Ctrl)
	{
		Ctrl->SetParent(this);
		AttachButton(Ctrl);
		Status = true;
	}
	return Status;
}

void LToolBar::Empty()
{
	for (auto But: Children)
	{
		DeleteObj(But);
	}
}

#ifdef MAC
bool LToolBar::Attach(LViewI *parent)
{
	return LLayout::Attach(parent);
}
#endif

///////////////////////////////////////////////////////////////////////
COLOUR Map(LSurface *pDC, COLOUR c)
{
	if (pDC && pDC->GetBits() <= 8)
	{
		if (pDC->IsScreen())
		{
			c = CBit(24, c);
		}
		#ifdef WIN32
		else
		{
			HPALETTE hPal = GetSystemPalette();
			if (hPal)
			{
				c = GetNearestPaletteIndex(hPal, c);
				DeleteObject(hPal);
			}
		}
		#endif
	}
	return c;
}

#ifdef WIN32
HPALETTE GetSystemPalette()
{
	HPALETTE hPal = 0;
	LOGPALETTE *Log = (LOGPALETTE*) new uchar[sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 255)];
	if (Log)
	{
		Log->palVersion = 0x300;
		Log->palNumEntries = 256;

		HDC hDC = CreateCompatibleDC(0);
		GetSystemPaletteEntries(hDC, 0, 256, Log->palPalEntry);
		DeleteDC(hDC);

		hPal = CreatePalette(Log);
	}

	return hPal;
}

bool BltBmpToBmp(HBITMAP hDest,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HBITMAP hSrc,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC DestDC = CreateCompatibleDC(0);
	HDC SrcDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hDest = (HBITMAP) SelectObject(DestDC, hDest);
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop) != 0;

		hDest = (HBITMAP) SelectObject(DestDC, hDest);
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);
	}

	if (DestDC)
	{
		DeleteDC(DestDC);
	}
	if (SrcDC)
	{
		DeleteDC(SrcDC);
	}

	return Status;
}

bool BltBmpToDc(HDC DestDC,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HBITMAP hSrc,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC SrcDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop) != 0;

		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);
	}

	if (SrcDC)
	{
		DeleteDC(SrcDC);
	}

	return Status;
}

bool BltDcToBmp(HBITMAP hDest,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HDC SrcDC,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC DestDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hDest = (HBITMAP) SelectObject(DestDC, hDest);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop) != 0;

		hDest = (HBITMAP) SelectObject(DestDC, hDest);
	}

	if (DestDC)
	{
		DeleteDC(DestDC);
	}

	return Status;
}
#endif
