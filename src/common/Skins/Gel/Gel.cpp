#include <math.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Path.h"
#include "lgi/common/Button.h"
#include "lgi/common/Combo.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/TableLayout.h"

#if defined WINNATIVE
	#define BTN_TEXT_OFFSET_Y	-1
#else
	#define BTN_TEXT_OFFSET_Y	0
#endif

#define GREY24					Rgb24(44, 44, 44)
#define GREY32(a)				Rgba32(44, 44, 44, a)

#define CHECK_BORDER			2
#define CHECK_RADIUS			3

#define Btn_Value				0x1
#define Btn_Enabled				0x2
#define Btn_Max					((Btn_Value|Btn_Enabled)+1)

/*
#if defined(WIN32) && !defined(LGI_SDL)
// Wine can't handle 32 bit DIB sections... :(
#define OsDefaultCs				System24BitColourSpace
#else
*/
// 32bit is marginally faster to render to.
#define OsDefaultCs				System32BitColourSpace
// #endif

#define CUSTOM_COLOURS			0

class GelSkin : public LSkinEngine
{
	LApp *App;
	LColour c80;
	LColour c160;
	LColour c172;
	LColour c222;
	LColour c232;
	LColour c253;
	LColour c255;
	LMemDC *CheckBox[Btn_Max];
	LMemDC *RadioBtn[Btn_Max];

	/*
	COLOUR LgiLighten(COLOUR c32, int Amount)
	{
		System32BitPixel *p = (System32BitPixel*)&c32;
		p->r += ((255 - p->r) * Amount) >> 8;
		p->g += ((255 - p->g) * Amount) >> 8;
		p->b += ((255 - p->b) * Amount) >> 8;
		return Rgba32(p->r, p->g, p->b, p->a);
	}

	COLOUR LgiDarken(COLOUR c32, int Amount)
	{
		System32BitPixel *p = (System32BitPixel*)&c32;
		p->r = (p->r * Amount) >> 8;
		p->g = (p->g * Amount) >> 8;
		p->b = (p->b * Amount) >> 8;
		return Rgba32(p->r, p->g, p->b, p->a);
	}
	*/

	LColour Tint(LColour back, double amt)
	{
		bool Darken = back.GetGray() >= 128;
		// auto ff = (float)(1.0f - amt);
		LColour Mixer = Darken ? LColour::Black : LColour::White;
		// printf("Darken=%i, Mixer=%s, back=%s %f\n", Darken, Mixer.GetStr(), back.GetStr(), ff);
		return back.Mix(Mixer, (float)(1.0f - amt));
	}

	void FillPath(LPath *Path, LSurface *pDC, LColour Back, bool Down, bool Enabled = true)
	{
		if (pDC)
		{
			LRect r(0, 0, pDC->X()-1, pDC->Y()-1);

			auto Top = Tint(Back, 253.0 / 240.0);
			auto Mid = Tint(Back, 232.0 / 240.0);
			auto Mid2 = Tint(Back, 222.0 / 240.0);
			auto Bot = Tint(Back, 255 / 240.0);
			if (!Enabled)
			{
				double Amt = 230.0 / 255.0;
				Top = Tint(Top, Amt);
				Mid = Tint(Mid, Amt);
				Mid2 = Tint(Mid2, Amt);
				Bot = Tint(Bot, Amt);
			}
			
			// Draw background
			LPath e;
			e.Rectangle(r.x1, r.y1, r.x2+1, r.y2+1);
			
			LPointF c1(r.x1, r.y1);
			LPointF d1(r.x1, r.y2+1);
			if (Down)
			{
				LBlendStop s1[] =
				{
					{0.0, Rgba32(192, 192, 192, 255)},
					{0.1, Top.c32()},
					{0.6, Mid.c32()},
					{0.601, Mid2.c32()},
					{1.0, Bot.c32()},
				};					
				LLinearBlendBrush b1(c1, d1, CountOf(s1), s1);
				e.Fill(pDC, b1);
			}
			else
			{
				LBlendStop s1[] =
				{
					{0.0, Top.c32()},
					{0.5, Mid.c32()},
					{0.501, Mid2.c32()},
					{1.0, Bot.c32()},
				};					
				LLinearBlendBrush b1(c1, d1, CountOf(s1), s1);
				e.Fill(pDC, b1);
			}

			pDC->Colour(Tint(Back, 255.0/240.0));
			pDC->Line(0, pDC->Y()-1, pDC->X()-1, pDC->Y()-1);
			pDC->Colour(Tint(Back, 198.0/240.0));
			pDC->Line(0, pDC->Y()-2, pDC->X()-2, pDC->Y()-2);
			pDC->Line(pDC->X()-1, 0, pDC->X()-1, pDC->Y()-2);
		}
	}
	
