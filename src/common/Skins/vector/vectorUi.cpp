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

#if defined WINNATIVE
	#define BTN_TEXT_OFFSET_Y	-1
#else
	#define BTN_TEXT_OFFSET_Y	0
#endif

#define CHECK_BORDER			2

#define CUSTOM_COLOURS			0

#define DEBUG_CHECKBOX			0

static void PaintInvalid(LSurface *pDC, LRect r)
{
	pDC->Colour(Rgb24(255, 0, 255), 24);
	pDC->Rectangle(&r);
}

// Draws straight into the screen's vector context. Each paint method opens a
// short-lived LSurface::Vector session and closes it before any LSurface text or
// blit calls, so the two never overlap.
class VectorSkin : public LSkinEngine
{
	LApp *App;
	LColour c80;
	LColour c160;

	LColour Tint(LColour back, double amt)
	{
		bool Darken = back.GetGray() >= 128;
		LColour Mixer = Darken ? LColour::Black : LColour::White;
		return back.Mix(Mixer, (float)(1.0f - amt));
	}

	// Same vertical 4/5 stop gradient look as the native Gel skin.
	// (ox, oy) is the top-left of the area in surface coordinates.
	void FillPathVec(LSurface::Vector &V, float ox, float oy, float W, float H, LColour Back, bool Down, bool Enabled = true)
	{
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

		LArray<LColourStop> Stops;
		if (Down)
		{
			Stops.Length(5);
			Stops[0].Set(0.0f, LColour(192, 192, 192));
			Stops[1].Set(0.1f, Top);
			Stops[2].Set(0.6f, Mid);
			Stops[3].Set(0.601f, Mid2);
			Stops[4].Set(1.0f, Bot);
		}
		else
		{
			Stops.Length(4);
			Stops[0].Set(0.0f, Top);
			Stops[1].Set(0.5f, Mid);
			Stops[2].Set(0.501f, Mid2);
			Stops[3].Set(1.0f, Bot);
		}

		V.ClearPath();
		V.Rect(ox, oy, W, H);
		V.Fill(LPointF(ox, oy), LPointF(ox, oy + H), Stops);

		V.ClearPath();
		V.MoveTo(ox, oy + H - 1);
		V.LineTo(ox + W - 1, oy + H - 1);
		V.Stroke(Tint(Back, 255.0 / 240.0), 1.0f);

		V.ClearPath();
		V.MoveTo(ox, oy + H - 2);
		V.LineTo(ox + W - 2, oy + H - 2);
		V.MoveTo(ox + W - 1, oy);
		V.LineTo(ox + W - 1, oy + H - 2);
		V.Stroke(Tint(Back, 198.0 / 240.0), 1.0f);
	}

	// Rounded, flat-filled button face. 'Client' is in the control's local space,
	// shifted by (ox, oy) on the surface.
	void DrawBtnVec(LSurface::Vector &V, float ox, float oy, LRect Client, LColour Back, bool Down, bool Enabled, bool Default)
	{
		// Edge
		V.ClearPath();
		V.RoundRect(ox + (float)Client.x1, oy + (float)Client.y1, (float)Client.X(), (float)Client.Y(), 6.0f);
		V.Fill(Default ? LColour(40, 40, 40) : LColour(114, 114, 114));

		// Border / fill
		LRectF fr(Client);
		float Resize = Default ? 2.0f : 1.0f;
		fr.Inset(Resize, Resize);
		if (Down)
		{
			fr.x1 = fr.x1 + 1;
			fr.y1 = fr.y1 + 1;
		}
		float Radius = 6 - Resize;

		V.ClearPath();
		V.RoundRect(ox + fr.x1, oy + fr.y1, fr.X(), fr.Y(), Radius);
		V.Fill(Enabled ? Back : Tint(Back, 230.0 / 240.0));
	}

	enum TMarkType
	{
		TNoMark,
		TCheckMark, // check mark for check boxes
		TRadioMark, // circle for radio button state
	};

