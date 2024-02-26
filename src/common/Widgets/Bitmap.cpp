#include "lgi/common/Lgi.h"
#include "lgi/common/Bitmap.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/LgiRes.h"

//////////////////////////////////////////////////////////////////////////////////
#ifdef _MT
#include "lgi/common/Thread.h"

class LBitmapThread : public LThread
{
	LBitmap *Bmp;
	char *File;
	LThread **Owner;

public:
	LBitmapThread(LBitmap *bmp, char *file, LThread **owner) : LThread("LBitmapThread")
	{
		Bmp = bmp;
		File = NewStr(file);
		Owner = owner;
		if (Owner)
		{
			*Owner = this;
		}
		Run();
	}

	~LBitmapThread()
	{
		if (Owner)
		{
			*Owner = 0;
		}
		DeleteArray(File);
	}

	int Main()
	{
		if (Bmp)
		{
			LSurface *pDC = GdcD->Load(File);
			if (pDC)
			{
				Bmp->SetDC(pDC);

				#ifndef __GTK_H__
				while (!Bmp->Handle())
					LSleep(10);
				#endif

				LRect r = Bmp->GetPos();
				r.SetSize(pDC->X()+4, pDC->Y()+4);
				Bmp->SetPos(r, true);
                Bmp->SendNotify();

				DeleteObj(pDC);
			}
		}

		return 0;
	}
};
#endif

LBitmap::LBitmap(int id, int x, int y, char *FileName, bool Async)
	: ResObject(Res_Bitmap)
{
	pDC = 0;
	pThread = 0;

	#if WINNATIVE
	SetStyle(WS_CHILD | WS_VISIBLE);
	#endif

	SetId(id);
	LRect r(0, 0, 4, 4);
	r.Offset(x, y);

	if (ValidStr(FileName))
	{
		#ifdef _MT
		if (!Async)
		{
		#endif
			pDC = GdcD->Load(FileName);
			if (pDC)
			{
				r.SetSize(pDC->X()+4, pDC->Y()+4);
			}
		#ifdef _MT
		}
		else
		{
			new LBitmapThread(this, FileName, &pThread);
		}
		#endif

	}

	SetPos(r);
	LResources::StyleElement(this);
}

LBitmap::~LBitmap()
{
	#ifdef _MT
	if (pThread)
	{
		pThread->Terminate();
		DeleteObj(pThread);
	}
	#endif
	Empty();
}

void LBitmap::Empty()
{
	if (ownDC)
	{
		DeleteObj(pDC);
	}
	else pDC = NULL;
}

bool LBitmap::OnLayout(LViewLayoutInfo &Inf)
{
	if (Inf.Width.Min)
	{
		// Height
		if (pDC)
		{
			Inf.Height.Min = pDC->Y();
			Inf.Height.Max = pDC->Y();
		}
		else
		{
			Inf.Height.Min = 32;
			Inf.Height.Max = 32;
		}
	}
	else
	{
		// Width
		if (pDC)
		{
			Inf.Width.Min = pDC->X();
			Inf.Width.Max = pDC->X();
		}
		else
		{
			Inf.Width.Min = 32;
			Inf.Width.Max = 32;
		}
	}
		
	return true;
}

void LBitmap::SetDC(LSurface *pNewDC, bool owndc)
{
	Empty();
	ownDC = owndc;
	if (pNewDC)
	{
		if (ownDC)
		{		
			pDC = new LMemDC;
			if (pDC && pDC->Create(pNewDC->X(), pNewDC->Y(), GdcD->GetColourSpace()))
			{
				LColour Bk = LColour(L_WORKSPACE);
				if (GetCss())
				{
					LCss::ColorDef b = GetCss()->BackgroundColor();
					if (b.IsValid())
						Bk = b;
				}

				pDC->Colour(Bk);
				pDC->Rectangle();

				pDC->Op(GDC_ALPHA);
				pDC->Blt(0, 0, pNewDC);

				LRect r = GetPos();
				r.SetSize(pDC->X() + 4, pDC->Y() + 4);
				SetPos(r);
				Invalidate();

				for (LViewI *p = GetParent(); p; p = p->GetParent())
				{
					LTableLayout *Tl = dynamic_cast<LTableLayout*>(p);
					if (Tl)
					{
						Tl->InvalidateLayout();
						break;
					}
				}
				
			}
		}
		else
		{
			pDC = pNewDC;
		}

		SendNotify(LNotifyTableLayoutRefresh);
	}
	Invalidate();
}

LSurface *LBitmap::GetSurface()
{
	return pDC;
}

LMessage::Result LBitmap::OnEvent(LMessage *Msg)
{
	return LView::OnEvent(Msg);
}

void LBitmap::OnPaint(LSurface *pScreen)
{
	LColour cBack = StyleColour(LCss::PropBackgroundColor, LColour(L_MED));

	LRect a = GetClient();
	pScreen->Colour(cBack);
	pScreen->Rectangle(&a);

	if (pDC)
	{
		pScreen->Op(GDC_ALPHA);
		pScreen->Blt(0, 0, pDC);
	}
}

void LBitmap::OnMouseClick(LMouse &m)
{
	if (!m.Down() && GetParent())
	{
		LDialog *Dlg = dynamic_cast<LDialog*>(GetParent());
		if (Dlg)
		{
			LNotification note(m);
			Dlg->OnNotify(this, note);
		}
	}
}