	void DrawBtn(LSurface *pDC, LRect &r, LColour Back, bool Down, bool Enabled, bool Default = false)
	{
		if (!pDC)
			return;

		#if LGI_SDL
		
			pDC->Colour(Base);
			pDC->Rectangle(&r);
			pDC->Colour(LColour(96, 96, 96));
			if (Down)
			{
				pDC->Line(r.x1, r.y1, r.x2, r.y1);
				pDC->Line(r.x1, r.y1, r.x1, r.y2);
			}
			else
			{
				pDC->Line(r.x1, r.y2, r.x2, r.y2);
				pDC->Line(r.x2, r.y1, r.x2, r.y2);
			}
		
		#else

			LRect Client = r;
			{
				// Edge
				LPath e;
				LRectF r(Client);
				e.RoundRect(r, 6);
			
				COLOUR EdgeColour = Default ? Rgba32(40, 40, 40, 255) : Rgba32(114, 114, 114, 255);
				LSolidBrush b(EdgeColour);
				e.Fill(pDC, b);
			}

			{
				// Border
				LPath e;
				LRectF r(Client);
				// r.y2++;
				int Resize = Default ? 2 : 1;
				r.Size(Resize, Resize);
				if (Down)
				{
					r.x1 = r.x1 + 1;
					r.y1 = r.y1 + 1;
				}
				e.RoundRect(r, 6 - Resize);

				// Fill
				LColour Top = Tint(Back, 253.0 / 240.0);
				LColour Mid = Tint(Back, 232.0 / 240.0);
				LColour Mid2 = Tint(Back, 222.0 / 240.0);
				LColour Bot = Tint(Back, 255.0 / 240.0);
				
				if (!Enabled)
				{
					auto Amt = 230.0 / 240.0;
					Top = Tint(Top, Amt);
					Mid = Tint(Mid, Amt);
					Mid2 = Tint(Mid2, Amt);
					Bot = Tint(Bot, Amt);
				}
			
				LPointF c1(r.x1, r.y1);
				LPointF d1(r.x1, r.y2);
				if (Down)
				{
					LBlendStop s1[] =
					{
						{0.0, Rgba32(192, 192, 192, 255)},
						{0.1, Top.c32()},
						{0.6, Tint(Mid, 230.0/240.0).c32()},
						{0.601, Tint(Mid2, 230.0/240.0).c32()},
						{1.0, Tint(Bot, 230.0/240.0).c32()},
					};
					LLinearBlendBrush b1(c1, d1, CountOf(s1), s1);
					e.Fill(pDC, b1);
				}
				else
				{
					LBlendStop s1[] =
					{
						{0.0, Top.c32()},
						{0.5, Mid.c32()},
						{0.501, Mid2.c32()},
						{1.0, Bot.c32()},
					};
					LLinearBlendBrush b1(c1, d1, CountOf(s1), s1);
					e.Fill(pDC, b1);
				}

				double Round = (r.X()-13)/r.X();
				if (Round < 0.6)
					Round = 0.6;
					
				int Sa = Down ? 128 : 50;
				LBlendStop s3[] =
				{
					{Round, GREY32(0)},
					{1.0, GREY32(Sa)},
				};					
				
				// Rounded corners
				LPointF c3(r.x1 + (r.X()/2), r.y1 + (r.Y()/2));
				LPointF d3(r.x1, r.y1);
				LRadialBlendBrush b3(c3, d3, CountOf(s3), s3);
				e.Fill(pDC, b3);
			}
		
		#endif
	}

