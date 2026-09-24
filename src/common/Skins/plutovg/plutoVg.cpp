#include <math.h>
#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Button.h"
#include "lgi/common/Combo.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/RadioGroup.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/CssTools.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/RectF.h"

#include "plutovg/plutovg.h"

#if defined WINNATIVE
	#define BTN_TEXT_OFFSET_Y	-1
#else
	#define BTN_TEXT_OFFSET_Y	0
#endif

#define GREY24					Rgb24(44, 44, 44)

#define CHECK_BORDER			2
#define CHECK_RADIUS			3

#define Btn_Value				0x1
#define Btn_Enabled				0x2
#define Btn_Max					((Btn_Value|Btn_Enabled)+1)

// 32bit is marginally faster to render to.
#define OsDefaultCs				System32BitColourSpace

#define CUSTOM_COLOURS			0

// Set to 1 to render DrawBtn's border/fill as a gradient (like the native Gel skin).
// Currently disabled so DrawBtn uses the same flat fill as DrawIndicator.
#define DRAWBTN_GRADIENT		0

// Wraps a plutovg surface/canvas bound to a LSurface's pixels, releasing both on scope exit.
struct PVCanvas
{
	plutovg_surface_t *Surface = nullptr;
	plutovg_canvas_t *Canvas = nullptr;

	PVCanvas(LSurface *pDC)
	{
		if (pDC && pDC->GetBits() == 32 && (*pDC)[0])
		{
			Surface = plutovg_surface_create_for_data((*pDC)[0], pDC->X(), pDC->Y(), (int)pDC->GetRowStep());
			if (Surface)
				Canvas = plutovg_canvas_create(Surface);
		}
	}

	~PVCanvas()
	{
		if (Canvas)
			plutovg_canvas_destroy(Canvas);
		if (Surface)
			plutovg_surface_destroy(Surface);
	}

	operator plutovg_canvas_t*() { return Canvas; }
	operator bool() { return Canvas != nullptr; }
};

static plutovg_color_t PVColour(LColour c)
{
	plutovg_color_t pv;
	plutovg_color_init_rgba8(&pv, c.r(), c.g(), c.b(), c.a());
	return pv;
}

