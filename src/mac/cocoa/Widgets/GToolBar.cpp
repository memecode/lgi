/*
**	FILE:				GToolbar.cpp
**	AUTHOR:				Matthew Allen
**	DATE:				18/8/2010
**	DESCRIPTION:		Mac native toolbar impl
**
**	Copyright (C) 2010, Matthew Allen
**		fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GToken.h"
#include "LVariant.h"

#define AttachButton(b) b->Attach(this);

COLOUR Map(LSurface *pDC, COLOUR c);

////////////////////////////////////////////////////////////////////////
LImageList *LLoadImageList(char *File, int x, int y)
{
	LImageList *ImgList = 0;
	char *Path = FileExists(File) ? NewStr(File) : LgiFindFile(File);
	if (Path)
	{
		LSurface *pDC = LoadDC(Path);
		if (pDC)
		{
			ImgList = new LImageList(x, y, pDC);
			DeleteObj(pDC);
		}

		DeleteArray(Path);
	}
	else
	{
		printf("LLoadImageList: Couldn't find '%s'\n", File);
	}

	return ImgList;
}

LToolBar *LgiLoadToolbar(LViewI *Parent, char *File, int x, int y)
{
	LToolBar *Toolbar = new LToolBar;
	if (Toolbar)
	{
		char *FileName = LgiFindFile(File);
		if (FileName)
		{
			bool Success = FileName && Toolbar->SetBitmap(FileName, x, y);
			if (!Success)
			{
				LgiMsg(Parent,
						"Can't load '%s' for the toolbar.\n"
						"You can find it in this program's archive.",
						"Lgi::LgiLoadToolbar",
						MB_OK,
						File);
				DeleteObj(Toolbar);
			}

			DeleteArray(FileName);
		}
		else
		{
			LgiMsg(Parent,
					"Can't find the graphic '%s' for the toolbar.\n"
					"You can find it in this program's archive.",
					"Lgi::LgiLoadToolbar",
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
	#if defined BEOS
	LSurface *Mask;
	#endif

	int Sx, Sy;
	LRect *Bounds;

	LImageListPriv()
	{
		Sx = 0;
		Sy = 0;
		Bounds = 0;
	}

	~LImageListPriv()
	{
		DeleteArray(Bounds);
	}
};

LImageList::LImageList(int x, int y, LSurface *pDC)
{
	d = new LImageListPriv;
	d->Sx = x;
	d->Sy = y;

	#if WINNATIVE

	/*
	if (LGetOs() != LGI_OS_WIN9X)
		d->AlphaBlend = (MsImg32_AlphaBlend)GetProcAddress(GetModuleHandle("Msimg32.dll"), "AlphaBlend");
	*/

	if (pDC &&
		Create(pDC->X(), pDC->Y(), GdcD->AlphaBlend ? 32 : GdcD->GetBits()))
	{
		RGBQUAD *Rgb = GetInfo()->bmiColors;
		bool HasAlpha = pDC->GetBits() == 32;

		if (GdcD->GetBits() <= 8)
		{
			// create palette system palette
			PALETTEENTRY Entries[256];
			HDC hDC = CreateCompatibleDC(0);
			GetSystemPaletteEntries(hDC, 0, 256, Entries);
			DeleteDC(hDC);

			GPalette *Pal = new GPalette(0, 256);
			if (Pal)
			{
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = (*Pal)[i];
					if (p)
					{
						Rgb[i].rgbRed = p->R = Entries[i].peRed;
						Rgb[i].rgbGreen = p->G = Entries[i].peGreen;
						Rgb[i].rgbBlue = p->B = Entries[i].peBlue;
						Rgb[i].rgbReserved = 0;
					}
				}

				Palette(Pal);
			}
		}

		if (pDC->GetBits() == 8 && GetBits() == 8)
		{
			// Set the palette
			HDC hDC = StartDC();
			if (hDC)
			{
				SetDIBColorTable(hDC, 0, 256, Rgb);
				EndDC();
			}

			// gotta remap here
			char Remap[256];
			GPalette *SrcPal = pDC->Palette();
			GPalette *DstPal = Palette();
			if (SrcPal && DstPal)
			{
				// build remap table
				ZeroObj(Remap);
				for (int i=0; i<256; i++)
				{
					GdcRGB *From = (*SrcPal)[i];
					if (From)
					{
						Remap[i] = DstPal->MatchRgb(Rgb24(From->R, From->G, From->B));
					}
				}

				// do blt
				for (int y=0; y<Y(); y++)
				{
					uchar *s = (*pDC)[y];
					uchar *d = (*this)[y];
					for (int x=0; x<X(); x++)
					{
						*d++ = Remap[*s++];
					}
				}
			}
		}
		else
		{
			// just standard copy blt
			Blt(0, 0, pDC);
		}

		if (GdcD->AlphaBlend && !HasAlpha)
		{
			int SrcBits = pDC->GetBits();
			if (SrcBits < 32)
			{
				Pixel32 k;
				for (int y=0; y<Y(); y++)
				{
					Pixel32 *p = (Pixel32*) (*this)[y];
					Pixel32 *e = p + X();
					if (!y) k = *p;

					while (p < e)
					{
						if (p->r == k.r &&
							p->g == k.g &&
							p->b == k.b)
						{
							p->r = 0;
							p->g = 0;
							p->b = 0;
							p->a = 0;
						}
						p++;
					}
				}
			}
		}
	}

	#elif defined BEOS

	if (pDC)
	{
		int Bits = (GdcD->GetBits() == 32) ? 32 : 15;
		if (Create(pDC->X(), pDC->Y(), Bits))
		{
			GPalette *Pal = pDC->Palette();
			if (Pal)
			{
				Palette(new GPalette(Pal));
			}

			Blt(0, 0, pDC);
			
			COLOUR InitKey = Get(0, 0);

			for (int cy=0; cy<Y(); cy++)
			{
				switch (GetBits())
				{
					case 8:
					{
						uchar Key = InitKey;
						uchar *p = (*this)[cy];
						for (int cx=0; cx<X(); cx++)
						{
							if (p[cx] == Key)
							{
								p[cx] = B_TRANSPARENT_MAGIC_CMAP8;
							}
						}
						break;
					}
					case 15:
					{
						ushort Key = InitKey;
						ushort *p = (ushort*) (*this)[cy];
						ushort Trans = B_TRANSPARENT_MAGIC_RGBA15;
						
						for (int cx=0; cx<X(); cx++)
						{
							if (p[cx] == Trans)
							{
								p[cx] = Trans+1;
							}
							else if (p[cx] == Key)
							{
								p[cx] = Trans;
							}
						}
						break;
					}
					case 32:
					{
						ulong Key = InitKey;
						ulong *p = (ulong*) ((*this)[cy]);
						for (int cx=0; cx<X(); cx++)
						{
							if (	R32(p[cx]) == R32(Key) &&
									G32(p[cx]) == G32(Key) &&
									B32(p[cx]) == B32(Key))
							{
								p[cx] = B_TRANSPARENT_MAGIC_RGBA32;
							}
						}
						break;
					}
				}

			}
		}
	}

	#else

	if (pDC &&
		Create(pDC->X(), pDC->Y(),
		#if defined(MAC) || defined(__GTK_H__)
		32
		#else
		GdcD->GetBits()
		#endif
		))
	{
		Blt(0, 0, pDC);
		
		#if defined(__GTK_H__)

		GetBounds();

		#if 0
		IsAlpha(true);
		LSurface *Alpha = AlphaDC();
		if (Alpha)
		{
			COLOUR Key = Get(0, 0);
			for (int y=0; y<Y(); y++)
			{
				uchar *a = (*Alpha)[y];
				LAssert(a);
				for (int x=0; x<X(); x++)
				{
					COLOUR c = Get(x, y);
					*a++ = c == Key ? 0 : 0xff;
				}
			}
			
			Unlock();
		}
		#elif 1
		COLOUR Key = Get(0, 0);
		for (int y=0; y<Y(); y++)
		{
			COLOUR *p = (COLOUR*) (*this)[y];
			LAssert(p);
			COLOUR *e = p + X();
			while (p < e)
			{
				if (*p == Key)
					*p = 0;
				p++;
			}
		}
		#endif
		
		#elif defined(MAC)
		
		COLOUR Key = Get(0, 0);
		uint8 kr = R32(Key);
		uint8 kg = G32(Key);
		uint8 kb = B32(Key);
		
		for (int y=0; y<Y(); y++)
		{
			Pixel32 *p = (Pixel32*) ((*this)[y]);
			Pixel32 *e = p + X();
			while (p < e)
			{
				if (p->r == kr &&
					p->g == kg &&
					p->b == kb)
				{
					p->a = 0;
					p->b = 0;
					p->g = 0;
					p->r = 0;
				}
				else
				{
					p->a = 255;
				}
				p++;
			}
		}		
		
		#endif
	}

	#endif
}