	// Flat rounded-rect indicator (checkbox/radio) with an optional check/radio mark.
	// 'r' is in the indicator's local space, shifted by (ox, oy) on the surface.
	void DrawIndicatorVec(LSurface::Vector &V, float ox, float oy, LRect r, LColour Back, bool Down, bool Enabled, float radius, bool Default = false, TMarkType MarkType = TNoMark)
	{
		LColour Edge = Default ? LColour(40, 40, 40) : LColour(114, 114, 114);
		LColour Fill = Enabled ? Back : Tint(Back, 230.0 / 240.0);

		float x = ox + r.x1, y = oy + r.y1;
		float w = (float)r.X(), h = (float)r.Y();

		V.ClearPath();
		V.RoundRect(x, y, w, h, radius);
		V.Fill(Edge);

		float Inset = Default ? 2.0f : 1.0f;
		V.ClearPath();
		V.RoundRect(x + Inset, y + Inset, w - Inset * 2, h - Inset * 2, radius - Inset);
		V.Fill(Fill);

		if (MarkType != TNoMark && Down)
		{
			LRectF CheckBox = r;
			float Px = CheckBox.X() / 6;
			CheckBox.Inset(CHECK_BORDER + Px, CHECK_BORDER + Px);

			float Cx = ox + (float)(CheckBox.x1 + (CheckBox.X() / 2));
			float Cy = oy + (float)(CheckBox.y1 + (CheckBox.Y() / 2));
			float A = (float)CheckBox.X() / 6.0f;
			LColour Mark = Enabled ? c80 : c160;

			V.ClearPath();
			if (MarkType == TRadioMark)
			{
				float rad = (float)CheckBox.X() / 2.0f;
				V.RoundRect(Cx - rad, Cy - rad, rad * 2, rad * 2, rad);
				V.Fill(Mark);
			}
			else
			{
				V.MoveTo(ox + CheckBox.x1, oy + CheckBox.y1);
				V.LineTo(ox + CheckBox.x2, oy + CheckBox.y2);
				V.MoveTo(ox + CheckBox.x2, oy + CheckBox.y1);
				V.LineTo(ox + CheckBox.x1, oy + CheckBox.y2);
				V.Stroke(Mark, (float)(A * sqrt(2.0)));
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
	VectorSkin(LApp *a)
	{
		App = a;

		LColour Med = LColour(L_MED);
		double Nominal = 240.0;
		c80 = Tint(Med, 80.0/Nominal);
		c160 = Tint(Med, 160.0/Nominal);
	}

	uint32_t GetFeatures() override
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

	// Matches LSkinEngine's virtual signature, for external callers without a control.
	void FillPath(LPath *Path, LSurface *pDC, LColour Back, bool Down, bool Enabled = true) override
	{
		auto V = pDC->Vec();
		if (!V)
			return;
		FillPathVec(V, 0, 0, (float)pDC->X(), (float)pDC->Y(), Back, Down, Enabled);
	}

	// Matches LSkinEngine's pure virtual signature, for external callers without a control.
	void DrawBtn(LSurface *pDC, LRect &r, LColour Back, bool Down, bool Enabled, bool Default = false) override
	{
		auto V = pDC->Vec();
		if (!V)
			return;
		DrawBtnVec(V, 0, 0, r, Back, Down, Enabled, Default);
	}

	void OnPaint_LButton(LButton *Ctrl, LSkinState *State) override
	{
		LSurface *pDC = State->pScreen;
		LRect Bounds(0, 0, Ctrl->X() - 1, Ctrl->Y() - 1);

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

		{
			auto V = pDC->Vec();
			if (!V)
			{
				PaintInvalid(pDC, Bounds);
				return;
			}
			V.ClipRect(Bounds);
			if (NoPaint.IsValid())
			{
				V.ClearPath();
				V.Rect(0, 0, Ctrl->X(), Ctrl->Y());
				V.Fill(NoPaint);
			}
			DrawBtnVec(V, 0, 0, Ctrl->GetClient(), Back, Ctrl->Value() != 0, Ctrl->Enabled(), Ctrl->Default());
		}

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
			int Op = pDC->Op(GDC_ALPHA);
			pDC->Blt(CurX+Off, CurY+Off, State->Image);
			pDC->Op(Op);
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
					Text->Draw(pDC, CurX+Off, ty+Off+BTN_TEXT_OFFSET_Y);
				}
				else
				{
					f->Colour(LColour(L_LIGHT), Back);
					Text->Draw(pDC, CurX+Off+1, ty+Off+1+BTN_TEXT_OFFSET_Y);

					f->Colour(LColour(L_LOW), Back);
					Text->Draw(pDC, CurX+Off, ty+Off+BTN_TEXT_OFFSET_Y);
				}
				ty += Text->Y();
			}

			if (Ctrl->Focus())
			{
				LRect b(CurX-2, ty, CurX + sx + 1, ty + sy - 2);
				b.Offset(Off, Off);
				pDC->Colour(Rgb24(180, 180, 180), 24);
				pDC->Box(&b);
			}
		}
	}