class PlutoVgSkin : public LSkinEngine
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

	LColour Tint(LColour back, double amt)
	{
		bool Darken = back.GetGray() >= 128;
		LColour Mixer = Darken ? LColour::Black : LColour::White;
		return back.Mix(Mixer, (float)(1.0f - amt));
	}

	// Same vertical 4/5 stop gradient look as the native Gel skin, rendered via plutovg.
	void FillPath(LPath *Path, LSurface *pDC, LColour Back, bool Down, bool Enabled = true)
	{
		if (!pDC)
			return;

		PVCanvas Canvas(pDC);
		if (!Canvas)
			return;

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

		if (Down)
		{
			plutovg_gradient_stop_t s[] =
			{
				{0.0f, PVColour(LColour(192, 192, 192))},
				{0.1f, PVColour(Top)},
				{0.6f, PVColour(Mid)},
				{0.601f, PVColour(Mid2)},
				{1.0f, PVColour(Bot)},
			};
			plutovg_canvas_set_linear_gradient(Canvas, (float)r.x1, (float)r.y1, (float)r.x1, (float)(r.y2+1),
				PLUTOVG_SPREAD_METHOD_PAD, s, CountOf(s), NULL);
		}
		else
		{
			plutovg_gradient_stop_t s[] =
			{
				{0.0f, PVColour(Top)},
				{0.5f, PVColour(Mid)},
				{0.501f, PVColour(Mid2)},
				{1.0f, PVColour(Bot)},
			};
			plutovg_canvas_set_linear_gradient(Canvas, (float)r.x1, (float)r.y1, (float)r.x1, (float)(r.y2+1),
				PLUTOVG_SPREAD_METHOD_PAD, s, CountOf(s), NULL);
		}
		plutovg_canvas_fill_rect(Canvas, (float)r.x1, (float)r.y1, (float)r.X(), (float)r.Y());

		plutovg_canvas_set_line_width(Canvas, 1.0f);
		auto Edge1 = PVColour(Tint(Back, 255.0/240.0));
		plutovg_canvas_set_color(Canvas, &Edge1);
		plutovg_canvas_move_to(Canvas, 0.0f, (float)(pDC->Y()-1));
		plutovg_canvas_line_to(Canvas, (float)(pDC->X()-1), (float)(pDC->Y()-1));
		plutovg_canvas_stroke(Canvas);

		auto Edge2 = PVColour(Tint(Back, 198.0/240.0));
		plutovg_canvas_set_color(Canvas, &Edge2);
		plutovg_canvas_move_to(Canvas, 0.0f, (float)(pDC->Y()-2));
		plutovg_canvas_line_to(Canvas, (float)(pDC->X()-2), (float)(pDC->Y()-2));
		plutovg_canvas_move_to(Canvas, (float)(pDC->X()-1), 0.0f);
		plutovg_canvas_line_to(Canvas, (float)(pDC->X()-1), (float)(pDC->Y()-2));
		plutovg_canvas_stroke(Canvas);
	}

	// Same rounded, gradient shaded button look as the native Gel skin, rendered via plutovg.
	void DrawBtn(LSurface *pDC, LRect &r, LColour Back, bool Down, bool Enabled, bool Default = false)
	{
		if (!pDC)
			return;

		PVCanvas Canvas(pDC);
		if (!Canvas)
			return;

		LRect Client = r;

		// Edge
		{
			auto EdgeColour = PVColour(Default ? LColour(40, 40, 40) : LColour(114, 114, 114));
			plutovg_canvas_set_color(Canvas, &EdgeColour);
			plutovg_canvas_round_rect(Canvas, (float)Client.x1, (float)Client.y1, (float)Client.X(), (float)Client.Y(), 6.0f, 6.0f);
			plutovg_canvas_fill(Canvas);
		}

		// Border / fill
		{
			LRectF fr(Client);
			int Resize = Default ? 2 : 1;
			fr.Size(Resize, Resize);
			if (Down)
			{
				fr.x1 = fr.x1 + 1;
				fr.y1 = fr.y1 + 1;
			}
			float Radius = (float)(6 - Resize);

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

			#if DRAWBTN_GRADIENT

			if (Down)
			{
				plutovg_gradient_stop_t s[] =
				{
					{0.0f, PVColour(LColour(192, 192, 192))},
					{0.1f, PVColour(Top)},
					{0.6f, PVColour(Tint(Mid, 230.0/240.0))},
					{0.601f, PVColour(Tint(Mid2, 230.0/240.0))},
					{1.0f, PVColour(Tint(Bot, 230.0/240.0))},
				};
				plutovg_canvas_set_linear_gradient(Canvas, (float)fr.x1, (float)fr.y1, (float)fr.x1, (float)fr.y2,
					PLUTOVG_SPREAD_METHOD_PAD, s, CountOf(s), NULL);
			}
			else
			{
				plutovg_gradient_stop_t s[] =
				{
					{0.0f, PVColour(Top)},
					{0.5f, PVColour(Mid)},
					{0.501f, PVColour(Mid2)},
					{1.0f, PVColour(Bot)},
				};
				plutovg_canvas_set_linear_gradient(Canvas, (float)fr.x1, (float)fr.y1, (float)fr.x1, (float)fr.y2,
					PLUTOVG_SPREAD_METHOD_PAD, s, CountOf(s), NULL);
			}
			plutovg_canvas_round_rect(Canvas, (float)fr.x1, (float)fr.y1, (float)fr.X(), (float)fr.Y(), Radius, Radius);
			plutovg_canvas_fill(Canvas);

			// Rounded corner shading
			double Round = (fr.X()-13)/fr.X();
			if (Round < 0.6)
				Round = 0.6;

			uint8_t Sa = Down ? 128 : 50;
			plutovg_gradient_stop_t s3[] =
			{
				{(float)Round, PVColour(LColour(44, 44, 44, 0))},
				{1.0f, PVColour(LColour(44, 44, 44, Sa))},
			};

			float cx = (float)(fr.x1 + (fr.X()/2));
			float cy = (float)(fr.y1 + (fr.Y()/2));
			float crad = (float)hypot(fr.X()/2, fr.Y()/2);

			plutovg_canvas_set_radial_gradient(Canvas, cx, cy, crad, cx, cy, 0.0f,
				PLUTOVG_SPREAD_METHOD_PAD, s3, CountOf(s3), NULL);
			plutovg_canvas_round_rect(Canvas, (float)fr.x1, (float)fr.y1, (float)fr.X(), (float)fr.Y(), Radius, Radius);
			plutovg_canvas_fill(Canvas);

			#else

			// Flat fill, same look as DrawIndicator, until the gradient path above is re-enabled.
			LColour Fill = Enabled ? Back : Tint(Back, 230.0 / 240.0);
			auto FillC = PVColour(Fill);
			plutovg_canvas_set_color(Canvas, &FillC);
			plutovg_canvas_round_rect(Canvas, (float)fr.x1, (float)fr.y1, (float)fr.X(), (float)fr.Y(), Radius, Radius);
			plutovg_canvas_fill(Canvas);

			#endif
		}
	}

	enum TMarkType
	{
		TNoMark,
		TCheckMark, // check mark for check boxes
		TRadioMark, // circle for radio button state
	};

	// Flat rounded-rect indicator (checkbox/radio) with an optional check/radio mark, all via plutovg.
	void DrawIndicator(LSurface *pDC, LRect &r, LColour Back, bool Down, bool Enabled, float radius, bool Default = false, TMarkType MarkType = TNoMark)
	{
		PVCanvas Canvas(pDC);
		if (!Canvas)
			return;

		LColour Edge = Default ? LColour(40, 40, 40) : LColour(114, 114, 114);
		LColour Fill = Enabled ? Back : Tint(Back, 230.0 / 240.0);

		auto EdgeC = PVColour(Edge);
		plutovg_canvas_set_color(Canvas, &EdgeC);
		plutovg_canvas_round_rect(Canvas, (float)r.x1, (float)r.y1, (float)r.X(), (float)r.Y(), radius, radius);
		plutovg_canvas_fill(Canvas);

		int Inset = Default ? 2 : 1;
		auto FillC = PVColour(Fill);
		plutovg_canvas_set_color(Canvas, &FillC);
		plutovg_canvas_round_rect(Canvas,
			(float)(r.x1 + Inset), (float)(r.y1 + Inset),
			(float)(r.X() - Inset * 2), (float)(r.Y() - Inset * 2),
			radius - Inset, radius - Inset);
		plutovg_canvas_fill(Canvas);

		if (MarkType != TNoMark && Down)
		{
			LRectF CheckBox = r;
			int Px = (int)(CheckBox.X() / 6);
			CheckBox.Size(CHECK_BORDER + Px, CHECK_BORDER + Px);

			float Cx = (float)(CheckBox.x1 + (CheckBox.X() / 2));
			float Cy = (float)(CheckBox.y1 + (CheckBox.Y() / 2));
			float A = (float)CheckBox.X() / 6.0f;
			float B = (float)(CheckBox.X() / 2) - A;

			LColour Mark = Enabled ? c80 : c160;
			auto MarkC = PVColour(Mark);
			plutovg_canvas_set_color(Canvas, &MarkC);

			auto Path = plutovg_path_create();
			if (Path)
			{
				if (MarkType == TRadioMark)
				{
					plutovg_path_add_circle(Path, Cx, Cy, (float)CheckBox.X() / 2.0f);
				}
				else
				{
					plutovg_path_move_to(Path, (float)CheckBox.x1, (float)CheckBox.y1);
					plutovg_path_line_to(Path, (float)CheckBox.x1 + A, (float)CheckBox.y1);
					plutovg_path_line_to(Path, Cx, (float)CheckBox.y1 + B);
					plutovg_path_line_to(Path, (float)CheckBox.x2 - A, (float)CheckBox.y1);
					plutovg_path_line_to(Path, (float)CheckBox.x2, (float)CheckBox.y1);
					plutovg_path_line_to(Path, (float)CheckBox.x2, (float)CheckBox.y1 + A);
					plutovg_path_line_to(Path, (float)CheckBox.x2 - B, Cy);
					plutovg_path_line_to(Path, (float)CheckBox.x2, (float)CheckBox.y2 - A);
					plutovg_path_line_to(Path, (float)CheckBox.x2, (float)CheckBox.y2);
					plutovg_path_line_to(Path, (float)CheckBox.x2 - A, (float)CheckBox.y2);
					plutovg_path_line_to(Path, Cx, (float)CheckBox.y2 - B);
					plutovg_path_line_to(Path, (float)CheckBox.x1 + A, (float)CheckBox.y2);
					plutovg_path_line_to(Path, (float)CheckBox.x1, (float)CheckBox.y2);
					plutovg_path_line_to(Path, (float)CheckBox.x1, (float)CheckBox.y2 - A);
					plutovg_path_line_to(Path, (float)CheckBox.x1 + B, Cy);
					plutovg_path_line_to(Path, (float)CheckBox.x1, (float)CheckBox.y1 + A);
					plutovg_path_close(Path);
				}
				plutovg_canvas_add_path(Canvas, Path);
				plutovg_canvas_fill_path(Canvas, Path);
				plutovg_path_destroy(Path);
			}
		}
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
		auto Text = State->allText();
		LSurface *pDC = State->pScreen;
		if (Text.Length() > 0 && rcFill.X() > 3)
		{
			LRect Bounds;
			int i = 0;
			for (auto ds: Text)
			{
				auto t = dynamic_cast<LLayoutString*>(ds);
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
				i++;
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
	PlutoVgSkin(LApp *a)
	{
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

	~PlutoVgSkin()
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
				Ctrl->SetFont(LSysBold);
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

		auto Txt = State->allText();

		int ContentX = 0;
		int SpacingPx = 4;
		if (State->Image)
			ContentX += State->Image->X();
		int MaxTxt = 0;
		for (auto ds: Txt)
		{
			MaxTxt = MAX(MaxTxt, ds->X());
		}
		ContentX += MaxTxt;
		if (State->Image && Txt.Length() > 0)
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
		if (Txt.Length())
		{
			auto First = Txt[0];
			int sx = MaxTxt, sy = (int) Txt.Length() * First->Y();
			int ty = (Ctrl->Y()-sy) >> 1;

			LFont *f = First->GetFont();
			f->Transparent(true);

			for (auto Text: Txt)
			{
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

		// FillPath ignores its path parameter and fills the whole surface, so no LPath is needed here.
		static bool LastEnabled = true;
		FillPath(nullptr, &Mem, Back, State ? State->Value != 0 : false, State ? LastEnabled = State->Enabled : LastEnabled);
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
		if (!Mem.Create(Ctrl->X(), Ctrl->Y(), OsDefaultCs))
		{
			State->pScreen->Colour(Rgb24(255, 0, 255), 24);
			State->pScreen->Rectangle();
			return;
		}

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
			if (auto Text = State->FirstText())
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

				auto f = Text->GetFont();
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

	#define DEBUG_CHECKBOX 0

	void OnPaint_LCheckBox(LCheckBox *Ctrl, LSkinState *State)
	{
		int Flags = (Ctrl->Value()   ? Btn_Value   : 0) |
					(Ctrl->Enabled() ? Btn_Enabled : 0);

		// Create the bitmaps in cache if not already there
		LCssTools Tools(Ctrl);
		LColour workSpace(L_WORKSPACE);
		LColour &Back = Tools.GetBack();

		LMemDC *Temp = nullptr;
		LMemDC *&Mem = Back.IsValid() ? Temp : CheckBox[Flags];

		if (Mem && (Mem->X() != State->Rect.X() || Mem->Y() != State->Rect.Y()))
			DeleteObj(Mem);
		if (!Mem)
		{
			Mem = new LMemDC(_FL);
			if (Mem && Mem->Create(State->Rect.X(), State->Rect.Y(), OsDefaultCs))
			{
				LRect Box(0, 0, Mem->X()-1, Mem->Y()-1);
				DrawIndicator(Mem, Box,
							workSpace,
							Ctrl->Value() != 0,
							Ctrl->Enabled(),
							4.0f,
							false,
							TCheckMark);
			}
		}

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
		LCssTools Tools(Ctrl);
		LColour workSpace(L_WORKSPACE);
		LColour &Back = Tools.GetBack();

		// Create the bitmaps in cache if not already there
		LMemDC *&Mem = RadioBtn[Flags];
		if (!Mem || State->ForceUpdate)
		{
			DeleteObj(Mem);
			Mem = new LMemDC(_FL);
			if (Mem && Mem->Create(State->Rect.X(), State->Rect.Y(), OsDefaultCs))
			{
				LRect Box(0, 0, Mem->X()-1, Mem->Y()-1);
				DrawIndicator(Mem, Box,
							workSpace,
							Ctrl->Value() != 0,
							Ctrl->Enabled(),
							(float)Mem->X() / 2.0f,
							false,
							TRadioMark);
			}
		}

		// Output to screen
		if (Mem)
		{
			// Draw icon
			LRect ico;

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
	return new PlutoVgSkin(App);
}