LImageList::~LImageList()
{
	DeleteObj(d);
}

void LImageList::Draw(LSurface *pDest, int Dx, int Dy, int Image, int Flags)
{
	if (pDest)
	{
		LRect r;
		r.ZOff(d->Sx-1, d->Sy-1);
		r.Offset(Image * d->Sx, 0);

		if ((*this)[0])
		{
			COLOUR Key = Get(0, 0);

			if (Flags & IMGLST_DISABLED)
			{
				COLOUR Low = CBit(pDest->GetBits(), LC_LOW);
				COLOUR High = CBit(pDest->GetBits(), LC_LIGHT);

				for (int y=0; y<r.Y(); y++)
				{
					for (int x=0; x<r.X(); x++)
					{
						#ifdef MAC
						COLOUR c = Get(r.x1 + x, r.y1 + y);
						if (A32(c))
						#else
						if (Get(r.x1 + x, r.y1 + y) != Key)
						#endif
						{
							pDest->Colour(Low);
							pDest->Set(Dx + x, Dy + y);
							pDest->Colour(High);
							pDest->Set(Dx + x + 1, Dy + y + 1);
						}
					}
				}
			}
			else
			{
				#if WINNATIVE

				BOOL Status = false;
				if (GdcD->AlphaBlend &&
					!LAppInst->IsWine())
				{
					HDC hDest = pDest->StartDC();
					HDC hSrc = StartDC();

					BLENDFUNCTION Blend;
					Blend.BlendOp = AC_SRC_OVER;
					Blend.BlendFlags = 0;
					Blend.AlphaFormat = AC_SRC_ALPHA;

					LApplicator *pApp;
					if (pDest->Op() == GDC_ALPHA &&
						(pApp = pDest->Applicator()) != 0)
					{
						int Alpha = pApp->GetVar(GAPP_ALPHA_A);
						Blend.SourceConstantAlpha = Alpha;
					}
					else
					{
						Blend.SourceConstantAlpha = 255;
					}
					
					Status = GdcD->AlphaBlend(	hDest,
												Dx, Dy,
												r.X(), r.Y(),
												hSrc,
												r.x1, r.y1,
												r.X(), r.Y(),
												Blend);
					printf("%s:%i - Status=%i\n", _FL, Status);
					if (!Status)
					{
						int ScreenBits = GdcD->GetBits();
						DWORD err = GetLastError();
						printf("%s:%i - AlphaBlend failed with %i (0x%x), Screen depth=%i\n", _FL, err, err, ScreenBits);
					}

					pDest->EndDC();
					EndDC();
				}

				if (!Status)

				#elif defined BEOS

				LScreenDC *pViewDC = dynamic_cast<LScreenDC*>(pDest);
				if (pDest->IsScreen() && pViewDC)
				{
					BView *owner = pViewDC->Handle();
					BBitmap *Bmp = GetBitmap();

					if (owner && Bmp)
					{
						int x = Image * Sx;
						BRect S(x, 0, x+Sx-1, Sy-1);
						BRect D(Dx, Dy, Dx+(Sx-1), Dy+(Sx-1));

						drawing_mode Mode = owner->DrawingMode();
						owner->SetDrawingMode(B_OP_OVER);
						owner->DrawBitmap(Bmp, S, D);
						owner->SetDrawingMode(Mode);
					}
				}
				else

				#elif defined LINUX

				/*
				LMemDC Buf(r.X(), r.Y(), GdcD->GetBits());
				LSurface *Screen = pDest;
				LRect Src;
				Src.ZOff(r.X()-1, r.Y()-1);
				Src.Offset(Dx, Dy);
				Buf.Blt(0, 0, Screen, &Src);
				int ScrX = Dx;
				int ScrY = Dy;
				pDest = &Buf;
				Dx = Dy = 0;
				*/

				pDest->Blt(Dx, Dy, this, &r);
				return;
				
				#elif defined MAC
				
				pDest->Blt(Dx, Dy, this, &r);
				if (0)

				#endif
				{
					COLOUR Map[256];
					int b = GetBits();
					if (b == 8)
					{
						GPalette *Pal = Palette();
						if (Pal)
						{
							for (int i=0; i<Pal->GetSize(); i++)
							{
								Map[i] = CBit(24, i, 8, Pal);
							}
						}

						for (int y=0; y<r.Y(); y++)
						{
							for (int x=0; x<r.X(); x++)
							{
								COLOUR c = Get(r.x1 + x, r.y1 + y);
								if (c != Key)
								{
									pDest->Colour(Map[c], 24);
									pDest->Set(Dx + x, Dy + y);
								}
							}
						}
					}
					else
					{
						for (int y=0; y<r.Y(); y++)
						{
							// True colour
							for (int x=0; x<r.X(); x++)
							{
								COLOUR c = Get(r.x1 + x, r.y1 + y);
								
								if (c != Key)
								{
									pDest->Colour(c, b);
									pDest->Set(Dx + x, Dy + y);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// Pixmap!
			pDest->Blt(Dx, Dy, this, &r);
		}
	}
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

LRect *LImageList::GetBounds()
{
	if (!d->Bounds && (*this)[0])
	{
		d->Bounds = new LRect[GetItems()];
		if (d->Bounds)
		{
			COLOUR Key = Get(0, 0);

			for (int i=0; i<GetItems(); i++)
			{
				d->Bounds[i].x1 = d->Sx-1;
				d->Bounds[i].y1 = d->Sy-1;
				d->Bounds[i].x2 = 0;
				d->Bounds[i].y2 = 0;

				int Bx = i * d->Sx;
				for (int y=0; y<d->Sy; y++)
				{
					for (int x=0; x<d->Sx; x++)
					{
						if (Get(Bx+x, y) != Key)
						{
							d->Bounds[i].x1 = min(d->Bounds[i].x1, x);
							d->Bounds[i].y1 = min(d->Bounds[i].y1, y);
							d->Bounds[i].x2 = max(d->Bounds[i].x2, x);
							d->Bounds[i].y2 = max(d->Bounds[i].y2, y);
						}
					}
				}
			}
		}
	}

	return d->Bounds;
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
	char *CustomProp;

	// bitmap cache
	LMemDC *pColour;
	LMemDC *pDisabled;

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

	void FixSeparators(LToolBar *Tb)
	{
		// Fix up separators so that no 2 separators are next to each other. I.e.
		// all the buttons between them are switched off.
		LToolButton *Last = 0;
		bool HasVis = false;
		LAutoPtr<GViewIterator> It(Tb->IterateViews());
		for (LViewI *v = It->First(); v; v = It->Next())
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
		if (o = v.Str())
		{
			GToken t(o, ",");
			if (t.Length() >= 1)
			{
				Text = stricmp(t[0], "text") == 0;
				
				// Make all controls not visible.
				LViewI *v;
				LAutoPtr<GViewIterator> It(Tb->IterateViews());
				for (v = It->First(); v; v = It->Next())
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
LToolButton::LToolButton(int Bx, int By)
{
	Type = TBT_PUSH;
	SetId(IDM_NONE);
	SetDown(false);
	Clicked = false;
	Over = false;
	ImgIndex = -1;
	NeedsRightClick = false;
	ItemRef = 0;

	LRect r(0, 0, Bx+1, By+1);
	SetPos(r);
	SetParent(0);
	TipId = -1;

	_BorderSize = 0;
}

LToolButton::~LToolButton()
{
	Text.DeleteObjects();
}

bool LToolButton::Name(char *n)
{
	bool s = LView::Name(n);

	/*
	char *i = LView::Name();
	char *o = i;
	while (*i)
	{
		if (*i != '&')
			*o++ = *i;
		*i++;
	}
	*o++ = 0;
	*/

	Text.DeleteObjects();

	return s;
}

void LToolButton::Layout()
{
	LToolBar *Par = dynamic_cast<LToolBar*>(GetParent());

	// Text
	char *s = Name();
	if (Par->d->Text &&
		Par->d->Font &&
		s)
	{
		// Write each word centered on a different line
		char Buf[256];
		strsafecpy(Buf, s, sizeof(Buf));

		GToken t(Buf, " ");
		if (t.Length() < 3)
		{
			if (t.Length() > 0)
				Text.Add(new LDisplayString(Par->d->Font, t[0]));
			if (t.Length() > 1)
				Text.Add(new LDisplayString(Par->d->Font, t[1]));
		}
		else if (t.Length() == 3)
		{
			sprintf(Buf, "%s %s", t[0], t[1]);
			LDisplayString *d1 = new LDisplayString(Par->d->Font, Buf);
			sprintf(Buf, "%s %s", t[1], t[2]);
			LDisplayString *d2 = new LDisplayString(Par->d->Font, Buf);
			if (d1 && d2)
			{
				if (d1->X() < d2->X())
				{
					DeleteObj(d2);
					Text.Add(d1);
					Text.Add(new LDisplayString(Par->d->Font, t[2]));
				}
				else
				{
					DeleteObj(d1);
					Text.Add(new LDisplayString(Par->d->Font, t[0]));
					Text.Add(d2);
				}
			}
		}
		else
		{
			//LDisplayString *Cur = new LDisplayString(Par->d->Font, Buf);
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

		// Draw Background
		COLOUR Grey = (e && Over) ? LC_HIGH : LC_MED;

		int BaseX = (X() - (2*BORDER_BUTTON) - Par->d->Bx) / 2;
		int Off = BORDER_BUTTON + ((Down) ? 1 : 0);

		if (GetId() >= 0)
		{
			// Button
			int Flags = IMGLST_GDC;

			// Draw border
			if (Down)
			{
				LThinBorder(pDC, p, SUNKEN);
			}
			else if (e && Over)
			{
				LThinBorder(pDC, p, RAISED);
			}

			pDC->Colour(Grey, 24);
			pDC->Rectangle(&p);
			pDC->ClipRgn(&p);

			if (Par->d->ImgList)
			{
				if (ImgIndex >= 0)
				{
					if (e && Over)
					{
						// Draw transparent
						Par->d->ImgList->Draw(pDC, BaseX+Off, Off, ImgIndex, Flags);
					}
					else
					{
						// Draw cached
						Par->_DrawFromCache(pDC, BaseX+Off, Off, ImgIndex, !e);
					}
				}
			}
			else
			{
				// Draw a red cross indicating no icons.
				pDC->Colour(Rgb24(255, 0, 0), 24);
				pDC->Line(p.x1, p.y1, p.x2, p.y2);
				pDC->Line(p.x2, p.y1, p.x1, p.y2);
			}

			pDC->ClipRgn(0);

			// Text
			if (Par->d->Text &&
				Par->d->Font)
			{
				if (Name() && !Text.Length())
				{
					Layout();
				}

				if (Text.Length())
				{
					// Write each word centered on a different line
					int Ty = Off + Par->d->By + 2;
					COLOUR a = e ? LC_TEXT : LC_LOW;
					COLOUR b = LC_MED;

					Par->d->Font->Colour(a, b);
					for (int i=0; i<Text.Length(); i++)
					{
						LDisplayString *Ds = Text[i];
						Ds->Draw(pDC, Off + ((X()-Ds->X())/2), Ty);
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
			pDC->Colour(Grey, 24);
			pDC->Rectangle();

			if (X() > Y())
			{
				int c = Y()/2-1;
				pDC->Colour(Map(pDC, LC_LOW), 24);
				pDC->Line(2, c, Px-2, c);
				pDC->Colour(Map(pDC, LC_LIGHT), 24);
				pDC->Line(2, c+1, Px-2, c+1);
			}
			else
			{
				int c = X()/2-1;
				pDC->Colour(Map(pDC, LC_LOW), 24);
				pDC->Line(c, 2, c, Py-2);
				pDC->Colour(Map(pDC, LC_LIGHT), 24);
				pDC->Line(c+1, 2, c+1, Py-2);
			}
		}
	}
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
				SetDown(b);
				Invalidate();
			}
			break;
		}

		case TBT_RADIO:
		{
			if (GetParent() && b)
			{
				// Clear any other radio buttons that are down
				LToolButton *But;
				GViewIterator *it = GetParent()->IterateViews();
				if (it)
				{
					for (	But = dynamic_cast<LToolButton*>(it->IndexOf(this)>=0?this:0);
							But && But->GetId() >= 0;
							But = dynamic_cast<LToolButton*>(it->Next()))
					{
						if (But->Type == TBT_RADIO &&
							But != this &&
							But->Down)
						{
							But->SetDown(false);
							But->Invalidate();
						}
					}

					for (	But = dynamic_cast<LToolButton*>(it->IndexOf(this)>=0?this:0);
							But && But->GetId() >= 0;
							But = dynamic_cast<LToolButton*>(it->Prev()))
					{
						if (But->Type == TBT_RADIO &&
							But != this &&
							But->Down)
						{
							But->SetDown(false);
							But->Invalidate();
						}
					}

					DeleteObj(it);
				}
			}

			SetDown(b);
			Invalidate();
			break;
		}
	}
}

void LToolButton::OnCommand()
{
	if (GetParent())
	{
		LToolBar *t = dynamic_cast<LToolBar*>(GetParent());
		if (t) t->OnButtonClick(this);
	}
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

	if (!NeedsRightClick &&
		ToolBar &&
		ToolBar->IsCustomizable() &&
		m.IsContextMenu())
	{
		m.ToScreen();
		ToolBar->ContextMenu(m);
	}
	else
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
						char *n = Name();
						if (m.Left())
						{
							OnCommand();
						}
						SendNotify(m.Flags);
					}

					SetDown(m.Down());
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
							OnCommand();
						}
						SendNotify(m.Flags);
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
							OnCommand();
						}
						SendNotify(m.Flags);
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
		SetDown(true);
		Invalidate();
	}
	else
	{
		LToolBar *Bar = dynamic_cast<LToolBar*>(GetParent());
		if (Bar)
		{
			Bar->OnMouseEnter(m);
			if (Bar->d->Tip && TipId < 0)
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

void LToolButton::SetDown(bool d)
{
	Down = d;
}

void LToolButton::OnMouseMove(LMouse &m)
{
	#ifdef BEOS
	if (GetParent())
	{
		LToolBar *tb = dynamic_cast<LToolBar*>(GetParent());
		if (tb)
			tb->PostDescription(this, Name());
	}
	#endif
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
		SetDown(false);
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
	ToolbarRef = 0;
	_BorderSize = 1;
	_IsToolBar = 1;
	Raised(true);

	// Setup tool button font
	LFontType SysFontType;
	if (SysFontType.GetSystemFont("Small"))
	{
		d->Font = SysFontType.Create();
		if (d->Font)
		{
			d->Font->PointSize(min(d->Font->PointSize(), LSysFont->PointSize()));
			d->Font->Colour(0);
			d->Font->Bold(false);
			d->Font->Transparent(true);
		}
	}

	d->LastIndex = 0;
	d->OwnImgList = false;
	d->ImgList = 0;
	d->pColour = 0;
	d->pDisabled = 0;
}

LToolBar::~LToolBar()
{
	DeleteObj(d->Tip);
	_BuildCache(0);

	if (d->OwnImgList)
	{
		DeleteObj(d->ImgList);
	}

	DeleteObj(d->Font);
	DeleteObj(d);
}

bool LToolBar::Attach(LViewI *parent)
{
	LWindow *Wnd = dynamic_cast<LWindow*>(parent);
	if (Wnd)
	{
		LRect r = Wnd->GetPos();
		
		// Unified view...
		OsWindow Hnd = Wnd->WindowHandle();
		if (Hnd)
		{
			#ifndef kWindowUnifiedTitleAndToolbarAttribute
			#define kWindowUnifiedTitleAndToolbarAttribute (1<<7)
			#endif
			
			OSStatus e = ChangeWindowAttributes(Hnd, kWindowUnifiedTitleAndToolbarAttribute, 0);
			if (e) printf("%s:%i - Error ChangeWindowAttributes %i\n", _FL, e);
			
			CFStringRef Nm = Utf8ToCFString(Name());
			e = HIToolbarCreate(Nm, kHIToolbarIsConfigurable, &ToolbarRef);
			if (e) printf("%s:%i - HIToolbarCreate failed %i\n", _FL, e);

			SetWindowToolbar(Hnd, ToolbarRef);
			ShowHideWindowToolbar(Hnd, true, false);
			CFRelease(ToolbarRef);

			HIToolbarSetDisplayMode(ToolbarRef,
									d->Text ? kHIToolbarDisplayModeIconAndLabel : kHIToolbarDisplayModeIconOnly);
			
			r.y2 -= 60;
			Wnd->SetPos(r);
		}
	}
	else return LLayout::Attach(parent);
}

void LToolBar::OnCreate()
{
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
		GSubMenu *Sub = new GSubMenu;
		if (Sub)
		{
			int n = 1;
			LViewI *v;
			for (v = Children.First(); v; v = Children.Next(), n++)
			{
				LToolButton *Btn = dynamic_cast<LToolButton*>(v);
				if (Btn && Btn->Separator())
				{
					Sub->AppendSeparator();
				}
				else
				{
					GMenuItem *Item = Sub->AppendItem(v->Name(), n, true);
					if (Item)
					{
						Item->Checked(v->Visible());
					}
				}
			}
			Sub->AppendSeparator();
			GMenuItem *Txt = Sub->AppendItem(LLoadString(L_TOOLBAR_SHOW_TEXT, "Show Text Labels"), 1000, true);
			Txt->Checked(d->Text);

			bool Save = false;
			int Pick = Sub->Float(this, m.x, m.y);
			switch (Pick)
			{
				case 1000:
				{
					d->Text = !d->Text;
					Save = true;
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
				for (v = Children.First(); v; v = Children.Next())
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

				for (LViewI *v = Children.First(); v; v = Children.Next())
				{
					LToolButton *b = dynamic_cast<LToolButton*>(v);
					if (b && b->TipId >= 0)
					{
						d->Tip->DeleteTip(b->TipId);
						b->TipId = -1;
					}
				}

				GetWindow()->Pour();
			}
		}
	}
}

bool LToolBar::IsCustomizable()
{
	return d->CustomDom != 0 && d->CustomProp;
}

/*
void LToolBar::Customizable(ObjProperties *Store, char *Option)
{
	d->CustomDom = 0;
	d->CustomProp = Option;
	d->Customizable(this);
}
*/

void LToolBar::Customizable(GDom *Store, char *Option)
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
	
	if (ToolbarRef)
	{
		HIToolbarSetDisplayMode(ToolbarRef,
								i ? kHIToolbarDisplayModeIconAndLabel : kHIToolbarDisplayModeIconOnly);
	}
}

LFont *LToolBar::GetFont()
{
	return d->Font;
}

void LToolBar::_BuildCache(LImageList *From)
{
	DeleteObj(d->pColour);
	DeleteObj(d->pDisabled);

	if (From)
	{
		int Bits = From->GetBits();

		if (Bits == 8)
		{
			// Generate global palette for the full colour and
			// alpha blended versions of the icons by colour reducing
			// them both on the same DC.
			LSurface *r = new LMemDC(From->X(), From->Y() * 2, 24);
			if (r)
			{
				// Background
				r->Colour(LC_MED, 24);
				r->Rectangle();

				// Draw the full colour icons
				int i;
				for (i=0; i<From->GetItems(); i++)
				{
					From->Draw(r, i * From->TileX(), 0, i);
				}

				// Draw the alpha blended icons
				r->Op(GDC_ALPHA);
				LApplicator *pApp = r->Applicator();
				if (pApp)
				{
					pApp->SetVar(GAPP_ALPHA_A, 40);
					pApp->SetVar(GAPP_ALPHA_PAL, (int)r->Palette());
				}
				for (i=0; i<From->GetItems(); i++)
				{
					From->Draw(r, i * From->TileX(), From->TileY(), i);
				}

				// Put the full colour icons in their DC
				d->pColour = new LMemDC;
				if (d->pColour &&
					d->pColour->Create(From->X(), From->Y(), Bits))
				{
					d->pColour->Palette(new GPalette(GdcD->GetGlobalColour()->GetPalette()));

					int OldType = GdcD->GetOption(GDC_REDUCE_TYPE);
					GdcD->SetOption(GDC_REDUCE_TYPE, REDUCE_ERROR_DIFFUSION);
					d->pColour->Blt(0, 0, r);
					GdcD->SetOption(GDC_REDUCE_TYPE, OldType);
				}

				// Put the alpha blended icons in their DC
				//
				// Because the colour and alpha icons have the same palette
				// they should appear fine together on the screen, as the 2
				// palettes are the same.
				d->pDisabled = new LMemDC;
				if (d->pDisabled &&
					d->pDisabled->Create(From->X(), From->Y(), Bits))
				{
					d->pDisabled->Palette(new GPalette(GdcD->GetGlobalColour()->GetPalette()));

					LRect t(0, From->Y(), From->X()-1, (From->Y() * 2)-1);

					int OldType = GdcD->GetOption(GDC_REDUCE_TYPE);
					GdcD->SetOption(GDC_REDUCE_TYPE, REDUCE_ERROR_DIFFUSION);
					d->pDisabled->Blt(0, 0, r, &t);
					GdcD->SetOption(GDC_REDUCE_TYPE, OldType);
				}

				DeleteObj(r);
			}
		}
		else
		{
			From->Lock();
			
			// True colour screen
			d->pColour = new LMemDC;
			if (d->pColour &&
				d->pColour->Create(From->X(), From->Y(), Bits))
			{
				d->pColour->Colour(LC_MED, 24);
				d->pColour->Rectangle();

				if (Bits == 32)
				{
					d->pColour->Op(GDC_ALPHA);
					d->pColour->Blt(0, 0, From);
				}
				else
				{
					for (int i=0; i<From->GetItems(); i++)
					{
						From->Draw(d->pColour, i * From->TileX(), 0, i);
					}
				}
			}

			d->pDisabled = new LMemDC;
			if (d->pDisabled &&
				d->pDisabled->Create(From->X(), From->Y(), Bits == 8 ? 24 : Bits))
			{
				d->pDisabled->Colour(LC_MED, 24);
				d->pDisabled->Rectangle();

				d->pDisabled->Op(GDC_ALPHA);
				LApplicator *pApp = d->pDisabled->Applicator();
				if (pApp)
				{
					pApp->SetVar(GAPP_ALPHA_A, 40);
					pApp->SetVar(GAPP_ALPHA_PAL, (int)d->pDisabled->Palette());
				}

				for (int i=0; i<From->GetItems(); i++)
				{
					From->Draw(d->pDisabled, i * From->TileX(), 0, i, IMGLST_GDC);
				}
			}

			From->Unlock();
			d->pColour->Unlock();
			d->pDisabled->Unlock();
		}
	}
}

void LToolBar::_DrawFromCache(LSurface *pDC, int x, int y, int Index, bool Disabled)
{
	if (pDC &&
		d->pDisabled &&
		d->pColour &&
		d->ImgList)
	{
		LRect s;
		
		s.ZOff(d->ImgList->TileX()-1, d->ImgList->TileY()-1);
		s.Offset(d->ImgList->TileX()*Index, 0);

		pDC->Blt(x, y, Disabled ? d->pDisabled : d->pColour, &s);
	}
}

bool LToolBar::Pour(LRegion &r)
{
	int PosX = BORDER_SPACER;
	int PosY = BORDER_SPACER;
	int SrcX = 0;
	int SrcY = 0;
	int EndX = 0;
	int EndY = 0;
	int MaxDim = 0;

	LRect ButPos;
	LViewI *But = Children.First();
	while (But)
	{
		if (But->Visible())
		{
			int Tx = 0, Ty = 2;

			if (d->Text)
			{
				LToolButton *Btn = dynamic_cast<LToolButton*>(But);
				if (Btn)
				{
					if (Btn->Text.Length() == 0)
					{
						Btn->Layout();
					}
					
					for (int i=0; i<Btn->Text.Length(); i++)
					{
						LDisplayString *Ds = Btn->Text[i];
						Tx = max(Ds->X() + 4, Tx);
						Ty += Ds->Y();
					}
				}
				
				/*
				char *Name = But->Name();
				if (Name)
				{
					// Process '&'s
					Name = NewStr(Name);
					if (Name)
					{
						char *Src = Name;
						char *Dest = Name;
						while (*Src)
						{
							if (Src[0] == '&')
							{
								if (Src[1] == '&') *Dest++ = '&';
								Src++;
							}
							else
							{
								*Dest++ = *Src++;
							}
						}
						*Dest++ = 0;
					}

					// Work out the space required to write each word 
					// centered on a different line
					if (d->Font &&
						d->Text)
					{
						char *n = Name;
						while (*n)
						{
							char *e;
							for (e=n; *e; e++)
							{
								if (*e == ' ')
								{
									if (e[1] && e[2] == ' ')
									{
										continue;
									}
									break;
								}
							}
							int len = (int)e-(int)n;

							// int _x = 0, _y = 0;
							// d->Font->Size(&_x, &_y, n, l);
							LDisplayString ds(d->Font, n, len);

							Tx = max(Tx, ds.X());
							Ty += ds.Y();

							n = e;
							if (*n == ' ') n++;
						}
					}
					DeleteArray(Name);
				}
				*/
			}

			ButPos = But->GetPos();
			LToolButton *Button = dynamic_cast<LToolButton*>(But);
			if (Button)
			{
				if (Button->Separator())
				{
					// This will be stretched out later by the code that makes
					// everything the same height.
					ButPos.ZOff(BORDER_SEPARATOR+1, BORDER_SEPARATOR+1);
				}
				else
				{
					if (Button->Image() >= 0)
					{
						// Set initial size to the icon size
						ButPos.ZOff(d->Bx + 2, d->By + 2);
					}
					else
					{
						// Otherwise default to text size
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
			{
				MaxDim = max(MaxDim, ButPos.X());
			}
			else
			{
				MaxDim = max(MaxDim, ButPos.Y());
			}

			ButPos.Offset(PosX - ButPos.x1, PosY - ButPos.y1);

			if (But->GetId() == IDM_BREAK)
			{
				ButPos.ZOff(0, 0);
				if (d->Vertical)
				{
					PosX = MaxDim;
					PosY = BORDER_SHADE + BORDER_SPACER;
				}
				else
				{
					PosX = BORDER_SHADE + BORDER_SPACER;
					PosY = MaxDim;
				}
			}
			else
			{
				if (d->Vertical)
				{
					PosY = ButPos.y2 + 1;
				}
				else
				{
					PosX = ButPos.x2 + 1;
				}
			}

			But->SetPos(ButPos);
			// printf("%s:%i - btn pour=%s\n", _FL, ButPos.GetStr());
		}
		else
		{
			LRect p(0, 0, 0, 0);
			But->SetPos(p);
			// printf("%s:%i - btn pour=%s\n", _FL, p.GetStr());
		}
		
		But = Children.Next();
	}

	for (LViewI *w = Children.First(); w; w = Children.Next())
	{
		LRect p = w->GetPos();

		if (d->Vertical)
		{
			if (w->X() < MaxDim)
			{
				p.x2 = p.x1 + MaxDim - 1;
				w->SetPos(p);
				// printf("%s:%i - btn pour=%s\n", _FL, p.GetStr());
			}
		}
		else
		{
			if (w->Y() < MaxDim)
			{
				p.y2 = p.y1 + MaxDim - 1;
				w->SetPos(p);
				// printf("%s:%i - btn pour=%s\n", _FL, p.GetStr());
			}
		}

		EndX = max(EndX, p.x2);
		EndY = max(EndY, p.y2);
	}

	d->Sx = EndX + BORDER_SPACER;
	d->Sy = EndY + BORDER_SPACER;

	int BorderPx = Raised() || Sunken() ? _BorderSize<<1 : 0;

	LRect n;
	n.ZOff(max(7, d->Sx)+BorderPx, max(7, d->Sy)+BorderPx);

	LRect *Best = FindLargestEdge(r, GV_EDGE_TOP);
	if (Best)
	{
		n.Offset(Best->x1, Best->y1);
		n.Bound(Best);
		SetPos(n, true);
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
		w->PostEvent(M_COMMAND, Id, (LMessage::Param) Handle());
	}
}

int LToolBar::PostDescription(LView *Ctrl, char *Text)
{
	if (GetParent())
	{
		return GetParent()->PostEvent(M_DESCRIBE, (LMessage::Param) Ctrl, (LMessage::Param) Text);
	}
	return 0;
}

int LToolBar::OnEvent(LMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CHANGE:
		{
			if (GetParent())
			{
				return GetParent()->OnEvent(Msg);
			}
			break;
		}
	}

	return LView::OnEvent(Msg);
}

void LToolBar::OnPaint(LSurface *pDC)
{
	LRect r = GetClient();

	pDC->Colour(LC_MED, 24);
	pDC->Box(&r);
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

	LSurface *pDC = LoadDC(File);
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
			_BuildCache(d->ImgList);
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
	{
		DeleteObj(d->ImgList);
	}
	d->OwnImgList = Own;
	d->Bx = bx;
	d->By = by;

	d->ImgList = l;
	_BuildCache(d->ImgList);

	return d->ImgList != 0;
}

#ifndef CFSafeRelease   
#define CFSafeRelease(_cf)                  \
    do                                      \
        {                                   \
        if ((_cf) != NULL)                  \
            {                               \
            CFRelease((CFTypeRef) _cf);     \
            _cf = NULL;                     \
            }                               \
        }                                   \
    while (0)
#endif // CFSafeRelease

IconRef GetIconRefFromCGImage(CGImageRef cgImage)
{
	UInt32              dataRGB[128 * 128] = { 0 };
    UInt8               dataA[  128 * 128] = { 0 };
    CGContextRef        cgContextRGB, cgContextA;
    CGColorSpaceRef     cgColourSpace;
    SInt32              theSize;
    IconRef             iconRef;
    IconFamilyHandle    iconHnd;
    Handle              tmpHnd;
    OSStatus            theErr;

    // Initialise ourselves
    theErr  = noErr;
    iconRef = NULL;

    // Draw the image
    //
    // Since 128x128-sized IconRefs are constructed from two blocks of data,
    // we render the image twice to obtain the image data and the mask data.
    //
    // If we could require 10.4 we could use kIconServices256PixelDataARGB
    // and create a 256x256 icon directly from the ARGB data, but for now we
    // use kCGImageAlphaOnly to support 10.3.
    cgColourSpace = CGColorSpaceCreateDeviceRGB();
    if (cgColourSpace == NULL)
        return(NULL);

    cgContextRGB  = CLBitmapContextCreate(dataRGB, 128, 128, 8, 128 * 4, cgColourSpace, kCGImageAlphaNoneSkipFirst);
    cgContextA    = CLBitmapContextCreate(dataA,   128, 128, 8, 128 * 1, NULL,          kCGImageAlphaOnly);

    if (cgContextRGB != NULL && cgContextA != NULL)
	{
		#if 1
		int Scale = 4;
		size_t w = CGImageGetWidth(cgImage);
		size_t h = CGImageGetWidth(cgImage);
		int Ox = (128 - (Scale * w)) / 2;
		int Oy = (128 - (Scale * h)) / 2;
		
        CGContextDrawImage(cgContextRGB, CGRectMake(Ox, Oy, w*Scale, h*Scale), cgImage);
        CGContextDrawImage(cgContextA,   CGRectMake(Ox, Oy, w*Scale, h*Scale), cgImage);
		#else
        CGContextDrawImage(cgContextRGB, CGRectMake(0, 0, 128, 128), cgImage);
        CGContextDrawImage(cgContextA,   CGRectMake(0, 0, 128, 128), cgImage);
		#endif
	}

    CFSafeRelease(cgColourSpace);
    CFSafeRelease(cgContextRGB);
    CFSafeRelease(cgContextA);

    // Create the icon family handle
    //
    // An icon family handle is just a fixed (big-endian) header, and tagged data.
    theSize = sizeof(OSType) + sizeof(OSType);
    iconHnd = (IconFamilyHandle) NewHandle(theSize);
    
    if (iconHnd == NULL)
        theErr = memFullErr;
    
    if (theErr == noErr)
	{
        (*iconHnd)->resourceType = EndianU32_NtoB(kIconFamilyType);
        (*iconHnd)->resourceSize = EndianU32_NtoB(theSize);
	}

    if (theErr == noErr)
	{
        theErr = PtrToHand(dataRGB, &tmpHnd, sizeof(dataRGB));
        if (theErr == noErr)
		{
            theErr = SetIconFamilyData(iconHnd, kThumbnail32BitData, tmpHnd);
            DisposeHandle(tmpHnd);
		}
	}

    if (theErr == noErr)
	{
        theErr = PtrToHand(dataA, &tmpHnd, sizeof(dataA));
        if (theErr == noErr)
		{
            theErr = SetIconFamilyData(iconHnd, kThumbnail8BitMask, tmpHnd);
            DisposeHandle(tmpHnd);
		}
	}

    // Create the IconRef
    if (theErr == noErr)
        theErr = GetIconRefFromIconFamilyPtr(*iconHnd, GetHandleSize((Handle) iconHnd), &iconRef);

    // Clean up
    if (iconHnd != NULL)
        DisposeHandle((Handle) iconHnd);
    
    return iconRef;
}

LToolButton *LToolBar::AppendButton(char *Tip, int Id, int Type, int Enabled, int IconId)
{
	bool HasIcon = IconId != TOOL_ICO_NONE;

	LToolButton *But = new LToolButton(d->Bx, d->By);
	if (But)
	{
		But->Name(Tip);
		But->SetId(Id);
		But->Type = Type;
		But->SetParent(this);
		But->Enabled(Enabled);

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

		if (ToolbarRef)
		{
			CFStringRef Nm = Utf8ToCFString(Tip);
			OSStatus e = HIToolbarItemCreate(Nm, 0, &But->ItemRef);
			if (e) printf("%s:%i - HIToolbarItemCreate failed %i\n", _FL, e);
			else
			{
				e = HIToolbarItemSetCommandID(But->ItemRef, Id);
				if (e) printf("%s:%i - HIToolbarItemSetCommandID failed %i\n", _FL, e);

				e = HIToolbarItemSetLabel(But->ItemRef, Nm);
				if (e) printf("%s:%i - HIToolbarItemSetLabel failed %i\n", _FL, e);

				IconRef Icon = 0;
				if (d->ImgList && IconId >= 0)
				{
					LRect Sub(0, 0, d->ImgList->TileX()-1, d->ImgList->TileY()-1);
					Sub.Offset(IconId * d->ImgList->TileX(), 0);
					LAutoPtr<CGImg> Img(d->ImgList->GetImg(&Sub));

					Icon = GetIconRefFromCGImage(*Img);
					if (Icon)
					{
						e = HIToolbarItemSetIconRef(But->ItemRef, Icon);
						if (e) printf("%s:%i - HIToolbarItemSetIconRef failed %i\n", _FL, e);
						ReleaseIconRef(Icon);
					}
					else printf("%s:%i - GetIconRefFromCGImage failed\n", _FL);
				}

				e = HIToolbarInsertItemAtIndex(ToolbarRef, But->ItemRef, Children.Length());
				if (e) printf("%s:%i - HIToolbarInsertItemAtIndex failed %i\n", _FL, e);
				
				AddView(But);
			}
		}
		else
		{
			AttachButton(But);
		}
	}

	return But;
}

bool LToolBar::AppendSeparator()
{
	LToolButton *But = new LToolButton(d->Bx, d->By);
	if (But)
	{
		But->SetId(IDM_SEPARATOR);
		But->SetParent(this);
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

static HIObjectClassRef CustomItem = 0;
#define kLgiToolbarItemClassID CFSTR("com.memecode.lgi.toolbaritem") 

extern
pascal
OSStatus
CarbonControlProc
(
	EventHandlerCallRef nextHandler, 
	EventRef inEvent,
	void * userData
);

LToolBar::Custom::Custom()
{
	/*
	kMyButtonToolbarItemClassID, 
	kHIToolbarItemClassID, 0, MyButtonToolbarItemHandler, 
	GetEventTypeCount( buttonEvents ), buttonEvents, 0, NULL
	*/

	OSStatus err;
	if (!CustomItem)
	{
		EventTypeSpec Events[] =
		{
			{ kEventClassHIObject, kEventHIObjectConstruct },
			{ kEventClassHIObject, kEventHIObjectInitialize },
			{ kEventClassHIObject, kEventHIObjectDestruct },
			{ kEventClassToolbarItem, kEventToolbarItemCreateCustomView },
		};

		err = HIObjectRegisterSubclass(	kLgiToolbarItemClassID,
										kHIToolbarItemClassID,
										0,
										CarbonControlProc,
										CountOf(Events),
										Events,
										0,
										&CustomItem);
		if (err) printf("%s:%i - HIObjectRegisterSubclass failed %i\n", _FL, err);
	}

	EventRef event;
	err = CreateEvent(0, kEventClassHIObject, kEventHIObjectInitialize, GetCurrentEventTime(), 0, &event); 
	if (err)
		printf("%s:%i - CreateEvent failed %i\n", _FL, err);
	else
	{
		LView *Myself = this;
		err = SetEventParameter(event, GViewThisPtr, typeVoidPtr, sizeof(Myself), &Myself); 

		UInt32 options = 0;
		CFStringRef Id = CFSTR("MyCustomIdentifier");
		SetEventParameter(	event, kEventParamToolbarItemIdentifier, 
							typeCFStringRef, sizeof(CFStringRef), &Id);
		SetEventParameter(	event, kEventParamAttributes, typeUInt32, 
							sizeof( UInt32 ), &options ); 

		err = HIObjectCreate(kLgiToolbarItemClassID, event, (HIObjectRef*)&_View);
		if (err)
			printf("%s:%i - HIObjectCreate failed %i\n", _FL, err);
	}
}

LToolBar::Custom::~Custom()
{
}

bool LToolBar::AppendControl(LView *Ctrl)
{
	bool Status = false;
	if (Ctrl)
	{
		#ifdef MAC
		Custom *Cust = dynamic_cast<Custom*>(Ctrl);
		if (Cust && ToolbarRef)
		{
			AddView(Cust);
		}
		else
		#endif
		{
			Ctrl->SetParent(this);
			AttachButton(Ctrl);
			Status = true;
		}
	}
	return Status;
}

void LToolBar::Empty()
{
	for (LViewI *But = Children.First(); But; But = Children.Next())
	{
		DeleteObj(But);
	}
}

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