	LMemDC *DrawCtrl(LViewI *Ctrl, LRect *Sz, int Flags, bool Round)
	{
		LMemDC *Mem = new LMemDC(_FL);
		if (Mem && Mem->Create(Sz ? Sz->X() : 14, Sz ? Sz->Y() : 14, OsDefaultCs))
		{
			// blank out background
			LColour Back = Ctrl->GetLView()->StyleColour(LCss::PropBackgroundColor, LColour(L_MED));
			Mem->Colour(Back);
			Mem->Rectangle();
			
			LRectF Box(0, 0, Mem->X(), Mem->Y());
			double Radius = Box.X()/2;
			LPointF Center(Box.X()/2, Box.Y()/2);

			// int Grey = R24(LC_MED);
			bool Enabled = (Flags & Btn_Enabled) != 0;

			if (Enabled)
			{
				// draw sunken border
				LRectF r = Box;
				LPath p;
				if (Round)
					p.Circle(Center, Radius);
				else
					p.RoundRect(r, CHECK_RADIUS + CHECK_BORDER);
				
				// gradient from 169,169,169 at the top through to 225,225,225
				LPointF a(0, 0), b(0, 15);
				LBlendStop s[] =
				{
					{0, c172.c32()},
					{1, c253.c32()}
				};
				LLinearBlendBrush c(a, b, 2, s);
				p.Fill(Mem, c);
			}
			
			if (Enabled)
			{
				// draw button center
				LRectF r = Box;
				r.Size(CHECK_BORDER+1, CHECK_BORDER+1);
				LPath p;
				if (Round)
					p.Circle(Center, r.X()/2);
				else
					p.RoundRect(r, CHECK_RADIUS-1);

				if (Enabled)
				{
					LPointF a(0, r.y1), b(0, r.y2);
					LBlendStop s[] =
					{
						{1.0/15.0, c255.c32()},
						{1.0/14.0, c253.c32()},
						{1, c232.c32()}
					};
					LLinearBlendBrush c(a, b, CountOf(s), s);
					p.Fill(Mem, c);
				}
				else
				{
					LSolidBrush c(LSysColour(L_MED));
					p.Fill(Mem, c);
				}
			}
			else
			{
				// draw button highlight, a white outline shifted down 1 pixel
				LRectF r = Box;
				r.Size(CHECK_BORDER, CHECK_BORDER);
				r.Offset(0, 1);
				LPointF Cntr = Center;
				Cntr.y = Cntr.y + 1;
				
				LPath p;
				if (Round)
					p.Circle(Cntr, r.X()/2);
				else
					p.RoundRect(r, CHECK_RADIUS);
				r.Size(1, 1);
				if (Round)
					p.Circle(Cntr, r.X()/2);
				else
					p.RoundRect(r, CHECK_RADIUS-1);
				
				LSolidBrush c(LSysColour(L_LIGHT));
				p.Fill(Mem, c);
			}

			{
				// draw button outline
				LRectF r = Box;
				r.Size(CHECK_BORDER, CHECK_BORDER);
				LPath p;
				if (Round)
					p.Circle(Center, r.X()/2);
				else
					p.RoundRect(r, CHECK_RADIUS);
				r.Size(1, 1);
				if (Round)
					p.Circle(Center, r.X()/2);
				else
					p.RoundRect(r, CHECK_RADIUS-1);
				
				LSolidBrush c(Enabled ? c80 : c160);
				p.Fill(Mem, c);
			}
			
			
			if (Flags & Btn_Value)
			{
				// draw the check mark
				LRectF r = Box;
				int Px = (int) (Box.X()/6);
				r.Size(CHECK_BORDER+Px, CHECK_BORDER+Px);

				double Cx = r.x1 + (r.X() / 2);
				double Cy = r.y1 + (r.Y() / 2);
				double A = r.X() / 6;
				double B = (r.X() / 2) - A;

				LPath p;
				if (Round)
				{
					p.Circle(Center, r.X()/3);
				}
				else
				{
					p.MoveTo(r.x1, r.y1);
					p.LineTo(r.x1 + A, r.y1);
					p.LineTo(Cx, r.y1 + B);
					p.LineTo(r.x2 - A, r.y1);
					p.LineTo(r.x2, r.y1);
					p.LineTo(r.x2, r.y1 + A);
					p.LineTo(r.x2 - B, Cy);
					p.LineTo(r.x2, r.y2 - A);
					p.LineTo(r.x2, r.y2);
					p.LineTo(r.x2 - A, r.y2);
					p.LineTo(Cx, r.y2 - B);
					p.LineTo(r.x1 + A, r.y2);
					p.LineTo(r.x1, r.y2);
					p.LineTo(r.x1, r.y2 - A);
					p.LineTo(r.x1 + B, Cy);
					p.LineTo(r.x1, r.y1 + A);
					p.LineTo(r.x1, r.y1);
				}
				
				LSolidBrush c(Enabled ? c80 : c160);
				p.Fill(Mem, c);
			}
		}
		
		return Mem;
	}