	void OnPaint_ListColumn(ProcColumnPaint Callback, void *UserData, LSkinState *State) override
	{
		LSurface *pDC = State->pScreen;
		LRect r = State->Rect;

		LCssTools Tools(State->View);
		auto Ws = LColour(L_WORKSPACE);
		auto Back = Tint(Tools.GetBack(&Ws, 0), 220.0/240.0);

		{
			auto V = pDC->Vec();
			if (!V)
			{
				PaintInvalid(pDC, r);
				return;
			}
			V.ClipRect(r);
			FillPathVec(V, (float)r.x1, (float)r.y1, (float)r.X(), (float)r.Y(), Back, State->Value != 0, State->Enabled);
		}

		if (State->Value)
		{
			pDC->Colour(Rgb24(0xc0, 0xc0, 0xc0), 24);
			pDC->Line(r.x1, r.y1, r.x1, r.y2-1);
			pDC->Colour(Rgb24(0xe0, 0xe0, 0xe0), 24);
			pDC->Line(r.x1+1, r.y1+1, r.x1+1, r.y2-2);
		}

		LRect cr = r;
		cr.Inset(2, 2);
		if (Callback)
		{
			int Op = pDC->Op(GDC_ALPHA);
			Callback(UserData, pDC, cr, false);
			pDC->Op(Op);
		}
	}

