/*hdr
**      FILE:           GuiMenu.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           18/7/98
**      DESCRIPTION:    Gui menu system
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ToolBar.h"
#include "lgi/common/RefCount.h"

///////////////////////////////////////////////////////////////////////////////////////////////
#define LGI_OWNER_DRAW_MENUS

struct LMenuFont : public LRefCount
{
	LAutoPtr<LFont> f;

	LMenuFont()
	{
		LFontType Type;
		if (Type.GetSystemFont("Menu"))
			f.Reset(Type.Create());
	}

}	*MenuFont = NULL;

LColour LGetSysColor(int nIndex)
{
	LColour c;
	auto i = GetSysColor(nIndex);
	c.Rgb(GetRValue(i), GetGValue(i), GetBValue(i));
	return c;
}

struct LMenuPrivate
{
	LColour RootMenuBack;
	bool HasFontRef = false;
	
	LMenuPrivate()
	{
		LArray<int> Ver;
		int Os = LGetOs(&Ver);
		if
		(
			(
				Os == LGI_OS_WIN32
				||
				Os == LGI_OS_WIN64
			)		
			&&
			(
				Ver[0] > 5
				||
				(Ver[0] == 5 && Ver[1] > 0)
			)
		)
		{
			#ifndef SPI_GETFLATMENU
			#define SPI_GETFLATMENU 0x1022
			#endif

			BOOL Flat = true;
			SystemParametersInfo(SPI_GETFLATMENU, 0, &Flat, 0);
			if (Flat)
				RootMenuBack = LGetSysColor(COLOR_MENUBAR);
			else
				RootMenuBack = LGetSysColor(COLOR_MENU);
			// LgiTrace("NT: flat=%i, menu=%x\n", Flat, RootMenuBack);
		}
		else
		{
			RootMenuBack = LGetSysColor(COLOR_MENU);
			// LgiTrace("9x: menu=%x\n", RootMenuBack);
		}
	}

	~LMenuPrivate()
	{
		if (HasFontRef && MenuFont)
		{
			if (MenuFont->DecRef())
				MenuFont = NULL;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
LSubMenu::LSubMenu(const char *name, bool Popup)
{
	if (Popup)
	{
		Info = CreatePopupMenu();
	}
	else
	{
		Info = CreateMenu();
	}

	Menu = 0;
	Parent = 0;
	TrackHandle = 0;
	Window = 0;
}

LSubMenu::~LSubMenu()
{
	if (TrackHandle)
	{
		// At least in Win98, if you call InvalidateRect(...) on a window
		// multiple times just after you use TracePopupMenu(...) on that
		// window the invalid region isn't updated.
		//
		// This behaviour is not by design, it is a bug. However this
		// update forces the window to repaint thus working around the
		// bug in windows.
		UpdateWindow(TrackHandle);
		TrackHandle = 0;
	}

	if (Info)
	{
		DestroyMenu(Info);
		Info = 0;
	}

	if (Parent)
	{
		Parent->Child = 0;
	}
	
	LMenuItem *i;
	while (i = Items[0])
	{
		LAssert(i->Parent == this);
		DeleteObj(i);
	}
}

void LSubMenu::SysMouseClick(LMouse &m)
{
}

LMenuItem *LSubMenu::AppendItem(const char *Str, int Id, bool Enabled, int Where, const char *Shortcut)
{
	int Pos = (int) (Where < 0 ? Items.Length() : min(Where, Items.Length()));

	auto Item = new LMenuItem(Menu, this, Str, Id, Pos, Shortcut);
	if (Item)
	{
		Item->Enabled(Enabled);
		Items.Insert(Item, Where);
		Item->ScanForAccel();
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Item;
}

LMenuItem *LSubMenu::AppendSeparator(int Where)
{
	LMenuItem *Item = new LMenuItem;
	if (Item)
	{
		Item->Menu = Menu;
		Items.Insert(Item, Where);
		Item->Position = (int) Items.IndexOf(Item);
		Item->Separator(true);
		Item->Parent = this;
		Item->Insert(Item->Position);
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Item;
}

LMenuItem *LSubMenu::AppendCustom(LView *View, int Where)
{
	if (!View)
		return nullptr;

	int Pos = (int)(Where < 0 ? Items.Length() : min(Where, Items.Length()));
	auto Item = new LMenuItem(Menu, this, View, Pos);
	Items.Insert(Item, Where);
	if (Menu)
		Menu->OnChange();
	return Item;
}

LSubMenu *LSubMenu::AppendSub(const char *Str, int Where)
{
	int Pos = (int) (Where < 0 ? Items.Length() : min(Where, Items.Length()));
	LMenuItem *Item = new LMenuItem(Menu, this, Str, ItemId_Submenu, Pos);
	LSubMenu *Sub = new LSubMenu;

	if (Item && Sub)
	{
		Items.Insert(Item, Where);
		Item->Sub(Sub);
	}
	else
	{
		DeleteObj(Item);
		DeleteObj(Sub);
	}

	if (Menu)
	{
		Menu->OnChange();
	}

	return Sub;
}

void LSubMenu::Empty()
{
	while (RemoveItem(0));
}

bool LSubMenu::RemoveItem(int i)
{
    LMenuItem *Item = Items[i];
    if (Item && Item->Remove())
    {
	DeleteObj(Item);
        return true;
    }
    return false;
}

bool LSubMenu::RemoveItem(LMenuItem *Item)
{
	if (Item &&
        Items.HasItem(Item) &&
        Item->Remove())
    {
        return true;
	}    
	return false;
}

namespace
{
	class CustomMenuTracking
	{
		static thread_local CustomMenuTracking *Active;
		CustomMenuTracking *Previous = nullptr;
		LSubMenu *Root = nullptr;
		HWND Owner = nullptr;
		HHOOK Hook = nullptr;
		bool OwnHook = false;
		LMenuItem *Hover = nullptr;
		LMenuItem *Pressed = nullptr;
		RECT HoverRect = {};
		RECT PressedRect = {};
		HWND HoverWindow = nullptr;
		HWND PressedWindow = nullptr;
		int Result = 0;

		static bool HasCustom(LSubMenu *Sub)
		{
			for (size_t Index = 0; Index < Sub->Length(); Index++)
			{
				auto Item = Sub->ItemAt((int)Index);
				if (Item->GetCustomView() || (Item->Sub() && HasCustom(Item->Sub())))
					return true;
			}
			return false;
		}

		LMenuItem *HitTest(LSubMenu *Sub, POINT Point, RECT &Bounds)
		{
			for (size_t Index = 0; Index < Sub->Length(); Index++)
			{
				auto Item = Sub->ItemAt((int)Index);
				if (Item->Sub())
					if (auto Found = HitTest(Item->Sub(), Point, Bounds))
						return Found;
			}
			int Index = MenuItemFromPoint(Owner, Sub->Handle(), Point);
			if (Index >= 0)
			{
				auto Item = Sub->ItemAt(Index);
				if (Item && Item->GetCustomView() && Item->Enabled() &&
					GetMenuItemRect(Owner, Sub->Handle(), Index, &Bounds) &&
					PtInRect(&Bounds, Point))
					return Item;
			}
			return nullptr;
		}

		LMenuItem *Selected(LSubMenu *Sub)
		{
			for (size_t Index = 0; Index < Sub->Length(); Index++)
			{
				auto Item = Sub->ItemAt((int)Index);
				if (Item->Sub())
					if (auto Found = Selected(Item->Sub()))
						return Found;
				MENUITEMINFO State = {};
				State.cbSize = sizeof(State);
				State.fMask = MIIM_STATE;
				if (Item->GetCustomView() && Item->Enabled() &&
					GetMenuItemInfo(Sub->Handle(), (UINT)Index, true, &State) &&
					(State.fState & MFS_HILITE) != 0)
					return Item;
			}
			return nullptr;
		}

		LMouse Mouse(LMenuItem *Item, POINT Point, const RECT &Bounds)
		{
			LMouse Event;
			Event.Target = Item->GetCustomView();
			Event.x = Point.x - Bounds.left;
			Event.y = Point.y - Bounds.top;
			Event.Left((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
			Event.Middle((GetKeyState(VK_MBUTTON) & 0x8000) != 0);
			Event.Right((GetKeyState(VK_RBUTTON) & 0x8000) != 0);
			Event.Down(Event.Left() || Event.Middle() || Event.Right());
			Event.Ctrl((GetKeyState(VK_CONTROL) & 0x8000) != 0);
			Event.Shift((GetKeyState(VK_SHIFT) & 0x8000) != 0);
			Event.Alt((GetKeyState(VK_MENU) & 0x8000) != 0);
			return Event;
		}

		void Repaint(HWND Window, const RECT &Bounds)
		{
			wchar_t ClassName[32] = {};
			if (Window && GetClassNameW(Window, ClassName, CountOf(ClassName)) &&
				wcscmp(ClassName, L"#32768") == 0)
			{
				RECT ClientBounds = Bounds;
				MapWindowPoints(nullptr, Window, (POINT*)&ClientBounds, 2);
				RedrawWindow(Window, &ClientBounds, nullptr, RDW_INVALIDATE | RDW_NOERASE);
			}
		}

		void Activate(LMenuItem *Item)
		{
			int Command = Item->GetCustomView()->GetId();
			if (Command > 0 && EndMenu())
				Result = Command;
		}

		void ReleasePressed(POINT Point, UINT Message)
		{
			if (!Pressed)
				return;
			auto Event = Mouse(Pressed, Point, PressedRect);
			Event.Down(false);
			Event.Left(Message == WM_LBUTTONUP);
			Event.Middle(Message == WM_MBUTTONUP);
			Event.Right(Message == WM_RBUTTONUP);
			Pressed->GetCustomView()->OnMouseClick(Event);
			Pressed = nullptr;
			Repaint(PressedWindow, PressedRect);
		}

		bool Filter(MSG *Message)
		{
			if (Message->message == WM_KEYDOWN && Message->wParam == VK_RETURN)
			{
				if (auto Item = Selected(Root))
				{
					Activate(Item);
					return true;
				}
			}
			bool Move = Message->message == WM_MOUSEMOVE;
			bool Down = Message->message == WM_LBUTTONDOWN || Message->message == WM_RBUTTONDOWN ||
				Message->message == WM_MBUTTONDOWN || Message->message == WM_LBUTTONDBLCLK ||
				Message->message == WM_RBUTTONDBLCLK || Message->message == WM_MBUTTONDBLCLK;
			bool Up = Message->message == WM_LBUTTONUP || Message->message == WM_RBUTTONUP ||
				Message->message == WM_MBUTTONUP;
			if (!Move && !Down && !Up)
				return false;

			RECT Bounds = {};
			auto Item = HitTest(Root, Message->pt, Bounds);
			if (Up && Pressed && Pressed != Item)
			{
				ReleasePressed(Message->pt, Message->message);
				return true;
			}
			if (Item != Hover)
			{
				if (Hover)
				{
					auto Event = Mouse(Hover, Message->pt, HoverRect);
					Hover->GetCustomView()->OnMouseExit(Event);
					Repaint(HoverWindow, HoverRect);
				}
				Hover = Item;
				if (Hover)
				{
					auto Event = Mouse(Hover, Message->pt, Bounds);
					Hover->GetCustomView()->OnMouseEnter(Event);
				}
			}
			if (Item)
			{
				HoverRect = Bounds;
				POINT Center = { (Bounds.left + Bounds.right) / 2, (Bounds.top + Bounds.bottom) / 2 };
				HoverWindow = WindowFromPoint(Center);
				auto Event = Mouse(Item, Message->pt, Bounds);
				if (Move)
				{
					Event.Flags |= LGI_EF_MOVE;
					Item->GetCustomView()->OnMouseMove(Event);
				}
				else
				{
					Event.Down(Down);
					Event.Left(Message->message == WM_LBUTTONDOWN || Message->message == WM_LBUTTONUP || Message->message == WM_LBUTTONDBLCLK);
					Event.Middle(Message->message == WM_MBUTTONDOWN || Message->message == WM_MBUTTONUP || Message->message == WM_MBUTTONDBLCLK);
					Event.Right(Message->message == WM_RBUTTONDOWN || Message->message == WM_RBUTTONUP || Message->message == WM_RBUTTONDBLCLK);
					Event.Double(Message->message == WM_LBUTTONDBLCLK || Message->message == WM_RBUTTONDBLCLK || Message->message == WM_MBUTTONDBLCLK);
					if (Down)
					{
						Pressed = Item;
						PressedRect = Bounds;
						PressedWindow = HoverWindow;
					}
					Item->GetCustomView()->OnMouseClick(Event);
					if (Up)
					{
						Pressed = nullptr;
						Activate(Item);
					}
					Repaint(HoverWindow, HoverRect);
				}
				return !Move;
			}
			return false;
		}

		static LRESULT CALLBACK MenuFilter(int Code, WPARAM Param, LPARAM Data)
		{
			if (Code == MSGF_MENU && Active && Active->Filter((MSG*)Data))
				return 1;
			return CallNextHookEx(Active ? Active->Hook : nullptr, Code, Param, Data);
		}

	public:
		CustomMenuTracking(LSubMenu *Sub, HWND Window) : Root(Sub), Owner(Window)
		{
			Previous = Active;
			Active = this;
			if (Previous && Previous->Hook)
				Hook = Previous->Hook;
			else if (HasCustom(Sub))
			{
				Hook = SetWindowsHookExW(WH_MSGFILTER, MenuFilter, nullptr, GetCurrentThreadId());
				OwnHook = Hook != nullptr;
			}
		}

		~CustomMenuTracking()
		{
			Active = Previous;
			if (OwnHook)
				UnhookWindowsHookEx(Hook);
			POINT Point = {};
			GetCursorPos(&Point);
			ReleasePressed(Point, 0);
			if (Hover)
			{
				auto Event = Mouse(Hover, Point, HoverRect);
				Hover->GetCustomView()->OnMouseExit(Event);
			}
		}

		CustomMenuTracking(const CustomMenuTracking &) = delete;
		CustomMenuTracking &operator=(const CustomMenuTracking &) = delete;
		bool Ready() { return Hook || !HasCustom(Root); }
		int Command(int NativeCommand) { return Result > 0 ? Result : NativeCommand; }
	};

	thread_local CustomMenuTracking *CustomMenuTracking::Active = nullptr;
}

int LSubMenu::Float(LView *From, int x, int y, int Button)
{
	int Cmd = 0;

	if (From && Info)
	{
		LViewI *Wnd = From;
		while (Wnd && !Wnd->Handle())
		{
			Wnd = Wnd->GetParent();
		}

		if (Wnd && Wnd->Handle())
		{
			CustomMenuTracking Tracking(this, Wnd->Handle());
			if (!Tracking.Ready())
			{
				LgiTrace("%s:%i - Unable to install custom menu mouse hook.\n", __FILE__, __LINE__);
				return 0;
			}
			Cmd = TrackPopupMenu(	Info,
									TPM_LEFTALIGN |
									TPM_TOPALIGN |
									TPM_RETURNCMD |
									((Button == BtnLeft) ? TPM_LEFTBUTTON : TPM_RIGHTBUTTON),
									x,
									y,
									0,
									Wnd->Handle(),
									NULL);
				Cmd = Tracking.Command(Cmd);
		}
		else
		{
			LgiTrace("%s:%i - No handle to track popup menu.\n", __FILE__, __LINE__);
		}
	}

	return Cmd;
}

size_t LSubMenu::Length()
{
	return Items.Length();
}

LMenuItem *LSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

LSubMenu *LSubMenu::FindSubMenu(int Id)
{
	for (auto i: Items)
	{
		LSubMenu *Sub = i->Sub();

		if (i->Id() == Id)
		{
			return Sub;
		}
		else if (Sub)
		{
			LSubMenu *m = Sub->FindSubMenu(Id);
			if (m)
			{
				return m;
			}
		}
	}

	return 0;
}

LMenuItem *LSubMenu::FindItem(int Id)
{
	for (auto i: Items)
	{
		LSubMenu *Sub = i->Sub();

		if (i->Id() == Id)
		{
			return i;
		}
		else if (Sub)
		{
			i = Sub->FindItem(Id);
			if (i)
			{
				return i;
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class LMenuItemPrivate
{
public:
	bool StartUnderline;		// Underline the first display string
	bool HasAccel;				// The last display string should be right aligned
	List<LDisplayString> Strs;	// Draw each alternate display string with underline
								// except the last in the case of HasAccel==true.
	LString Shortcut;

	LMenuItemPrivate()
	{
		HasAccel = false;
		StartUnderline = false;
	}

	~LMenuItemPrivate()
	{
		Strs.DeleteObjects();
	}

	void UpdateStrings(LFont *Font, char *n)
	{
		// Build up our display strings, 
		Strs.DeleteObjects();
		StartUnderline = false;

		char *Tab = strrchr(n, '\t');
		if (Tab)
		{
			*Tab = 0;
		}

		char *Amp = 0, *a = n;
		while (a = strchr(a, '&'))
		{
			if (a[1] != '&')
			{
				Amp = a;
				break;
			}

			a++;
		}

		if (Amp)
		{
			// Before amp
			Strs.Insert(new LDisplayString(Font, n, Amp - n ));

			// Amp'd letter
			char *e = LSeekUtf8(++Amp, 1);
			Strs.Insert(new LDisplayString(Font, Amp, e - Amp ));

			// After Amp
			if (*e)
			{
				Strs.Insert(new LDisplayString(Font, e));
			}
		}
		else
		{
			Strs.Insert(new LDisplayString(Font, n));
		}

		if (HasAccel = (Tab != 0))
		{
			Strs.Insert(new LDisplayString(Font, Tab + 1));
			*Tab = '\t';
		}
		else if (HasAccel = (Shortcut.Get() != 0))
		{
			Strs.Insert(new LDisplayString(Font, Shortcut));
		}
	}
};

LMenuItem::LMenuItem()
{
	d = new LMenuItemPrivate;
	Menu = NULL;
	Parent = NULL;
	Child = 0;
	Position = -1;
	_Icon = -1;

	ZeroObj(Info);
	Info.cbSize = sizeof(Info);

	#ifdef LGI_OWNER_DRAW_MENUS
	Info.fMask = MIIM_DATA;
	Info.fType = MFT_OWNERDRAW;

	#if _MSC_VER >= _MSC_VER_VS2005
	Info.dwItemData = (ULONG_PTR)this;
	#else
	Info.dwItemData = (DWORD)this;
	#endif

	#endif

	Enabled(true);
}

LMenuItem::LMenuItem(LMenu *m, LSubMenu *p, const char *Txt, int id, int Pos, const char *Shortcut)
{
	d = new LMenuItemPrivate;
	d->Shortcut = Shortcut;
	Position = Pos;
	Parent = NULL;
	Menu = m;
	Parent = NULL;
	Child = 0;
	_Icon = -1;

	ZeroObj(Info);
	Info.cbSize = sizeof(Info);

	#ifdef LGI_OWNER_DRAW_MENUS
	Info.fMask = MIIM_DATA;
	Info.fType = MFT_OWNERDRAW;
	#if _MSC_VER >= _MSC_VER_VS2005
	Info.dwItemData = (ULONG_PTR)this;
	#else
	Info.dwItemData = (DWORD)this;
	#endif
	#endif

	Id(id);
	Enabled(true);
	Name(Txt);

	Parent = p;
	Insert(Position);
}

LMenuItem::LMenuItem(LMenu *m, LSubMenu *p, LView *custom, int Pos) : LMenuItem()
{
	Menu = m;
	Position = Pos;
	CustomView.Reset(custom);
	Info.fMask |= MIIM_FTYPE | MIIM_DATA;
	Info.fType = MFT_OWNERDRAW;
	Info.dwItemData = (ULONG_PTR)this;
	Id(custom->GetId());
	Parent = p;
	Insert(Position);
}

LMenuItem::~LMenuItem()
{
	if (Parent)
	{
		if (Parent->Handle())
		{
			int Index = (int)Parent->Items.IndexOf(this);
			RemoveMenu(Parent->Handle(), Index, MF_BYPOSITION);
		}
		Parent->Items.Delete(this);
	}
	DeleteObj(Child);
	DeleteObj(d);
}

// the following 3 functions paint the menus according the to
// windows standard. but also allow for correct drawing of menuitem
// icons. some implementations of windows force the program back
// to the 8-bit palette when specifying the icon graphic, thus removing
// control over the colours displayed. these functions remove that
// limitation and also provide the application the ability to override
// the default painting behaviour if desired.
void LMenuItem::_Measure(LPoint &Size)
{
	if (CustomView)
	{
		LViewLayoutInfo Inf;
		int Width = CustomView->X();
		int Height = CustomView->Y();
		if (CustomView->OnLayout(Inf))
		{
			int Preferred = Inf.Width.Max > 0 ? Inf.Width.Max : Inf.Width.Min;
			if (Preferred > 0)
				Width = Preferred;
			Inf.Width.Min = Inf.Width.Max = max(Width, 1);
			CustomView->OnLayout(Inf);
			Preferred = Inf.Height.Max > 0 ? Inf.Height.Max : Inf.Height.Min;
			if (Preferred > 0)
				Height = Preferred;
		}
		Size.x = max(Width, 1);
		Size.y = max(Height, 1);
		CustomView->SetPos(LRect(0, 0, Size.x - 1, Size.y - 1));
	}
	else if (Separator())
	{
		Size.x = 8;
		Size.y = 8;
	}
	else
	{
		LFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : LSysFont;
		bool BaseMenu = Parent == Menu; // true if attached to a windows menu
										// else is a submenu
		int Ht = Font->GetHeight();
		Size.x = BaseMenu ? 0 : 20;

		for (auto s: d->Strs)
		{
			Size.x += s->X();
		}

		if (d->Shortcut ||
			strchr(Name(), '\t'))
		{
			Size.x += 32;
		}

		if (!BaseMenu)
		{
			// leave room for child pointer
			Size.x += Child ? 8 : 0;
		}

		Size.y = BaseMenu ? GetSystemMetrics(SM_CYMENU)+1 : Ht+2;
		if (Size.y < 16) Size.y = 16;
	}
}

void LMenuItem::_PaintText(LSurface *pDC, int x, int y, int Width)
{
	bool Underline = d->StartUnderline;
	LFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : LSysFont;
	for (int i=0; i < (d->Strs.Length() - (d->HasAccel ? 1 : 0)); i++)
	{
		LDisplayString *s = d->Strs[i];
		if (!s) break;

		s->Draw(pDC, x, y);
		if (Underline)
		{
			int UnderX = s->X();
			pDC->Colour(Font->Fore());
			pDC->Line(x, y+s->Y()-1, x+max(s->X()-2, 1), y+s->Y()-1);
		}

		Underline = !Underline;
		x += s->X();
	}

	if (d->HasAccel)
	{
		LDisplayString *s = *d->Strs.rbegin();
		if (s)
		{
			s->Draw(pDC, Width - s->X() - 8, y);
		}
	}
}

void LMenuItem::_Paint(LSurface *pDC, int Flags)
{
	if (CustomView)
	{
		LRect Bounds(0, 0, pDC->X() - 1, pDC->Y() - 1);
		if (CustomView->GetPos() != Bounds)
			CustomView->SetPos(Bounds);
		pDC->Colour(L_MENU_BACKGROUND);
		pDC->Rectangle();
		CustomView->_Paint(pDC);
		return;
	}

	bool BaseMenu = Parent == Menu;
	int IconX = BaseMenu ? 5 : 20;
	bool Selected = TestFlag(Flags, ODS_SELECTED);
	bool Disabled = TestFlag(Flags, ODS_DISABLED);
	bool Checked = TestFlag(Flags, ODS_CHECKED);
	LRect r(0, 0, pDC->X()-1, pDC->Y()-1);
	auto Text = Name();

	if (Separator())
	{
		// paint a separator
		int Cy = pDC->Y() / 2;

		pDC->Colour(L_MENU_BACKGROUND);
		pDC->Rectangle();

		pDC->Colour(L_LOW);
		pDC->Line(0, Cy-1, pDC->X()-1, Cy-1);
		
		pDC->Colour(L_LIGHT);
		pDC->Line(0, Cy, pDC->X()-1, Cy);
	}
	else
	{
		// paint a text menu item
		LColour Fore(Selected ? L_FOCUS_SEL_FORE : L_MENU_TEXT);
		LColour Back(BaseMenu ? Menu->d->RootMenuBack : (Selected ? LColour(L_FOCUS_SEL_BACK) : LColour(L_MENU_BACKGROUND)));
		int x = IconX;
		LFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : LSysFont;
		int y = (pDC->Y() - Font->GetHeight()) >> 1;

		// paint the background
		if (BaseMenu)
		{
			// for a menu, sunken on selected
			LRect rgn = r;
			if (Selected)
			{
				LThinBorder(pDC, rgn, DefaultSunkenEdge);
				Fore = LColour(L_MENU_TEXT);
				x++;
				y++;
			}
			
			// always dialog colour
			pDC->Colour(Back);
			pDC->Rectangle(&rgn);
		}
		else
		{
			// for a sub menu
			pDC->Colour(Back);
			pDC->Rectangle();
		}

		// draw the text on top
		Font->Transparent(true);
		if (Disabled)
		{
			// disabled text
			if (!Selected)
			{
				Font->Colour(L_LIGHT);
				_PaintText(pDC, x+1, y, r.X());
			}
			// else selected... don't draw the highlight

			// "grayed" text...
			Font->Colour(L_LOW);
			_PaintText(pDC, x, y-1, r.X()-1);
		}
		else
		{
			// normal coloured text
			Font->Colour(Fore, Back);
			_PaintText(pDC, x, y-1, r.X());
		}

		LImageList *ImgLst = (Menu && Menu->GetImageList()) ? Menu->GetImageList() : Parent ? Parent->GetImageList() : 0;

		// draw icon/check mark
		if (Checked && IconX > 0)
		{
			// it's a check!
			int x = 4;
			int y = 6;
			
			pDC->Colour(Fore);
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
			y++;
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
			y++;
			pDC->Line(x, y, x+2, y+2);
			pDC->Line(x+2, y+2, x+6, y-2);
		}
		else if (ImgLst &&
				_Icon >= 0)
		{
			// it's an icon!
			ImgLst->Draw(pDC, 0, 0, _Icon, Back);
		}
	}
}

bool LMenuItem::ScanForAccel()
{
	LString Raw;

	if (d->Shortcut)
	{
		Raw = d->Shortcut;
	}
	else if (auto n = LBase::Name())
	{
		auto Tab = strchr(n, '\t');
		if (Tab)
		{
			// Deprecated way of storing shortcut in the name:
			Raw = Tab + 1;
			LAssert(!"Fix this");
		}
	}

	auto Shortcuts = Raw.Strip().SplitDelimit("\t");
	for (auto Accel: Shortcuts)
	{
		auto Keys = Accel.SplitDelimit("-+");
		if (Keys.Length() > 0)
		{
			int Flags = 0;
			int Vkey = 0;
			int Chr = 0;
			bool AccelDirty = false;
			
			for (size_t i=0; i<Keys.Length(); i++)
			{
				auto &k = Keys[i];

				if (k.Equals("CtrlCmd"))
				{
					k = LUiEvent::CtrlCmdName();
					AccelDirty = true;
				}
				else if (k.Equals("AltCmd"))
				{
					k = LUiEvent::AltCmdName();
					AccelDirty = true;
				}

				if (k.Equals("Ctrl") || k.Equals("Control"))
				{
					Flags |= LGI_EF_CTRL;
				}
				else if (k.Equals("Alt") || k.Equals("Option"))
				{
					Flags |= LGI_EF_ALT;
				}
				else if (k.Equals("Shift"))
				{
					Flags |= LGI_EF_SHIFT;
				}
				else if (stricmp(k, "Del") == 0 ||
						 stricmp(k, "Delete") == 0)
				{
					Vkey = VK_DELETE;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Ins") == 0 ||
						 stricmp(k, "Insert") == 0)
				{
					Vkey = VK_INSERT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Home") == 0)
				{
					Vkey = VK_HOME;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "End") == 0)
				{
					Vkey = VK_END;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "PageUp") == 0 ||
						 stricmp(k, "Page Up") == 0 ||
						 stricmp(k, "Page-Up") == 0)
				{
					Vkey = VK_PRIOR;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "PageDown") == 0 ||
						 stricmp(k, "Page Down") == 0 ||
						 stricmp(k, "Page-Down") == 0)
				{
					Vkey = VK_NEXT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Backspace") == 0)
				{
					Vkey = VK_BACK;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Up") == 0)
				{
					Vkey = VK_UP;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Down") == 0)
				{
					Vkey = VK_DOWN;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Left") == 0)
				{
					Vkey = VK_LEFT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Right") == 0)
				{
					Vkey = VK_RIGHT;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Esc") == 0)
				{
					Vkey = VK_ESCAPE;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (stricmp(k, "Space") == 0)
				{
					Chr = ' ';
				}
				else if (k[0] == 'F' && IsDigit(k[1]))
				{
					Vkey = VK_F1 + (int)k.LStrip("F").Int() - 1;
					Flags |= LGI_EF_IS_NOT_CHAR;
				}
				else if (IsAlpha(k[0]))
				{
					Chr = toupper(k[0]);
				}
				else if (IsDigit(k[0]))
				{
					Chr = k[0];
				}
				else if (strchr(",./\\[]`;\'", k[0]))
				{
					if (Flags & LGI_EF_CTRL)
					{
						switch (k[0])
						{
							case ';': Chr = 186; break;
							case '=': Chr = 187; break;
							case ',': Chr = 188; break;
							case '_': Chr = 189; break;
							case '.': Chr = 190; break;
							case '/': Chr = 191; break;
							case '`': Chr = 192; break;
							case '[': Chr = 219; break;
							case '\\': Chr = 220; break;
							case ']': Chr = 221; break;
							case '\'': Chr = 222; break;
							default: LAssert(!"Unknown key."); break;
						}
					}
					else
					{
						Chr = k[0];
					}
				}
				else
				{
					LgiTrace("%s:%i - Unknown Accel Part '%s'\n", _FL, k.Get());
				}
			}

			if (AccelDirty)
			{
				d->Shortcut = LString("+").Join(Keys);
				LString n = Name();
				LFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : LSysFont;
				d->UpdateStrings(Font, n);
			}
			
			if ((Vkey || Chr) && Menu)
			{
				Menu->Accel.Insert( new LAccelerator(Flags, Vkey, Chr, Id()) );
			}
		}
	}

	return false;
}

LSubMenu *LMenuItem::GetParent()
{
	return Parent;
}

bool LMenuItem::Remove()
{
	bool Status = false;
	if (Parent)
	{
		int Index = (int) Parent->Items.IndexOf(this);
		Parent->Items.Delete(this);
		Status = RemoveMenu(Parent->Handle(), Index, MF_BYPOSITION) != 0;

		int n=0;
		for (auto i: Parent->Items)
		{
			i->Position = n++;
		}
	}
	
	return Status;
}

bool LMenuItem::Update()
{
	bool Status = FALSE;

	if (Parent && Parent->Handle())
	{
		Status = SetMenuItemInfo(Parent->Handle(), Position, true, &Info) != 0;
		LAssert(Status);
	}

	return Status;
}

void LMenuItem::Icon(int i)
{
	_Icon = i;
}

int LMenuItem::Icon()
{
	return _Icon;
}

void LMenuItem::Id(int i)
{
	Info.wID = i;
	Info.fMask |= MIIM_ID;
	Update();
}

void LMenuItem::Separator(bool s)
{
	if (s)
	{
		Info.fType |= MFT_SEPARATOR;
	}
	else
	{
		Info.fType &= ~MFT_SEPARATOR;
	}

	Info.fMask |= MIIM_TYPE;
	Update();
}

void LMenuItem::Checked(bool c)
{
	if (c)
	{
		Info.fState |= MFS_CHECKED;
	}
	else
	{
		Info.fState &= ~MFS_CHECKED;
	}
	
	Info.fMask |= MIIM_STATE;
	Update();
}

bool LMenuItem::Name(const char *Txt)
{
	if (!LBase::Name(Txt))
		return false;

	LString n = Txt;
	if (!n)
		return false;

	// Set OS menu structure
	Info.dwTypeData = (LPWSTR)LBase::NameW();
	Info.cch = (UINT) StrlenW(LBase::NameW());
	Info.fType |= MFT_STRING;
	Info.fMask |= MIIM_TYPE | MIIM_DATA;

	LFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : LSysFont;
	d->UpdateStrings(Font, n);

	// Tell the OS
	Update();

	return true;
}

void LMenuItem::Enabled(bool e)
{
	Info.fState &= ~(MFS_ENABLED|MFS_DISABLED|MFS_GRAYED);

	if (!e)
	{
		Info.fState |= MFS_DISABLED|MFS_GRAYED;
	}

	Info.fMask |= MIIM_STATE;
	Update();

	// LgiTrace("%s:%i - LMenuItem::Enabled(%i) %s\n", _FL, e, Name());
}

void LMenuItem::Focus(bool f)
{
	if (f)
	{
		Info.fState |= MFS_HILITE;
	}
	else
	{
		Info.fState &= ~MFS_HILITE;
	}

	Info.fMask |= MIIM_STATE;
	Update();
}

void LMenuItem::Sub(LSubMenu *s)
{
	Child = s;
	if (Child)
	{
		Info.hSubMenu = Child->Handle();
		s->Menu = Menu;
		s->Parent = this;
	}
	else
	{
		Info.hSubMenu = 0;
	}
	Info.fMask |= MIIM_SUBMENU;
	Update();
}

bool LMenuItem::Insert(int Pos)
{
	bool Status = false;

	if (Parent && Parent->Handle())
	{
		LAssert(Position >= 0);
		Position = Pos;
		Status = InsertMenuItem(Parent->Handle(),
								Position,
								true,
								&Info) != 0;
		LAssert(Status);
	}

	return Status;
}

void LMenuItem::Visible(bool i)
{
}

int LMenuItem::Id() { return CustomView ? CustomView->GetId() : Info.wID; }
const char *LMenuItem::Name() { return LBase::Name(); }
bool LMenuItem::Separator() { return (Info.fType & MFT_SEPARATOR) != 0; }
bool LMenuItem::Checked() { return (Info.fState & MF_CHECKED) != 0; }
bool LMenuItem::Enabled()
{
	return (Info.fState & MFS_DISABLED) == 0;
}
bool LMenuItem::Visible() { return true; }
bool LMenuItem::Focus() { return (Info.fState & MFS_HILITE) != 0; }
LSubMenu *LMenuItem::Sub() { return Child; }

///////////////////////////////////////////////////////////////////////////////////////////////
LMenu::LMenu(const char *AppName) : LSubMenu("", false)
{
	d = new LMenuPrivate;
	Menu = this;
	Window = NULL;
}

LMenu::~LMenu()
{
	Accel.DeleteObjects();
	DeleteObj(d);
}

void LMenu::OnChange()
{
	if (Info && Window)
	{
		if (::GetMenu(Window->Handle()) != Info)
		{
			SetMenu(Window->Handle(), Info);
		}
	}
}

LFont *LMenu::GetFont()
{
	if (!d->HasFontRef)
	{
		if (!MenuFont)
			MenuFont = new LMenuFont;
		if (!MenuFont)
			return LSysFont;

		MenuFont->IncRef();
		LMenu::d->HasFontRef = true;
	}

	return MenuFont && MenuFont->f ? MenuFont->f : LSysFont;
}

bool LMenu::Attach(LViewI *p)
{
	Window = p;

	return true;
}

bool LMenu::Detach()
{
	bool Status = FALSE;

	if (Window)
	{
		HMENU hWndMenu = ::GetMenu(Window->Handle());
		if (hWndMenu == Info)
		{
			Status = SetMenu(Window->Handle(), NULL) != 0;
			if (Status)
			{
				Window = NULL;
			}
		}
	}

	return Status;
}

bool LMenu::OnKey(LView *v, LKey &k)
{
	if (k.Down() && k.vkey != 17)
	{
		for (auto a: Accel)
		{
			if (a->Match(k))
			{
				LMenuItem *i = FindItem(a->GetId());
				if (!i || i->Enabled())
				{
					int Cmd = a->GetId();
					Window->OnCommand(Cmd, 0, 0);
					return true;
				}
			}
		}
	}

	return false;
}

int LMenu::_OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_MENUCHAR:
		{
			short Key = Msg->A() & 0xffff;
			HMENU View = (HMENU)Msg->B();
			MENUITEMINFO Info;

			ZeroObj(Info);
			Info.cbSize = sizeof(Info);
			Info.fMask = MIIM_DATA;
			if (GetMenuItemInfo(View, 0, true, &Info))
			{
				LMenuItem *Item = (LMenuItem*)Info.dwItemData;
				if (Item)
				{
					LSubMenu *Menu = Item->Parent;
					if (Menu)
					{
						int Index=0;
						for (auto i: Menu->Items)
						{
							auto n = i->Name();
							if (n)
							{
								char *Amp = strchr(n, '&');
								if (Amp &&
									toupper(Amp[1]) == toupper(Key))
								{
									return (MNC_EXECUTE << 16) | Index;
								}
							}
							Index++;
						}
					}
				}
			}			
			break;
		}
		case WM_MEASUREITEM:
		{
			LPMEASUREITEMSTRUCT Item = (LPMEASUREITEMSTRUCT)Msg->b;
			if (Item && Item->CtlType == ODT_MENU && Item->itemData)
			{
				LPoint Size;

				((LMenuItem*)Item->itemData)->_Measure(Size);
				Item->itemWidth = Size.x;
				Item->itemHeight = Size.y;
				return true;
			}
			break;
		}
		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT Item = (LPDRAWITEMSTRUCT)Msg->b;
			if (Item &&
				Item->CtlType == ODT_MENU && Item->itemData)
			{
				LRect r = Item->rcItem;
				int SavedDc = SaveDC(Item->hDC);
				if (!SavedDc)
					return false;
				IntersectClipRect(Item->hDC, Item->rcItem.left, Item->rcItem.top,
					Item->rcItem.right, Item->rcItem.bottom);

				{
					LScreenDC Dc(Item->hDC, Item->hwndItem);
					
					// Get the original origin. We need to do this because when
					// "animated menus" are on the offset starts at -3,-3 and throws
					// the menu items off. This is a bug in windows, but watcha
					// gonna do?
					auto origin = Dc.GetOrigin();
					
					// Clip and offset so that the menu item draws in client co-ords.
					Dc.SetOrigin(LPoint(origin.x-r.x1, origin.y-r.y1));
					Dc.SetSize(r.X(), r.Y());

					// Paint the item...
					((LMenuItem*)Item->itemData)->_Paint(&Dc, Item->itemState);

					// Set the origin back the original value.
					Dc.SetOrigin(origin);
				}
				RestoreDC(Item->hDC, SavedDc);
				return true;
			}
			break;
		}
	}

	return 0;
}

bool LMenu::SetPrefAndAboutItems(int PrefId, int AboutId)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////
LAccelerator::LAccelerator(int flags, int vkey, int chr, int id)
{
	Flags = flags;
	Vkey = vkey;
	Chr = chr;
	Id = id;
}

bool LAccelerator::Match(LKey &k)
{
	if
	(
		!k.IsChar
		&&
		(
			(Chr != 0 && tolower(k.c16) == tolower(Chr))
			||
			(Vkey != 0 && k.vkey == Vkey)
		)
	)
	{
		if
		(
			(Ctrl() ^ k.Ctrl()) == 0 &&
			(Alt() ^ k.Alt()) == 0 &&
			(Shift() ^ k.Shift()) == 0 &&
			(!TestFlag(Flags, LGI_EF_IS_CHAR) || k.IsChar) &&
			(!TestFlag(Flags, LGI_EF_IS_NOT_CHAR) || !k.IsChar)
		)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////
LCommand::LCommand()
{
}

LCommand::~LCommand()
{
}

bool LCommand::Enabled()
{
	if (ToolButton)
		return ToolButton->Enabled();
	if (MenuItem)
		return MenuItem->Enabled();
	return false;
}

void LCommand::Enabled(bool e)
{
	if (ToolButton)
	{
		ToolButton->Enabled(e);
	}
	if (MenuItem)
	{
		MenuItem->Enabled(e);
	}
}

bool LCommand::Value()
{
	bool HasChanged = false;

	if (ToolButton)
	{
		HasChanged |= (ToolButton->Value() != 0) ^ PrevValue;
	}
	if (MenuItem)
	{
		HasChanged |= (MenuItem->Checked() != 0) ^ PrevValue;
	}

	if (HasChanged)
	{
		Value(!PrevValue);
	}

	return PrevValue;
}

void LCommand::Value(bool v)
{
	if (ToolButton)
	{
		ToolButton->Value(v);
	}
	if (MenuItem)
	{
		MenuItem->Checked(v);
	}
	PrevValue = v;
}