	void DrawText(LSkinState *State, int x, int y, LRect &rcFill, bool Enabled, LView *Ctrl, LCssTools &Tools, bool Debug = false)
	{
		LCss::ColorDef CssFore, CssBack;
		LColour Fore = Tools.GetFore(), Back = Tools.GetBack(), Light, Low;

		if (!Enabled)
		{
			Light = LColour(L_LIGHT);
			Low = LColour(L_LOW);
		}

		LRegion Rgn;
		Rgn = rcFill;
		LArray<LDisplayString*> *Text = State->AllText();
		LSurface *pDC = State->pScreen;
		if (Text && Text->Length() > 0 && rcFill.X() > 3)
		{
			LRect Bounds;
			for (unsigned i=0; i<Text->Length(); i++)
			{
				LLayoutString *t = dynamic_cast<LLayoutString*>((*Text)[i]);
				if (!t)
					break;
				LRect c;
				c.ZOff(t->X() - 1, t->Y() - 1);
				c.Offset(x + (t->Fx >> LDisplayString::FShift), y + t->y);
				Rgn.Subtract(&c);
				
				if (i)
					Bounds.Union(&c);
				else
					Bounds = c;

				LFont *f = t->GetFont();
				if (Enabled)
				{
					f->Colour(Fore, Back);
					f->Transparent(!Back.IsValid());
					t->Draw(pDC, c.x1, c.y1, &c);
				}
				else
				{
					f->Transparent(!Back.IsValid());
					f->Colour(Light, Back);
					t->Draw(pDC, c.x1 + 1, c.y1 + 1, &c);

					f->Transparent(true);
					f->Colour(Low, Back);
					t->Draw(pDC, c.x1, c.y1, &c);
				}
			}

			if ((Ctrl->Focus() && Enabled) || Debug)
			{
				pDC->Colour(Debug ? LColour::Blue : LColour(L_MIDGREY));
				pDC->Box(&Bounds);
			}
		}
		
		if (Back.IsValid())
		{
			pDC->Colour(Back);
			for (LRect *rc = Rgn.First(); rc; rc = Rgn.Next())
				pDC->Rectangle(rc);
		}
	}

public:
	GelSkin(LApp *a)
	{
		// printf("Skin @ %i bits\n", GdcD->GetBits());
		App = a;
		ZeroObj(CheckBox);
		ZeroObj(RadioBtn);

		LColour Med = LColour(L_MED);
		double Nominal = 240.0;
		c80 = Tint(Med, 80.0/Nominal);
		c160 = Tint(Med, 160.0/Nominal);
		c172 = Tint(Med, 172.0/Nominal);
		c222 = Tint(Med, 222.0/Nominal);
		c232 = Tint(Med, 232.0/Nominal);
		c253 = Tint(Med, 252.0/Nominal);
		c255 = Tint(Med, 255.0/Nominal);
	}
	
	~GelSkin()
	{
		int i;
		for (i=0; i<CountOf(CheckBox); i++)
			DeleteObj(CheckBox[i]);
		for (i=0; i<CountOf(RadioBtn); i++)
			DeleteObj(RadioBtn[i]);
	}

	uint32_t GetFeatures()
	{
		return
				#if CUSTOM_COLOURS
				GSKIN_COLOUR |
				#endif
				GSKIN_BUTTON |
				GSKIN_COMBO |
				GSKIN_LISTCOL |
				GSKIN_CHECKBOX |
				GSKIN_RADIO;
	}

