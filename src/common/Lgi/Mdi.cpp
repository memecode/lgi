#include "lgi/common/Lgi.h"
#include "lgi/common/Mdi.h"
#include <stdio.h>
#include "lgi/common/DisplayString.h"

#define DEBUG_MDI			0

enum LMdiDrag
{
	DragNone	= 0,
	DragMove	= 1 << 1,
	DragLeft	= 1 << 2,
	DragTop		= 1 << 3,
	DragRight	= 1 << 4,
	DragBottom	= 1 << 5,
	DragClose	= 1 << 6,
	DragSystem	= 1 << 7
};

class LMdiChildPrivate
{
public:
	LMdiChild *Child;
	LRect Title;
	#if MDI_TAB_STYLE
	LRect Tab, Btn;
	int Order;
	#else
	LRect Close;
	LRect System;
	#endif
	
	int Fy;	
	LMdiDrag Drag;	
	int Ox, Oy;
	bool CloseDown;
	bool CloseOnUp;
	
	LMdiChildPrivate(LMdiChild *c)
	{
		CloseOnUp = false;
		Child = c;
		CloseDown = false;
		Fy = LSysFont->GetHeight() + 1;
		Title.ZOff(-1, -1);
		
		#if MDI_TAB_STYLE
		Tab.ZOff(-1, -1);
		Order = -1;
		#else
		Close.ZOff(-1, -1);
		System.ZOff(-1, -1);
		#endif
	}
	#if !MDI_TAB_STYLE
	LMdiDrag HitTest(int x, int y)
	{
		LMdiDrag Hit = DragNone;
		
		if (Child->WindowFromPoint(x, y) == Child)
		{
			if (!Child->GetClient().Overlap(x, y))
			{
				if (System.Overlap(x, y))
				{
					Hit = DragSystem;
				}
				else if (Close.Overlap(x, y))
				{
					Hit = DragClose;
				}
				else if (Title.Overlap(x, y))
				{
					Hit = DragMove;
				}
				else
				{
					LRect c = Child->LLayout::GetClient();
					int Corner = 24;
					if (x < c.x1 + Corner)
					{
						(int&)Hit |= DragLeft;
					}
					if (x > c.x2 - Corner)
					{
						(int&)Hit |= DragRight;
					}
					if (y < c.y1 + Corner)
					{
						(int&)Hit |= DragTop;
					}
					if (y > c.y2 - Corner)
					{
						(int&)Hit |= DragBottom;
					}
				}
			}
		}
		return Hit;
	}
	#endif
};

class LMdiParentPrivate
{
public:
	bool InOnPosChange;
	bool Btn;
	LRect Tabs;
	LRect Content;
	::LArray<LMdiChild*> Children;
	