	void OnPaint_LCombo(LCombo *Ctrl, LSkinState *State) override
	{
		LSurface *pDC = State->pScreen;
		LRect Bounds(0, 0, Ctrl->X() - 1, Ctrl->Y() - 1);

		// Font
		if (Ctrl->GetFont() == LSysFont)
			Ctrl->SetFont(LSysBold);

		// Back
		LColour TextDefault(L_TEXT), BackDefault(L_HIGH);
		LCssTools Tools(Ctrl->GetCss(), Ctrl->GetFont());
		LColour &Fore = Tools.GetFore(&TextDefault), &Back = Tools.GetBack();
		if (Back.IsValid())
		{
			pDC->Colour(Back);
			pDC->Rectangle(&Bounds);
		}

		auto Offset = State->Value ? 1 : 0;
		int n = 22;
		LColour DkGrey(L_DKGREY);

		{
			auto V = pDC->Vec();
			if (!V)
			{
				PaintInvalid(pDC, Bounds);
				return;
			}
			V.ClipRect(Bounds);
			DrawBtnVec(V, 0, 0, Ctrl->GetClient(), BackDefault, Offset, State->Enabled, false);

			// Downward pointing arrow, apex at (Bx,By), base 10px wide, 5px above.
			float Bx = (float)(Ctrl->X() < 26 ? Ctrl->X() / 2 : Ctrl->X() - 12) + Offset;
			float By = (float)((Ctrl->Y() + 5) >> 1) + Offset;
			V.ClearPath();
			V.MoveTo(Bx-5, By-5);
			V.LineTo(Bx+5, By-5);
			V.LineTo(Bx, By);
			V.ClosePath();
			V.Fill(State->Enabled ? Fore : DkGrey);
		}

		if (Ctrl->X() > 32)
		{
			if (auto Text = State->FirstText())
			{
				int sx = Text->X(), sy = Text->Y();
				int tx = LCombo::Pad.x1;
				int ty = (Ctrl->Y()-sy+1) >> 1;

				int Off = Offset;
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
					Text->Draw(pDC, tx+Off, ty+Off+BTN_TEXT_OFFSET_Y, &c);
				}
				else
				{
					f->Colour(LColour(L_LIGHT), LColour(L_MED));
					Text->Draw(pDC, tx+Off+1, ty+Off+1+BTN_TEXT_OFFSET_Y, &c);

					f->Colour(LColour(L_LOW), LColour(L_MED));
					Text->Draw(pDC, tx+Off, ty+Off+BTN_TEXT_OFFSET_Y, &c);
				}

				if (Ctrl->Focus() && c.X() > 4)
				{
					LRect b(tx-2, ty, tx + sx + 1, ty + sy - 2);
					b.Offset(Off, Off);
					c.Inset(-2, 0);
					b.Bound(&c);

					pDC->Colour(Rgb24(180, 180, 180), 24);
					pDC->Box(&b);
				}
			}

			// Draw separator
			int x = Ctrl->X() - n + Offset;
			pDC->Colour(Rgb24(180, 180, 180), 24);
			pDC->Line(x, 1, x, Ctrl->Y()-2);
		}
	}

	void OnPaint_LCheckBox(LCheckBox *Ctrl, LSkinState *State) override
	{
		LCssTools Tools(Ctrl);
		LColour workSpace(L_WORKSPACE);
		LColour &Back = Tools.GetBack();
		LSurface *pDC = State->pScreen;
		LRect Box = State->Rect;
		LRect Local(0, 0, Box.X() - 1, Box.Y() - 1);

		if (Back.IsValid())
		{
			pDC->Colour(Back);
			pDC->Rectangle(&Box);
		}

		{
			auto V = pDC->Vec();
			if (!V)
			{
				PaintInvalid(pDC, Box);
				return;
			}
			V.ClipRect(Box);
			DrawIndicatorVec(V, (float)Box.x1, (float)Box.y1, Local, workSpace,
							Ctrl->Value() != 0, Ctrl->Enabled(), 4.0f, false, TCheckMark);
		}

		LRect TxtBounds = State->TextBounds();

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
		LRect t(Box.X(), 0, Ctrl->X()-1, Ctrl->Y()-1);
		if (t.Valid())
		{
			DrawText(State,
					Box.X() + LTableLayout::CellSpacing,
					t.Y() > TxtBounds.Y() ? (t.Y()-TxtBounds.Y())>>1 : 0,
					t,
					Ctrl->Enabled(),
					Ctrl,
					Tools,
					DEBUG_CHECKBOX);
		}
	}

	void OnPaint_LCheckBox(LSkinState *State) override
	{
		if (!State || !State->pScreen)
			return;

		LSurface *pDC = State->pScreen;
		LRect Box = State->Rect;
		LRect Local(0, 0, Box.X() - 1, Box.Y() - 1);

		LColour back = State->parentBackground.IsValid() ? State->parentBackground : LColour(L_WORKSPACE);
		pDC->Colour(back);
		pDC->Rectangle(&Box);

		auto V = pDC->Vec();
		if (!V)
		{
			PaintInvalid(pDC, Box);
			return;
		}
		V.ClipRect(Box);
		DrawIndicatorVec(V, (float)Box.x1, (float)Box.y1, Local, LColour(L_WORKSPACE),
						State->Value != 0, State->Enabled, 4.0f, false, TCheckMark);
	}

	void OnPaint_LRadioButton(LRadioButton *Ctrl, LSkinState *State) override
	{
		LCssTools Tools(Ctrl);
		LColour workSpace(L_WORKSPACE);
		LColour &Back = Tools.GetBack();
		LSurface *pDC = State->pScreen;

		// Indicator is drawn at the left edge, vertically centred against the control.
		int W = State->Rect.X(), H = State->Rect.Y();
		LRect ico(0, 0, W - 1, H - 1);
		if (H < Ctrl->Y())
			ico.Offset(0, (Ctrl->Y() - H) >> 1);

		if (Back.IsValid())
		{
			pDC->Colour(Back);
			pDC->Rectangle(&ico);
			if (ico.y1 > 0)
				pDC->Rectangle(0, 0, ico.x2, ico.y1-1);
			if (ico.y2 < Ctrl->Y())
				pDC->Rectangle(0, ico.y2+1, ico.x2, Ctrl->Y()-1);
		}

		{
			auto V = pDC->Vec();
			if (!V)
			{
				PaintInvalid(pDC, ico);
				return;
			}
			V.ClipRect(ico);
			DrawIndicatorVec(V, (float)ico.x1, (float)ico.y1, LRect(0, 0, W - 1, H - 1), workSpace,
							Ctrl->Value() != 0, Ctrl->Enabled(), (float)W / 2.0f, false, TRadioMark);
		}

		// Draw text
		LRect t(W, 0, Ctrl->X()-1, Ctrl->Y()-1);
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
					W + 4, y,
					t,
					Ctrl->Enabled(),
					Ctrl,
					Tools);
		}
	}

	void OnPaint_LRadioButton(LSkinState *State) override
	{
		if (!State || !State->pScreen)
			return;

		LSurface *pDC = State->pScreen;
		LRect Box = State->Rect;
		LRect Local(0, 0, Box.X() - 1, Box.Y() - 1);

		LColour back = State->parentBackground.IsValid() ? State->parentBackground : LColour(L_WORKSPACE);
		pDC->Colour(back);
		pDC->Rectangle(&Box);

		auto V = pDC->Vec();
		if (!V)
		{
			PaintInvalid(pDC, Box);
			return;
		}
		V.ClipRect(Box);
		DrawIndicatorVec(V, (float)Box.x1, (float)Box.y1, Local, LColour(L_WORKSPACE),
			State->Value != 0, State->Enabled, (float)Box.X() / 2.0f, false, TRadioMark);
	}

	LFont *GetDefaultFont(char *Class) override
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
	return new VectorSkin(App);
}