	#if CUSTOM_COLOURS
	uint32 GetColour(int i)
	{
		switch (i)
		{
			// LC_BLACK
			case 0: return Rgb24(0, 0, 0);
			// LC_DKGREY
			case 1: return Rgb24(88, 88, 88);
			// LC_MIDGREY
			case 2: return Rgb24(180, 180, 180);
			// LC_LTGREY
			case 3:	return Rgb24(222, 222, 222);
			// LC_WHITE
			case 4: return Rgb24(255, 255, 255);

			// LC_SHADOW
			case 5: return Rgb24(64, 64, 64);
			// LC_LOW
			case 6: return Rgb24(128, 128, 128);
			#ifdef LINUX
			// LC_MED
			case 7: return Rgb24(230, 230, 230);
			#else
			// LC_MED
			case 7: return Rgb24(216, 216, 216);
			#endif
			// LC_HIGH
			case 8: return Rgb24(0xf6, 0xf6, 0xf6);
			// LC_LIGHT
			case 9: return Rgb24(0xff, 0xff, 0xff);
			
			// LC_DIALOG
			case 10: return Rgb24(216, 216, 216);
			// LC_WORKSPACE
			case 11: return Rgb24(0xff, 0xff, 0xff);
			// LC_TEXT
			case 12: return Rgb24(0, 0, 0);
			// LC_SELECTION
			case 13: return Rgb24(0x4a, 0x59, 0xa5);
			// LC_SEL_TEXT
			case 14: return Rgb24(0xff, 0xff, 0xff);

			// LC_ACTIVE_TITLE
			case 15: return Rgb24(0, 0, 0x80);
			// LC_ACTIVE_TITLE_TEXT
			case 16: return Rgb24(0xff, 0xff, 0xff);
			// LC_INACTIVE_TITLE
			case 17: return Rgb24(0x80, 0x80, 0x80);
			// LC_INACTIVE_TITLE_TEXT
			case 18: return Rgb24(0x40, 0x40, 0x40);

			// LC_MENU_BACKGROUN
			case 19: return Rgb24(222, 222, 222);
			// LC_MENU_TEXT
			case 20: return Rgb24(0, 0, 0);
		}
		
		return 0;
	}
	#endif
	