	LMdiParentPrivate()
	{
		Btn = false;
		InOnPosChange = false;
		Tabs.ZOff(-1, -1);
		Content.ZOff(-1, -1);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
LMdiChild::LMdiChild()
{
	d = new LMdiChildPrivate(this);
	LPoint m(100, d->Fy + 8);
	SetMinimumSize(m);
	#if WINNATIVE
	SetStyle(GetStyle() | WS_CLIPSIBLINGS);
	#endif
}

LMdiChild::~LMdiChild()
{
	Detach();
	DeleteObj(d);
}

bool LMdiChild::Attach(LViewI *p)
{
	#if MDI_TAB_STYLE
	LMdiParent *par = dynamic_cast<LMdiParent*>(p);
	if (!par)
		return false;	
	if (par->d->Children.HasItem(this))
	{
		LgiTrace("%s:%i - Already attached:\n", _FL);
		#ifdef BEOS
		for (unsigned i=0; i<par->d->Children.Length(); i++)
		{
			LMdiChild *c = par->d->Children[i];
			printf("[%i]=%p %p %p %s\n",
				i, c, c->Handle(), c->Handle()?c->Handle()->Window():0, c->Name());
		}
		#endif
		
		return false;
	}
	
	par->d->Children.Add(this);
	d->Order = par->GetNextOrder();
	#endif
	bool s = LLayout::Attach(p);
	if (s)
		AttachChildren();
	return s;
}

bool LMdiChild::Detach()
{
	#if MDI_TAB_STYLE
	LMdiParent *par = dynamic_cast<LMdiParent*>(GetParent());
	if (par)
	{
		LAssert(par->d->Children.HasItem(this));
		par->d->Children.Delete(this);
	}
	#endif
	return LLayout::Detach();
}

const char *LMdiChild::Name()
{
	return LView::Name();
}

bool LMdiChild::Name(const char *n)
{
	bool s = LView::Name(n);
	
	#if MDI_TAB_STYLE
	if (GetParent())
		GetParent()->Invalidate();
	#else
	Invalidate((LRect*)0, false, true);
	#endif
	
	return s;
}

bool LMdiChild::PourAll()
{
	LRect c = GetClient();
	#if !MDI_TAB_STYLE
	c.Size(2, 2);
	#endif
	LRegion Client(c);
	LRegion Update;
	#if DEBUG_MDI
	LgiTrace("    LMdiChild::Pour() '%s', cli=%s, vis=%i, children=%i\n",
		Name(), c.GetStr(), Visible(), Children.Length());
	#endif
	for (auto w: Children)
	{
		if (Visible())
		{
			LRect OldPos = w->GetPos();
			Update.Union(&OldPos);
			if (w->Pour(Client))
			{
				if (!w->IsAttached())
					w->Attach(this);
				if (!w->Visible())
					w->Visible(true);
				Client.Subtract(&w->GetPos());
				Update.Subtract(&w->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
		else
		{
			w->Visible(false);
		}
		#if DEBUG_MDI
		LgiTrace("      %s, vis=%i\n", w->GetClass(), w->Visible());
		#endif
	}
	return true;
}

void LMdiChild::OnTitleClick(LMouse &m)
{
	if (!m.IsContextMenu() && m.Down())
	{
		Raise();
	}
}

void LMdiChild::OnButtonClick(LMouse &m)
{
	if (m.Down())
	{
		if (OnRequestClose(false))
		{
			Quit();
		}
	}
}

void LMdiChild::OnPaintButton(LSurface *pDC, LRect &rc)
{
	// Default: Draw little 'x' for closing the MDI child
	LRect r = rc;
	pDC->Colour(LColour(192,192,192));
	
	r.Size(3, 3);
	pDC->Line(r.x1, r.y1, r.x2, r.y2);
	pDC->Line(r.x1+1, r.y1, r.x2, r.y2-1);
	pDC->Line(r.x1, r.y1+1, r.x2-1, r.y2);
	pDC->Line(r.x2, r.y1, r.x1, r.y2);
	pDC->Line(r.x2-1, r.y1, r.x1, r.y2-1);
	pDC->Line(r.x2, r.y1+1, r.x1+1, r.y2);
}

#if MDI_TAB_STYLE

int LMdiChild::GetOrder()
{
	return d->Order;
}

#else

LRect &LMdiChild::GetClient(bool ClientSpace)
{
	static LRect r;
	
	r = LLayout::GetClient(ClientSpace);
	r.Size(3, 3);
	r.y1 += d->Fy + 2;
	r.Size(1, 1);
	
	return r;
}

void LMdiChild::OnPaint(LSurface *pDC)
{
	LRect p = LLayout::GetClient();
	// Border
	LWideBorder(pDC, p, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Box(&p);
	p.Size(1, 1);
	// Title bar
	char *n = Name();
	if (!n) n = "";
	d->Title.ZOff(p.X()-1, d->Fy + 1);
	d->Title.Offset(p.x1, p.y1);
	d->System = d->Title;
	d->System.x2 = d->System.x1 + d->System.Y() - 1;
	// Title text
	bool Top = false;
	if (GetParent())
	{
		GViewIterator *it = GetParent()->IterateViews();
		Top = it->Last() == (LViewI*)this;
		DeleteObj(it);
	}
	
	LSysFont->Colour(Top ? LC_ACTIVE_TITLE_TEXT : LC_INACTIVE_TITLE_TEXT, Top ? LC_ACTIVE_TITLE : LC_INACTIVE_TITLE);
	LSysFont->Transparent(false);
	LDisplayString ds(LSysFont, n);
	ds.Draw(pDC, d->System.x2 + 1, d->Title.y1 + 1, &d->Title);
	
	// System button
	LRect r = d->System;
	r.Size(2, 1);
	pDC->Colour(LC_BLACK, 24);
	pDC->Box(&r);
	r.Size(1, 1);
	pDC->Colour(LC_WHITE, 24);
	pDC->Rectangle(&r);
	pDC->Colour(LC_BLACK, 24);
	for (int k=r.y1 + 1; k < r.y2 - 1; k += 2)
	{
		pDC->Line(r.x1 + 1, k, r.x2 - 1, k);
	}
	// Close button
	d->Close = d->Title;
	d->Close.x1 = d->Close.x2 - d->Close.Y() + 1;
	d->Close.Size(2, 2);
	r = d->Close;
	LWideBorder(pDC, r, d->CloseDown ? SUNKEN : RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);
	int Cx = d->Close.x1 + (d->Close.X() >> 1) - 1 + d->CloseDown;
	int Cy = d->Close.y1 + (d->Close.Y() >> 1) - 1 + d->CloseDown;
	int s = 3;
	pDC->Colour(Rgb24(108,108,108), 24);
	pDC->Line(Cx-s, Cy-s+1, Cx+s-1, Cy+s);
	pDC->Line(Cx-s+1, Cy-s, Cx+s, Cy+s-1);
	pDC->Line(Cx-s, Cy+s-1, Cx+s-1, Cy-s);
	pDC->Line(Cx-s+1, Cy+s, Cx+s, Cy-s+1);
	pDC->Colour(LC_BLACK, 24);
	pDC->Line(Cx-s, Cy-s, Cx+s, Cy+s);
	pDC->Line(Cx-s, Cy+s, Cx+s, Cy-s);
	// Client
	LRect Client = GetClient();
	Client.Size(-1, -1);
	pDC->Colour(LC_MED, 24);
	pDC->Box(&Client);
	
	if (!Children.First())
	{
		Client.Size(1, 1);
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(&Client);
	}
	
	Pour();
}

void LMdiChild::OnMouseClick(LMouse &m)
{
	if (m.Left())
	{
		Raise();
		if (m.Down())
		{
			LRect c = LLayout::GetClient();
			d->Drag = DragNone;
			d->Ox = d->Oy = -1;
			
			d->Drag = d->HitTest(m.x, m.y);
			if (d->Drag)
			{
				Capture(true);
			}
			if (d->Drag == DragSystem)
			{
				if (m.Double())
				{
					d->CloseOnUp = true;
				}
				else
				{
					/*
					LPoint p(d->System.x1, d->System.y2+1);
					PointToScreen(p);
					GSubMenu *Sub = new GSubMenu;
					if (Sub)
					{
						Sub->AppendItem("Close", 1, true);
						Close = Sub->Float(this, p.x, p.y, true) == 1;
						DeleteObj(Sub);
					}
					*/
				}
			}
			else if (d->Drag == DragClose)
			{
				d->CloseDown = true;
				Invalidate(&d->Close);
			}
			else if (d->Drag == DragMove)
			{
				d->Ox = m.x - c.x1;
				d->Oy = m.y - c.y1;
			}
			else
			{
				if (d->Drag & DragLeft)
				{
					d->Ox = m.x - c.x1;
				}
				if (d->Drag & DragRight)
				{
					d->Ox = m.x - c.x2;
				}
				if (d->Drag & DragTop)
				{
					d->Oy = m.y - c.y1;
				}
				if (d->Drag & DragBottom)
				{
					d->Oy = m.y - c.y2;
				}
			}
		}
		else
		{
			if (IsCapturing())
			{
				Capture(false);
			}
			if (d->CloseOnUp)
			{
				d->Drag = DragNone;
				if (OnRequestClose(false))
				{
					Quit();
					return;
				}
			}
			else if (d->Drag == DragClose)
			{
				if (d->Close.Overlap(m.x, m.y))
				{
					d->Drag = DragNone;
					d->CloseDown = false;
					Invalidate(&d->Close);
					
					if (OnRequestClose(false))
					{
						Quit();
						return;
					}
				}
				else printf("%s:%i - Outside close btn.\n", _FL);
			}
			else
			{
				d->Drag = DragNone;
			}
		}
	}
}

LCursor LMdiChild::GetCursor(int x, int y)
{
	LMdiDrag Hit = d->HitTest(x, y);
	if (Hit & DragLeft)
	{
		if (Hit & DragTop)
		{
			return LCUR_SizeFDiag;
		}
		else if (Hit & DragBottom)
		{
			return LCUR_SizeBDiag;
		}
		else
		{
			return LCUR_SizeHor;
		}
	}
	else if (Hit & DragRight)
	{
		if (Hit & DragTop)
		{
			return LCUR_SizeBDiag;
		}
		else if (Hit & DragBottom)
		{
			return LCUR_SizeFDiag;
		}
		else
		{
			return LCUR_SizeHor;
		}
	}
	else if ((Hit & DragTop) || (Hit & DragBottom))
	{
		return LCUR_SizeVer;
	}
	
	return LCUR_Normal;
}

void LMdiChild::OnMouseMove(LMouse &m)
{
	if (IsCapturing())
	{
		LRect p = GetPos();
		LPoint Min = GetMinimumSize();
		if (d->Drag == DragClose)
		{
			bool Over = d->Close.Overlap(m.x, m.y);
			if (Over ^ d->CloseDown)
			{
				d->CloseDown = Over;
				Invalidate(&d->Close);
			}
		}
		else
		{
			// GetMouse(m, true);
			LPoint n(m.x, m.y);
			n.x += GetPos().x1;
			n.y += GetPos().y1;
			// GetParent()->PointToView(n);
				
			if (d->Drag == DragMove)
			{
				p.Offset(n.x - d->Ox - p.x1, n.y - d->Oy - p.y1);
				if (p.x1 < 0) p.Offset(-p.x1, 0);
				if (p.y1 < 0) p.Offset(0, -p.y1);
			}
			else
			{
				if (d->Drag & DragLeft)
				{
					p.x1 = min(p.x2 - Min.x, n.x - d->Ox);
					if (p.x1 < 0) p.x1 = 0;
				}
				if (d->Drag & DragRight)
				{
					p.x2 = max(p.x1 + Min.x, n.x - d->Ox);
				}
				if (d->Drag & DragTop)
				{
					p.y1 = min(p.y2 - Min.y, n.y - d->Oy);
					if (p.y1 < 0) p.y1 = 0;
				}
				if (d->Drag & DragBottom)
				{
					p.y2 = max(p.y1 + Min.y, n.y - d->Oy);
				}
			}
		}
		SetPos(p);
	}
}
#endif

#if defined __GTK_H__
using namespace Gtk;
#endif
void LMdiChild::Raise()
{
	LMdiParent *p = dynamic_cast<LMdiParent*>(GetParent());
	if (p)
	{
		#if MDI_TAB_STYLE
		
		#else
			#if defined __GTK_H__
				GtkWidget *wid = Handle();
				GdkWindow *wnd = wid ? GDK_WINDOW(wid->window) : NULL;
				if (wnd)
					gdk_window_raise(wnd);
				else
					LAssert(0);
			#elif WINNATIVE
				SetWindowPos(Handle(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			#else
				#error "Impl me."
			#endif
		#endif
		/*
		printf("%s:%i %p=%s %p=%s\n", _FL,
			p->d->Children.Last(),
			p->d->Children.Last()->Name(),
			this,
			Name());
		*/
			
		if (p->d->Children.Last() != this)
		{
			p->d->Children.Delete(this);
			p->d->Children.Add(this);
			p->d->Tabs.ZOff(-1, -1);
			p->OnPosChange();
			Focus(true);
		}
		#if DEBUG_MDI
		LgiTrace("LMdiChild::Raise() '%s'\n", Name());
		#endif
	}
}

void LMdiChild::Lower()
{
	LMdiParent *p = dynamic_cast<LMdiParent*>(GetParent());
	if (p)
	{
		#if MDI_TAB_STYLE
		#else
			#if defined __GTK_H__
				GtkWidget *wid = Handle();
				GdkWindow *wnd = wid ? GDK_WINDOW(wid->window) : NULL;
				if (wnd)
					gdk_window_lower(wnd);
				else
					LAssert(0);
			#elif WINNATIVE
				SetWindowPos(Handle(), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			#else
				#error "Impl me."
			#endif
		#endif
		if (p->d->Children[0] != this)
		{
			p->d->Children.Delete(this);
			p->d->Children.AddAt(0, this);
			p->d->Tabs.ZOff(-1, -1);
			p->OnPosChange();
		}
		
		#if DEBUG_MDI
		LgiTrace("LMdiChild::Lower() '%s'\n", Name());
		#endif
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
LMdiParent::LMdiParent()
{
	d = new LMdiParentPrivate;
	SetPourLargest(true);
}

LMdiParent::~LMdiParent()
{
	if (GetWindow())
	{
		GetWindow()->UnregisterHook(this);
	}
	
	DeleteObj(d);
}

bool LMdiParent::HasButton()
{
	return d->Btn;
}

void LMdiParent::HasButton(bool b)
{
	d->Btn = b;
	if (IsAttached())
		Invalidate();
}

::LArray<LMdiChild*> &LMdiParent::PrivChildren()
{
	return d->Children;
}

void LMdiParent::OnPaint(LSurface *pDC)
{
	#if MDI_TAB_STYLE
	if (d->Children.Length() == 0)
	#endif
	{
		pDC->Colour(L_LOW);
		pDC->Rectangle();
		return;
	}
	
	#if MDI_TAB_STYLE
	// Draw tabs...
	LFont *Fnt = GetFont();
	
	LColour Back(L_LOW), Black(L_BLACK);
	int Cx = 5;
	pDC->Colour(Back);
	pDC->Rectangle(d->Tabs.x1, d->Tabs.y1, d->Tabs.x1 + Cx - 1, d->Tabs.y2 - 1);
	pDC->Colour(Black);
	pDC->Line(d->Tabs.x1, d->Tabs.y2, d->Tabs.x2, d->Tabs.y2);
	
	int MarginPx = 10;
	
	::LArray<LMdiChild*> Views;
	LMdiChild *Last = dynamic_cast<LMdiChild*>(d->Children.Last());
	GetChildren(Views);
	LColour cActive(L_WORKSPACE);
	LColour cInactive(LColour(L_MED).Mix(cActive));
	
	for (int Idx=0; Idx<Views.Length(); Idx++)
	{
		LMdiChild *c = Views[Idx];
		bool Active = c == Last;
		
		LDisplayString ds(GetFont(), c->Name());
		int BtnWidth = d->Btn ? ds.Y()-4 : 0;
		c->d->Tab.ZOff(ds.X()+BtnWidth+1, d->Tabs.y2-1);
		c->d->Tab.Offset(d->Tabs.x1 + Cx, d->Tabs.y1);
		c->d->Tab.x2 += MarginPx * 2 - 2;
		if (d->Btn)
		{
			c->d->Btn = c->d->Tab;
			c->d->Btn.Size(2, 2);
			c->d->Btn.x1 = c->d->Btn.x2 - BtnWidth + 1;
			c->d->Btn.Offset(-2, 0);
		}
		else c->d->Btn.ZOff(-1, -1);
		
		c->Visible(Active);
		LColour Bk(Active ? cActive : cInactive);
		LColour Edge(L_BLACK);
		LColour Txt(L_TEXT);
		
		LRect r = c->d->Tab;
		if (Active)
			r.y2 += 1;
		int OffX = 0, OffY = 0;
		for (; OffY < r.Y(); OffX++, OffY += 2)
		{
			pDC->Colour(Edge);
			pDC->VLine(r.x1 + OffX, r.y2 - OffY - 1, r.y2 - OffY);
			pDC->Colour(Back);
			pDC->VLine(r.x1 + OffX, r.y1, r.y2 - OffY - 2);
			if (OffY > 0)
			{
				pDC->Colour(Bk);
				pDC->VLine(r.x1 + OffX, r.y2 - OffY + 1, r.y2);
			}
		}
		pDC->Colour(Edge);
		pDC->HLine(r.x1 + OffX - 1, r.x2, r.y1);
		pDC->VLine(r.x2, r.y1, r.y2);
		r.Size(1, 1);
		r.y2 += 1;
		Fnt->Fore(Txt);
		Fnt->Back(Bk);
		Fnt->Transparent(false);
		LRect txt = r;
		txt.x1 += OffX - 1;
		ds.Draw(pDC, r.x1 + MarginPx + 2, r.y1, &txt);
		c->OnPaintButton(pDC, c->d->Btn);
		
		Cx += c->d->Tab.X();
	}
	pDC->Colour(Back);
	pDC->Rectangle(d->Tabs.x1 + Cx, d->Tabs.y1, d->Tabs.x2, d->Tabs.y2 - 1);
	#endif
}

bool LMdiParent::Attach(LViewI *p)
{
	bool s = LLayout::Attach(p);
	if (s)
	{
		GetWindow()->RegisterHook(this, LKeyAndMouseEvents);
		OnPosChange();
	}
	return s;
}

LMdiChild *LMdiParent::IsChild(LViewI *View)
{
	for (LViewI *v=View; v; v=v->GetParent())
	{
		if (v->GetParent() == this)
		{
			LMdiChild *c = dynamic_cast<LMdiChild*>(v);
			if (c)
			{
				return c;
			}
			LAssert(0);
			break;
		}
	}
	
	return 0;
}

bool LMdiParent::OnViewMouse(LView *View, LMouse &m)
{
	if (m.Down())
	{
		LMdiChild *v = IsChild(View);
		LMdiChild *l = IsChild(*Children.rbegin());
		if (v != NULL && v != l)
		{
			v->Raise();
		}
	}
	
	return true;
}

bool LMdiParent::OnViewKey(LView *View, LKey &Key)
{
	if (Key.Down() && Key.Ctrl() && Key.c16 == '\t')
	{
		LMdiChild *Child = IsChild(View);
		#if DEBUG_MDI
		LgiTrace("Child=%p %s view=%s\n", Child, Child ? Child->Name() : 0, View->GetClass());
		#endif
		if (Child)
		{
			::LArray<LMdiChild*> Views;
			GetChildren(Views);
			auto Idx = Views.IndexOf(Child);
			int Inc = Key.Shift() ? -1 : 1;
			auto NewIdx = (Idx + Inc) % Views.Length();
			LMdiChild *NewFront = Views[NewIdx];
			if (NewFront)
			{
				NewFront->Raise();
			}
			return true;
		}
	}
	return false;
}

#if MDI_TAB_STYLE

void LMdiParent::OnMouseClick(LMouse &m)
{
	::LArray<LMdiChild*> Views;
	if (GetChildren(Views))
	{
		for (int i=0; i<Views.Length(); i++)
		{
			LMdiChild *c = Views[i];
			if (c->d->Btn.Overlap(m.x, m.y))
			{
				c->OnButtonClick(m);
				break;
			}
			else if (c->d->Tab.Overlap(m.x, m.y))
			{
				c->OnTitleClick(m);
				break;
			}
		}
	}	
}

void LMdiParent::OnPosChange()
{
	if (!IsAttached())
		return;
	LFont *Fnt = GetFont();
	LRect Client = GetClient();
	LRect Tabs, Content;
		
	d->InOnPosChange = true;
	
	Tabs = Client;
	Tabs.y2 = Tabs.y1 + Fnt->GetHeight() + 1;
	Content = Client;
	Content.y1 = Tabs.y2 + 1;
	
	if (Tabs != d->Tabs ||
		Content != d->Content)
	{
		d->Tabs = Tabs;
		d->Content = Content;
		
		LMdiChild *Last = d->Children.Length() ? d->Children.Last() : NULL;
		#if DEBUG_MDI
		LgiTrace("OnPosChange: Tabs=%s, Content=%s, Children=%i, Last=%s\n",
			d->Tabs.GetStr(), d->Content.GetStr(),
			Children.Length(),
			Last ? Last->Name() : 0);
		#endif
		for (int Idx=0; Idx<d->Children.Length(); Idx++)
		{
			LMdiChild *c = d->Children[Idx];
			if (c == Last)
			{
				#if DEBUG_MDI
				LgiTrace("  [%p/%s], vis=%i, attached=%i\n",
					c, c->Name(), c == Last, c->IsAttached());
				#endif
				
				if (!c->IsAttached())
					c->LLayout::Attach(this);
				c->SetPos(d->Content);
				c->Visible(true);
				c->PourAll();
			}
			else
			{
				#if DEBUG_MDI
				LgiTrace("  [%p/%s], vis=%i, attached=%i\n",
					c, c->Name(), c == Last, c->IsAttached());
				#endif
				#ifdef BEOS
				if (c->IsAttached())
				{
					#if DEBUG_MDI
					LgiTrace("    detaching %s\n", c->GetClass());
					#endif
					c->LLayout::Detach();
					c->SetParent(this);
				}
				#else
				c->Visible(false);
				#endif
			}
		}
		Invalidate();
	}
	d->InOnPosChange = false;
}

#endif

LRect LMdiParent::NewPos()
{
	LRect Status(0, 0, (int)(X()*0.75), (int)(Y()*0.75));
	int Block = 5;
	for (int y=0; y<Y()>>Block; y++)
	{
		for (int x=0; x<X()>>Block; x++)
		{
			bool Has = false;
			for (auto c: Children)
			{
				LRect p = c->GetPos();
				if (p.x1 >> Block == x &&
					p.y1 >> Block == y)
				{
					Has = true;
					break;
				}
			}
			
			if (!Has)
			{
				Status.Offset(x << Block, y << Block);
				return Status;
			}
		}
	}
	
	return Status;
}

bool LMdiParent::Detach()
{
	d->InOnPosChange = true;
	return LLayout::Detach();
}

void LMdiParent::OnChildrenChanged(LViewI *Wnd, bool Attaching)
{
	#if MDI_TAB_STYLE
	if (!d->InOnPosChange /*&& Attaching*/)
	{
		d->Tabs.ZOff(-1, -1);
		OnPosChange();
		Invalidate();
	}
	#else
	for (LViewI *v=Children.First(); v; )
	{
		LMdiChild *c = dynamic_cast<LMdiChild*>(v);
		v = Children.Next();
		if (c)
		{
			c->Invalidate(&c->d->Title);
			
			if (!v)
			{
				LViewI *n = c->Children.First();
				if (n)
				{
					n->Focus(true);
				}
			}
		}
	}
	#endif
}

LViewI *LMdiParent::GetTop()
{
	return d->Children.Length() > 0 ? d->Children.Last() : NULL;
}

#if MDI_TAB_STYLE
int LMdiParent::GetNextOrder()
{
	int m = 0;
	for (int i=0; i<d->Children.Length(); i++)
	{
		m = MAX(m, d->Children[i]->GetOrder());
	}
	return m + 1;
}
#endif