	void OnPaint_LButton(LButton *Ctrl, LSkinState *State)
	{
		LMemDC Mem(_FL);
		if (!Mem.Create(Ctrl->X(), Ctrl->Y(), OsDefaultCs))
		{
			State->pScreen->Colour(Rgb24(255, 0, 255), 24);
			State->pScreen->Rectangle();
			return;
		}

		// Font
		if (Ctrl->GetFont() == LSysFont)
		{
			if (LSysFont == LSysBold)
			{
				LAssert(!"these shouldn't be the same.");
			}
			else
			{
				// printf("%s:%i - setting btn to bold, fnt=%p -> %p\n", _FL, Ctrl->GetFont(), LSysBold);
				Ctrl->SetFont(LSysBold);
				// printf("	fnt=%p\n", Ctrl->GetFont());
			}
		}

		// Background
		LCssTools Tools(Ctrl->GetCss(), Ctrl->GetFont());
		LColour DefaultBack(L_HIGH);
		LColour &Fore = Tools.GetFore(), &Back = Tools.GetBack(&DefaultBack);
		LColour NoPaint(LSysColour(L_MED));
		if (Ctrl->GetCss())
		{
			LCss::ColorDef np = Ctrl->GetCss()->NoPaintColor();
			if (np.Type == LCss::ColorRgb)
				NoPaint.Set(np.Rgb32, 32);
			else
				NoPaint.Empty();
		}
		#if defined(WINDOWS) || defined(HAIKU)
		if (!NoPaint.IsValid())
		{
			// We have to paint something otherwise we'll get garbage
			auto p = Ctrl->GetParent();
			NoPaint = LColour(L_MED);
			if (p) // Use the parent's background?
				NoPaint = p->GetLView()->StyleColour(LCss::PropBackgroundColor, NoPaint);
		}
		#endif
		if (NoPaint.IsValid())
			Mem.Colour(NoPaint);
		else
			Mem.Colour(0, 32);
		Mem.Rectangle();
		
		DrawBtn(&Mem,
				Ctrl->GetClient(),
				Back,
				Ctrl->Value() != 0,
				Ctrl->Enabled(),
				Ctrl->Default());
		
		LSurface *Out = &Mem;
		
		LArray<LDisplayString*> *Txt = State->AllText();

		int ContentX = 0;
		int SpacingPx = 4;
		if (State->Image)
			ContentX += State->Image->X();
		int MaxTxt = 0;
		if (Txt)
		{
			for (unsigned i=0; i<Txt->Length(); i++)
			{
				MaxTxt = MAX(MaxTxt, (*Txt)[i]->X());
			}
			ContentX += MaxTxt;
		}
		if (State->Image && Txt && Txt->Length() > 0)
			ContentX += SpacingPx;

		int CurX = (Ctrl->X() - ContentX) >> 1;
		int Off = Ctrl->Value() ? 1 : 0;
		if (State->Image)
		{
			int CurY = (Ctrl->Y() - State->Image->Y()) >> 1;
			int Op = Out->Op(GDC_ALPHA);
			Out->Blt(CurX+Off, CurY+Off, State->Image);
			Out->Op(Op);
			CurX += State->Image->X() + SpacingPx;
		}
		if (Txt && Txt->Length() > 0)
		{
			LDisplayString *First = (*Txt)[0];
			int sx = MaxTxt, sy = (int) Txt->Length() * First->Y();
			int ty = (Ctrl->Y()-sy) >> 1;

			LFont *f = First->GetFont();
			f->Transparent(true);
			
			for (unsigned i=0; i<Txt->Length(); i++)
			{
				LDisplayString *Text = (*Txt)[i];
				if (Ctrl->Enabled())
				{
					f->Colour(Fore, Back);
					Text->Draw(Out, CurX+Off, ty+Off+BTN_TEXT_OFFSET_Y);
				}
				else
				{
					f->Colour(LColour(L_LIGHT), Back);
					Text->Draw(Out, CurX+Off+1, ty+Off+1+BTN_TEXT_OFFSET_Y);

					f->Colour(LColour(L_LOW), Back);
					Text->Draw(Out, CurX+Off, ty+Off+BTN_TEXT_OFFSET_Y);
				}
				ty += Text->Y();
			}
			
			if (Ctrl->Focus())
			{
				LRect b(CurX-2, ty, CurX + sx + 1, ty + sy - 2);
				b.Offset(Off, Off);
				Out->Colour(Rgb24(180, 180, 180), 24);
				Out->Box(&b);
			}
		}
		
		int Op = State->pScreen->Op(GDC_ALPHA);
		State->pScreen->Blt(0, 0, &Mem);
		State->pScreen->Op(Op);
	}

	void OnPaint_ListColumn(ProcColumnPaint Callback, void *UserData, LSkinState *State)
	{
		// Setup memory context
		LRect r = State->Rect;
		LMemDC Mem(_FL, r.X(), r.Y(), OsDefaultCs);
		if (!Mem[0])
			return;

		LCssTools Tools(State->View);
		auto Ws = LColour(L_WORKSPACE);
		auto Back = Tint(Tools.GetBack(&Ws, 0), 220.0/240.0);
		r.Offset(-r.x1, -r.y1);

		LPath e;
		e.Rectangle(r.x1, r.y1, r.x2, r.y2);
		static bool LastEnabled = true;			
		FillPath(&e, &Mem, Back, State ? State->Value != 0 : false, State ? LastEnabled = State->Enabled : LastEnabled);
		if (State && State->Value)
		{
			Mem.Colour(Rgb24(0xc0, 0xc0, 0xc0), 24);
			Mem.Line(r.x1, r.y1, r.x1, r.y2-1);
			Mem.Colour(Rgb24(0xe0, 0xe0, 0xe0), 24);
			Mem.Line(r.x1+1, r.y1+1, r.x1+1, r.y2-2);
		}
		r.Inset(2, 2);
		if (Callback)
		{
			Mem.Op(GDC_ALPHA);
			Callback(UserData, &Mem, r, false);
		}
		State->pScreen->Blt(State->Rect.x1, State->Rect.y1, &Mem);
	}

	void OnPaint_LCombo(LCombo *Ctrl, LSkinState *State)
	{
		LMemDC Mem(_FL);
		if (Mem.Create(Ctrl->X(), Ctrl->Y(), OsDefaultCs))
		{
			// Font
			if (Ctrl->GetFont() == LSysFont)
				Ctrl->SetFont(LSysBold);

			// Back
			LColour TextDefault(L_TEXT), BackDefault(L_HIGH);
			LCssTools Tools(Ctrl->GetCss(), Ctrl->GetFont());
			LColour &Fore = Tools.GetFore(&TextDefault), &Back = Tools.GetBack();
			if (Back.IsValid())
			{
				Mem.Colour(Back);
				Mem.Rectangle();
			}

			DrawBtn(&Mem, Ctrl->GetClient(), BackDefault, false, State->Enabled);
			
			int n = 22;
			LColour DkGrey(L_DKGREY);
			
			if (Ctrl->X() > 32)
			{
				LDisplayString *Text = State->FirstText();
				if (Text)
				{
					int sx = Text->X(), sy = Text->Y();
					int tx = LCombo::Pad.x1;
					int ty = (Ctrl->Y()-sy+1) >> 1;
						
					int Off = 0;
					LRect c = Ctrl->GetClient();
					c.x1 += 8;
					c.x2 -= n + 3;
					
					int Cx = Ctrl->X();
					int PadX = LCombo::Pad.x1 + LCombo::Pad.x2;
					if (Text->X() > PadX)
					{
						// Make the text fit
						Text->TruncateWithDots(Cx - PadX);
					}

					LFont *f = Text->GetFont();
					f->Transparent(true);
					if (Ctrl->Enabled())
					{
						f->Colour(Fore, Back);
						Text->Draw(&Mem, tx+Off, ty+Off+BTN_TEXT_OFFSET_Y, &c);
					}
					else
					{
						f->Colour(LColour(L_LIGHT), LColour(L_MED));
						Text->Draw(&Mem, tx+Off+1, ty+Off+1+BTN_TEXT_OFFSET_Y, &c);

						f->Colour(LColour(L_LOW), LColour(L_MED));
						Text->Draw(&Mem, tx+Off, ty+Off+BTN_TEXT_OFFSET_Y, &c);
					}
					
					if (Ctrl->Focus() && c.X() > 4)
					{
						LRect b(tx-2, ty, tx + sx + 1, ty + sy - 2);
						b.Offset(Off, Off);
						c.Inset(-2, 0);
						b.Bound(&c);

						Mem.Colour(Rgb24(180, 180, 180), 24);
						Mem.Box(&b);
					}
				}
				
				// Draw separator
				Mem.Colour(Rgba32(180, 180, 180, 255), 32);
				Mem.Line(Mem.X()-n, 1, Mem.X()-n, Mem.Y()-2);
			}
			
			Mem.Colour(State->Enabled ? Fore : DkGrey);
			int Bx = Mem.X() < 26 ? Mem.X()/2 : Mem.X()-13, By = (Mem.Y() + 4) >> 1;
			for (int i=0; i<5; i++)
			{				
				Mem.Line(Bx-i, By-i, Bx+i, By-i);
			}
			
			State->pScreen->Blt(0, 0, &Mem);
		}
		else
		{
			State->pScreen->Colour(Rgb24(255, 0, 255), 24);
			State->pScreen->Rectangle();
		}
	}

	#define DEBUG_CHECKBOX 0

	void OnPaint_LCheckBox(LCheckBox *Ctrl, LSkinState *State)
	{
		int Flags = (Ctrl->Value()   ? Btn_Value   : 0) |
					(Ctrl->Enabled() ? Btn_Enabled : 0);
		
		// Create the bitmaps in cache if not already there
		LCssTools Tools(Ctrl);
		LColour &Back = Tools.GetBack();

		LMemDC *Temp = 0;
		LMemDC *&Mem = Back.IsValid() ? Temp : CheckBox[Flags];
		
		if (Mem && (Mem->X() != State->Rect.X() || Mem->Y() != State->Rect.Y()))
			DeleteObj(Mem);
		if (!Mem)
			Mem = DrawCtrl(Ctrl, &State->Rect, Flags, false);

		LRect TxtBounds = State->TextBounds();

		// Output to screen
		auto pDC = State->pScreen;
		if (Mem)
		{
			LRect &Box = State->Rect;
			pDC->Blt(Box.x1, Box.y1, Mem);

			LRect Box1(Box.x1, 0, Box.x2, Box.y1 - 1);
			LRect Box2(Box.x1, Box.y2 + 1, Box.x2, Ctrl->Y()-1);
			pDC->Colour(Back.IsValid() ? Back : LColour(L_MED));
			if (Box.y1 > 0)
				pDC->Rectangle(&Box1);
			if (Box.y2 < Ctrl->Y() - 1)
				pDC->Rectangle(&Box2);
			#if DEBUG_CHECKBOX
				pDC->Colour(LColour::Red);
				if (Box.y1 > 0)
					pDC->Box(&Box1);
				pDC->Colour(LColour::Green);
				if (Box.y2 < Ctrl->Y() - 1)
					pDC->Box(&Box2);
			#endif

			// Draw text
			LRect t(Mem->X(), 0, Ctrl->X()-1, Ctrl->Y()-1);
			if (t.Valid())
			{
				DrawText(State,
						Mem->X() + LTableLayout::CellSpacing,
						t.Y() > TxtBounds.Y() ? (t.Y()-TxtBounds.Y())>>1 : 0,
						t,
						(Flags & Btn_Enabled) != 0,
						Ctrl,
						Tools,
						DEBUG_CHECKBOX);
			}
		}
		else
		{
			pDC->Colour(LColour(255, 0, 255));
			pDC->Rectangle();
		}

		DeleteObj(Temp);
	}

	void OnPaint_LRadioButton(LRadioButton *Ctrl, LSkinState *State)
	{
		int Flags = (Ctrl->Value() ? Btn_Value : 0) |
					(Ctrl->Enabled() ? Btn_Enabled : 0);
		
		// Create the bitmaps in cache if not already there
		LMemDC *&Mem = RadioBtn[Flags];
		if (!Mem || State->ForceUpdate)
		{
			DeleteObj(Mem);
			Mem = DrawCtrl(Ctrl, &State->Rect, Flags, true);
		}

		// Output to screen
		if (Mem)
		{
			// Draw icon
			LRect ico;
			LCssTools Tools(Ctrl);
			LColour &Back = Tools.GetBack();

			ico.ZOff(Mem->X()-1, Mem->Y()-1);
		    if (ico.Y() < Ctrl->Y())
		        ico.Offset(0, (Ctrl->Y() - ico.Y()) >> 1);
			State->pScreen->Blt(ico.x1, ico.y1, Mem);
			if (Back.IsValid())
			{
				State->pScreen->Colour(Back);
				if (ico.y1 > 0)
					State->pScreen->Rectangle(0, 0, ico.x2, ico.y1-1);
				if (ico.y2 < Ctrl->Y())
					State->pScreen->Rectangle(0, ico.y2+1, ico.x2, Ctrl->Y()-1);
			}

			// Draw text
			LRect t(Mem->X(), 0, Ctrl->X()-1, Ctrl->Y()-1);
			if (t.Valid())
			{
				int y = 0;
				if (State->TextObjects())
				{
					LDisplayString *ds = State->FirstText();
					if (ds && t.Y() > ds->Y())
					{
						y = (t.Y() - ds->Y()) >> 1;
					}					
				}
				
				DrawText(State,
						Mem->X() + 4, y,
						t,
						(Flags & Btn_Enabled) != 0,
						Ctrl,
						Tools);
			}
		}
		else
		{
			State->pScreen->Colour(Rgb24(255, 0, 255), 24);
			State->pScreen->Rectangle();
		}
	}

	LFont *GetDefaultFont(char *Class)
	{
		if (Class && stricmp(Class, Res_Button) == 0)
		{
			return LSysBold;
		}

		return LSysFont;
	}
};

LSkinEngine *
CreateSkinEngine(class LApp *App)
{
	return new GelSkin(App);
}

