/*hdr
 **      FILE:           GuiMenu.cpp
 **      AUTHOR:         Matthew Allen
 **      DATE:           18/7/98
 **      DESCRIPTION:    Gui menu system
 **
 **      Copyright (C) 1998, Matthew Allen
 **              fret@memecode.com
 */

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include "Lgi.h"
#include "GToken.h"
#include "GDisplayString.h"

#define DEBUG_INFO		0

@interface LNSMenuItem : NSMenuItem
{
}
@property LMenuItem* item;
- (id)init:(LMenuItem*)it;
- (void)activate;
@end

@implementation LNSMenuItem
- (id)init:(LMenuItem*)it
{
	if ((self = [super init]) != nil)
	{
		self.item = it;
		self.target = self;
		self.action = @selector(activate);
	}
	
	return self;
}
- (void)activate
{
	self.item->OnActivate(self.item);
}
@end

struct LShortcut
{
	NSString *Str;

public:
	NSString *Key;
	NSEventModifierFlags Mod;
	
	LShortcut(const char *s)
	{
		Key = @"";
		Str = nil;
		Mod = 0;

		auto Keys = GString(s).SplitDelimit("+-");
		if (Keys.Length() <= 0)
			return;
		
		for (auto k: Keys)
		{
			if (stricmp(k, "CtrlCmd") == 0 ||
				stricmp(k, "AltCmd") == 0 ||
				stricmp(k, "Cmd") == 0 ||
				stricmp(k, "Command") == 0)
			{
				Mod |= NSEventModifierFlagCommand;
			}
			else if (stricmp(k, "Ctrl") == 0 ||
					stricmp(k, "Control") == 0)
			{
				Mod |= NSEventModifierFlagControl;
			}
			else if (stricmp(k, "Alt") == 0 ||
					stricmp(k, "Option") == 0)
			{
				Mod |= NSEventModifierFlagOption;
			}
			else if (stricmp(k, "Shift") == 0)
			{
				Mod |= NSEventModifierFlagShift;
			}
			else if (stricmp(k, "Del") == 0 ||
					 stricmp(k, "Delete") == 0)
			{
				unichar s[] = {NSDeleteCharacter};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "Ins") == 0 ||
					 stricmp(k, "Insert") == 0)
			{
				unichar s[] = {NSInsertFunctionKey};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "Home") == 0)
			{
				unichar s[] = {NSHomeFunctionKey};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "End") == 0)
			{
				unichar s[] = {NSEndFunctionKey};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "PageUp") == 0)
			{
				unichar s[] = {NSPageUpFunctionKey};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "PageDown") == 0)
			{
				unichar s[] = {NSPageDownFunctionKey};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "Backspace") == 0)
			{
				unichar s[] = {NSBackspaceCharacter};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (stricmp(k, "Space") == 0)
			{
				Key = @" ";
			}
			else if (k[0] == 'F' && isdigit(k[1]))
			{
				int64 index = k.Strip("F").Int();
				unichar s[] = {(unichar)(NSF1FunctionKey + index - 1)};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else if (isalpha(k[0]))
			{
				Key = Str = GString(k).Lower().NsStr();
			}
			else if (isdigit(k[0]))
			{
				Key = Str = GString(k).NsStr();
			}
			else if (strchr(",.", k(0)))
			{
				unichar s[] = {(unichar)k(0)};
				Key = Str = [[NSString alloc] initWithCharacters:s length:1];
			}
			else
			{
				printf("%s:%i - Unhandled shortcut token '%s'\n", _FL, k.Get());
			}
		}
	}
	
	~LShortcut()
	{
		if (Str) [Str release];
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
LSubMenu::LSubMenu(const char *name, bool Popup)
{
	Menu = 0;
	Parent = 0;
	Info = NULL;
	GBase::Name(name);
	Info.p = [[NSMenu alloc] init];
}

LSubMenu::~LSubMenu()
{
	while (Items.Length())
	{
		LMenuItem *i = Items[0];
		if (i->Parent != this)
		{
			i->Parent = NULL;
			Items.Delete(i);
		}
		delete i;
	}
	
	if (Info)
	{
		[Info.p release];
		Info = NULL;
	}
}

void LSubMenu::OnAttach(bool Attach)
{
	for (auto i: Items)
	{
		i->OnAttach(Attach);
	}
	
	if (Attach &&
		this != Menu &&
		Parent &&
		Parent->Parent)
	{
	}
}

size_t LSubMenu::Length()
{
	return Items.Length();
}

LMenuItem *LSubMenu::ItemAt(int Id)
{
	return Items.ItemAt(Id);
}

LMenuItem *LSubMenu::AppendItem(const char *Str, int Id, bool Enabled, int Where, const char *Shortcut)
{
	LMenuItem *i = new LMenuItem(Menu, this, Str, Id, Where, Shortcut);
	if (i)
	{
		if (Info)
		{
			Items.Insert(i, Where);
			auto Index = Items.IndexOf(i);
			// auto Max = Info.p.numberOfItems;

			GString s(i->GBase::Name());
			auto name = s.NsStr();
			LShortcut sc(Shortcut);

			i->Info.p = [[LNSMenuItem alloc] init:i];
			[i->Info.p setTitle:name];
			i->Info.p.keyEquivalent = sc.Key;
			[Info.p insertItem:i->Info atIndex:Index];
			if (!i->Info)
			{
				Items.Delete(i);
				delete i;
				return NULL;
			}

			i->Info.p.keyEquivalentModifierMask = sc.Mod;
			i->Id(Id);
			i->Enabled(Enabled);
			
			return i;
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", __FILE__, __LINE__);
			DeleteObj(i);
		}
	}
	
	return 0;
}

LMenuItem *LSubMenu::AppendSeparator(int Where)
{
	LMenuItem *i = new LMenuItem;
	if (i)
	{
		i->Parent = this;
		i->Menu = Menu;
		i->Id(-2);
		
		Items.Insert(i, Where);
		
		if (Info)
		{
			auto Index = Items.IndexOf(i);
			// auto Max = Info.p.numberOfItems;
			// printf("Adding ----- @ %i, %i\n", (int)Index, (int)Max);

			i->Info = [NSMenuItem separatorItem];
			[Info.p insertItem:i->Info atIndex:Index];
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", _FL);
		}
		
		return i;
	}
	
	return 0;
}

LSubMenu *LSubMenu::AppendSub(const char *Str, int Where)
{
	LMenuItem *i = new LMenuItem;
	if (i && Str)
	{
		i->Parent = this;
		i->Menu = Menu;
		i->Id(-1);
		
		Items.Insert(i, Where);
		if (Info)
		{
			i->Child = new LSubMenu(Str);
			if (i->Child)
			{
				i->Child->Parent = i;
				i->Child->Menu = Menu;
				i->Child->Window = Window;
				
				i->Info.p = [[NSMenuItem alloc] init];
				LgiAssert(i->Info);

				i->Name(Str);
				GString s(i->GBase::Name());
				[i->Child->Info.p setTitle:s.NsStr()];
				[i->Info.p setSubmenu:i->Child->Info.p];
				
				auto Index = Items.IndexOf(i);
				[Info.p insertItem:i->Info atIndex:Index];
			}
		}
		else
		{
			printf("%s:%i - No menu to attach item to.\n", __FILE__, __LINE__);
		}
		
		return i->Child;
	}
	
	return 0;
}

void LSubMenu::Empty()
{
	while (Items[0])
	{
		if (!RemoveItem(Items[0]))
			break; // Otherwise we'll get an infinite loop.
	}
}

bool LSubMenu::RemoveItem(int i)
{
	LMenuItem *Item = Items[i];
	if (Item)
	{
		return Item->Remove();
	}
	return false;
}

bool LSubMenu::RemoveItem(LMenuItem *Item)
{
	if (Item &&
		Items.HasItem(Item))
	{
		return Item->Remove();
	}
	return false;
}

bool LSubMenu::OnKey(GKey &k)
{
	return false;
}

#if 0
bool IsOverMenu(XEvent *e)
{
	if (e->xany.type == ButtonPress OR
		e->xany.type == ButtonRelease)
	{
		QWidget *q = QWidget::Find(e->xany.window);
		if (q)
		{
			QPopup *m = 0;
			for (; q; q = q->parentWidget())
			{
				if (m = dynamic_cast<QPopup*>(q))
				{
					break;
				}
			}
			
			if (m)
			{
				GRect gr = m->geometry();
				
				return gr.Overlap
				(
				 e->xbutton.x_root,
				 e->xbutton.y_root
					);
			}
		}
	}
	
	return false;
}
#endif

void LSubMenu::OnActivate(LMenuItem *item)
{
	if (!item)
		return;
	
	if (FloatResult)
		*FloatResult = item->Id();
	else if (Parent)
		Parent->OnActivate(item);
	else
		LgiAssert(!"Should have a float result OR a parent..");
}

int LSubMenu::Float(GView *From, int x, int y, int Btns)
{
	GdcPt2 p(x, y);
	OsView v = nil;
	
	auto w = From ? From->GetWindow() : NULL;
	if (w)
	{
		v = w->Handle();

		w->PointToView(p);
		// printf("Menu local = %i,%i\n", p.x, p.y);
		p = w->Flip(p);
		// printf("Menu flip = %i,%i\n", p.x, p.y);
	}
	
	FloatResult.Reset(new int(0));
	
	NSPoint loc = {(double)p.x, (double)p.y};
	[Info.p popUpMenuPositioningItem:nil atLocation:loc inView:v];

	return FloatResult ? *FloatResult : 0;
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
	GString Shortcut;
};

LMenuItem::LMenuItem()
{
	d = new LMenuItemPrivate();
	Menu = NULL;
	Info = NULL;
	Child = NULL;
	Parent = NULL;
	_Icon = -1;
	_Id = 0;
	_Flags = 0;
}

LMenuItem::LMenuItem(LMenu *m, LSubMenu *p, const char *Str, int Id, int Pos, const char *Shortcut)
{
	d = new LMenuItemPrivate();
	GBase::Name(Str);
	Menu = m;
	Parent = p;
	Info = NULL;
	Child = NULL;
	_Icon = -1;
	_Id = Id;
	_Flags = 0;
	d->Shortcut = Shortcut;
	Name(Str);
}

LMenuItem::~LMenuItem()
{
	if (Parent)
	{
		Parent->Items.Delete(this);
		Parent = NULL;
	}
	DeleteObj(Child);
	DeleteObj(d);
}

void LMenuItem::OnActivate(LMenuItem *item)
{
	if (Parent)
		Parent->OnActivate(item);
	else
		LgiAssert(!"Should have a parent.");
}

void LMenuItem::OnAttach(bool Attach)
{
	if (Attach)
	{
		if (_Icon >= 0)
		{
			Icon(_Icon);
		}
		
		if (Sub())
		{
			Sub()->OnAttach(Attach);
		}
	}
}

// the following 3 functions paint the menus according the to
// windows standard. but also allow for correct drawing of menuitem
// icons. some implementations of windows force the program back
// to the 8-bit palette when specifying the icon graphic, thus removing
// control over the colours displayed. these functions remove that
// limitation and also provide the application the ability to override
// the default painting behaviour if desired.
void LMenuItem::_Measure(GdcPt2 &Size)
{
	GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
	bool BaseMenu = Parent == Menu; // true if attached to a windows menu
	// else is a submenu
	int Ht = Font->GetHeight();
	// int IconX = BaseMenu ? ((24-Ht)/2)-Ht : 20;
	int IconX = BaseMenu ? 2 : 16;
	
	if (Separator())
	{
		Size.x = 8;
		Size.y = 8;
	}
	else
	{
		// remove '&' chars for string measurement
		char Str[256];
		char *n = Name(), *i = n, *o = Str;
		
		while (i && *i)
		{
			if (*i == '&')
			{
				if (i[1] == '&')
				{
					*o++ = *i++;
				}
			}
			else
			{
				*o++ = *i;
			}
			
			i++;
		}
		*o++ = 0;
		
		// check for accelerators
		char *Tab = strchr(Str, '\t');
		if (Tab)
		{
			// string with accel
			int Mx, Tx;
			GDisplayString ds(Font, Str, Tab-Str);
			Mx = ds.X();
			GDisplayString ds2(Font, Tab + 1);
			Tx = ds2.X();
			Size.x = IconX + 32 + Mx + Tx;
		}
		else
		{
			// normal string
			GDisplayString ds(Font, Str);
			Size.x = IconX + ds.X() + 4;
		}
		
		if (!BaseMenu)
		{
			// leave room for child pointer
			Size.x += Child ? 8 : 0;
		}
		
		Size.y = MAX(IconX, Ht+2);
	}
}

#define Time(a, b) ((double)(b - a) / 1000)

void LMenuItem::_PaintText(GSurface *pDC, int x, int y, int Width)
{
	char *n = Name();
	if (n)
	{
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		bool Underline = false;
		char *e = 0;
		for (char *s=n; s && *s; s = *e ? e : 0)
		{
			switch (*s)
			{
				case '&':
				{
					if (s[1] == '&')
					{
						e = s + 2;
						GDisplayString d(Font, "&");
						d.Draw(pDC, x, y, 0);
						x += d.X();
					}
					else
					{
						Underline = true;
						e = s + 1;
					}
					break;
				}
				case '\t':
				{
					GDisplayString ds(Font, e + 1);
					x = Width - ds.X() - 8;
					e = s + 1;
					break;
				}
				default:
				{
					if (Underline)
					{
						LgiNextUtf8(e);
					}
					else
					{
						for (e = s; *e; e++)
						{
							if (*e == '\t') break;
							if (*e == '&') break;
						}
					}
					
					ptrdiff_t Len = e - s;
					if (Len > 0)
					{
						// paint text till that point
						GDisplayString d(Font, s, Len);
						d.Draw(pDC, x, y, 0);
						
						if (Underline)
						{
							GDisplayString ds(Font, s, 1);
							int UnderX = ds.X();
							int Ascent = (int)ceil(Font->Ascent());
							pDC->Colour(Font->Fore());
							pDC->Line(x, y+Ascent+1, x+MAX(UnderX-2, 1), y+Ascent+1);
							Underline = false;
						}
						
						x += d.X();
					}
					break;
				}
			}
		}
	}
	
}

void LMenuItem::_Paint(GSurface *pDC, int Flags)
{
	bool BaseMenu = Parent == Menu;
	int IconX = BaseMenu ? 5 : 20;
	bool Selected = TestFlag(Flags, ODS_SELECTED);
	bool Disabled = TestFlag(Flags, ODS_DISABLED);
	bool Checked = TestFlag(Flags, ODS_CHECKED);
	
#if defined(WIN32) || defined(MAC)
	GRect r(0, 0, pDC->X()-1, pDC->Y()-1);
#else
	GRect r = Info->GetClient();
#endif
	
	if (Separator())
	{
		// Paint a separator
		int Cy = r.Y() / 2;
		
		pDC->Colour(L_MED);
		pDC->Rectangle();
		
		pDC->Colour(L_LOW);
		pDC->Line(0, Cy-1, pDC->X()-1, Cy-1);
		
		pDC->Colour(L_LIGHT);
		pDC->Line(0, Cy, pDC->X()-1, Cy);
	}
	else
	{
		// Paint a text menu item
		GColour Fore(L_TEXT);
		GColour Back(Selected ? L_HIGH : L_MED);
		int x = IconX;
		int y = 1;
		
		// For a submenu
		pDC->Colour(Back);
		pDC->Rectangle();
		
		// Draw the text on top
		GFont *Font = Menu && Menu->GetFont() ? Menu->GetFont() : SysFont;
		Font->Transparent(true);
		if (Disabled)
		{
			// Disabled text
			if (!Selected)
			{
				Font->Colour(L_LIGHT);
				_PaintText(pDC, x+1, y+1, r.X());
			}
			// Else selected... don't draw the hilight
			
			// "greyed" text...
			Font->Colour(L_LOW);
			_PaintText(pDC, x, y, r.X());
		}
		else
		{
			// Normal coloured text
			Font->Fore(Fore);
			_PaintText(pDC, x, y, r.X());
		}
		
		GImageList *ImgLst = (Menu && Menu->GetImageList()) ? Menu->GetImageList() : Parent ? Parent->GetImageList() : 0;
		
		// Draw icon/check mark
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
			GColour Bk(L_MED);
			ImgLst->Draw(pDC, 0, 0, _Icon, Bk);
		}
		
		// Sub menu arrow
		if (Child && !dynamic_cast<LMenu*>(Parent))
		{
			pDC->Colour(L_TEXT);
			
			int x = r.x2 - 4;
			int y = r.y1 + (r.Y()/2);
			for (int i=0; i<4; i++)
			{
				pDC->Line(x, y-i, x, y+i);
				x--;
			}
		}
	}
}

bool LMenuItem::ScanForAccel()
{
	if (!d->Shortcut)
		return false;
	
	
	return true;
}

LSubMenu *LMenuItem::GetParent()
{
	return Parent;
}

bool LMenuItem::Remove()
{
	if (!Parent)
		return false;

	if (Parent->Info && Info)
	{
		[Parent->Info.p removeItem:Info];
		Parent->Items.Delete(this);
		Info = NULL;
	}
	else
	{
		Parent->Items.Delete(this);
	}
	
	return true;
}

void LMenuItem::Id(int i)
{
	_Id = i;
	if (Parent && Parent->Info && Info)
	{
		#ifdef COCOA
		#else
		SetMenuItemCommandID(Parent->Info, Info, _Id);
		#endif
	}
}

void LMenuItem::Separator(bool s)
{
	if (s)
	{
		_Id = -2;
	}
	
	if (Parent)
	{
		#ifdef COCOA
		#else
		if (s)
			ChangeMenuItemAttributes(Parent->Info, Info, kMenuItemAttrSeparator, 0);
		else
			ChangeMenuItemAttributes(Parent->Info, Info, 0, kMenuItemAttrSeparator);
		#endif
	}
}

void LMenuItem::Checked(bool c)
{
	if (c)
		SetFlag(_Flags, ODS_CHECKED);
	else
		ClearFlag(_Flags, ODS_CHECKED);
	if (Parent)
	{
		#ifdef COCOA
		#else
		CheckMenuItem(Parent->Info, Info, c);
		#endif
	}
}

bool LMenuItem::Name(const char *n)
{
	char *Tmp = NewStr(n);
	if (Tmp)
	{
		char *in = Tmp, *out = Tmp;
		while (*in)
		{
			if (*in != '&')
				*out++ = *in;
			in++;
		}
		*out++ = 0;
	}
	
	bool Status = GBase::Name(Tmp);
	if (Status && Info)
	{
		GString s(Tmp);
		[Info.p setTitle:s.NsStr()];
	}
	
	DeleteArray(Tmp);
	
	return Status;
}

void LMenuItem::Enabled(bool e)
{
	if (Parent)
	{
		#ifdef COCOA
		#else
		if (e)
		{
			EnableMenuItem(Parent->Info, Info);
		}
		else
		{
			DisableMenuItem(Parent->Info, Info);
		}
		#endif
	}
}

void LMenuItem::Focus(bool f)
{
}

void LMenuItem::Sub(LSubMenu *s)
{
	Child = s;
}

void releaseData(void *info, const void *data, size_t size)
{
	uchar *i = (uchar*)info;
	DeleteArray(i);
}

void LMenuItem::Icon(int i)
{
	_Icon = i;
	
	if (Parent && Parent->Info && Info)
	{
		GImageList *Lst = Menu ? Menu->GetImageList() : Parent->GetImageList();
		if (!Lst)
			return;
		
#if 0
		int Bpp = Lst->GetBits() / 8;
		int Off = _Icon * Lst->TileX() * Bpp;
		int Line = (*Lst)[1] - (*Lst)[0];
		uchar *Base = (*Lst)[0];
		if (!Base)
			return;
		
		int TempSize = Lst->TileX() * Lst->TileY() * Bpp;
		uchar *Temp = new uchar[TempSize];
		if (!Temp)
			return;
		
		uchar *d = Temp;
		for (int y=0; y<Lst->TileY(); y++)
		{
			uchar *s = Base + Off + (Line * (Lst->TileY() - y - 1));
			uchar *e = s + (Lst->TileX() * Bpp);
			while (s < e)
			{
				if (memcmp(Base, s, Bpp) == 0)
					memset(d, 0, Bpp);
				else
					memcpy(d, s, Bpp);
				d += Bpp;
				s += Bpp;
			}
		}
		
		CGDataProviderRef Provider = CGDataProviderCreateWithData(0, Temp, TempSize, releaseData);
		if (Provider)
		{
			// CGColorSpaceRef Cs = CGColorSpaceCreateWithName(kCGColorSpaceUserRGB);
			CGColorSpaceRef Cs = CGColorSpaceCreateDeviceRGB();
			CGImageRef Ico = CGImageCreate(	Lst->TileX(),
										   Lst->TileX(),
										   8,
										   Lst->GetBits(),
										   Lst->TileX() * Bpp,
										   Cs,
										   kCGImageAlphaPremultipliedLast,
										   Provider,
										   NULL,
										   false,
										   kCGRenderingIntentDefault);
			if (Ico)
			{
				OSErr e = SetMenuItemIconHandle(Parent->Info, Info, kMenuCGImageRefType, (char**)Ico);
				if (e) printf("%s:%i - SetMenuItemIconHandle failed with %i\n", __FILE__, __LINE__, e);
			}
			else printf("%s:%i - CGImageCreate failed.\n", __FILE__, __LINE__);
			
			// CGColorSpaceRelease(Cs);
			// CGDataProviderRelease(Provider);
		}
#endif
	}
	else
	{
		printf("Can't set icon.\n");
	}
}

void LMenuItem::Visible(bool i)
{
}

int LMenuItem::Id()
{
	return _Id;
}

char *LMenuItem::Name()
{
	return GBase::Name();
}

bool LMenuItem::Separator()
{
	return _Id == -2;
}

bool LMenuItem::Checked()
{
	return TestFlag(_Flags, ODS_CHECKED);
}

bool LMenuItem::Enabled()
{
	if (Parent)
	{
		#ifdef COCOA
		#else
		return IsMenuItemEnabled(Parent->Info, Info);
		#endif
	}
	
	return true;
}

bool LMenuItem::Visible()
{
	return true;
}

bool LMenuItem::Focus()
{
	return 0;
}

LSubMenu *LMenuItem::Sub()
{
	return Child;
}

int LMenuItem::Icon()
{
	return _Icon;
}

///////////////////////////////////////////////////////////////////////////////////////////////
class LMenuPrivate
{
public:
	int PrefId, AboutId;
	
	LMenuPrivate()
	{
		PrefId = AboutId = 0;
	}
};


LMenu::LMenu(const char *AppName) : LSubMenu("", false)
{
	Menu = this;
	d = new LMenuPrivate;
	
	auto s = AppendSub("Root");
	if (s)
	{
		s->AppendItem("About", M_ABOUT);
		s->AppendSeparator();
		s->AppendItem("Preferences", M_PERFERENCES, true, -1, "Cmd+,");
		s->AppendItem("Hide", M_HIDE, true, -1, "Cmd+H");
		s->AppendSeparator();
		s->AppendItem("Quit", M_QUIT, true, -1, "Cmd+Q");
	}
}

LMenu::~LMenu()
{
	Accel.DeleteObjects();
	DeleteObj(d);
}

void LMenu::OnActivate(LMenuItem *item)
{
	if (!item)
	{
		LgiAssert(0);
		return;
	}
	switch (item->Id())
	{
		case M_ABOUT:
			if (Window && d->AboutId)
				Window->PostEvent(M_COMMAND, d->AboutId);
			break;
		case M_PERFERENCES:
			if (Window && d->PrefId)
				Window->PostEvent(M_COMMAND, d->PrefId);
			break;
		case M_HIDE:
			[[NSApplication sharedApplication] hide:Info];
			break;
		case M_QUIT:
			LgiCloseApp();
			break;
		default:
			if (Window)
				Window->PostEvent(M_COMMAND, item->Id());
			break;
	}
}

bool LMenu::SetPrefAndAboutItems(int PrefId, int AboutId)
{
	d->PrefId = PrefId;
	d->AboutId = AboutId;
	return true;
}

struct LMenuFont
{
	GFont *f;

	LMenuFont()
	{
		f = NULL;
	}

	~LMenuFont()
	{
		DeleteObj(f);
	}
}	MenuFont;

GFont *LMenu::GetFont()
{
	if (!MenuFont.f)
	{
		GFontType Type;
		if (Type.GetSystemFont("Menu"))
		{
			MenuFont.f = Type.Create();
			if (MenuFont.f)
			{
#ifndef MAC
				_Font->CodePage(SysFont->CodePage());
#endif
			}
			else
			{
				printf("LMenu::GetFont Couldn't create menu font.\n");
			}
		}
		else
		{
			printf("LMenu::GetFont Couldn't get menu typeface.\n");
		}
		
		if (!MenuFont.f)
		{
			MenuFont.f = new GFont;
			if (MenuFont.f)
				*MenuFont.f = *SysFont;
		}
	}
	
	return MenuFont.f ? MenuFont.f : SysFont;
}

bool LMenu::Attach(GViewI *p)
{
	bool Status = false;
	
	GWindow *w = dynamic_cast<GWindow*>(p);
	if (w)
	{
		Window = p;
		[NSApplication sharedApplication].mainMenu = Info;
		
		if (Info)
		{
			OnAttach(true);
			Status = true;
		}
		else
		{
			printf("%s:%i - No menu\n", _FL);
		}
	}
	
	return Status;
}

bool LMenu::Detach()
{
	bool Status = false;
	return Status;
}

bool LMenu::OnKey(GView *v, GKey &k)
{
	if (k.Down())
	{
		for (auto a: Accel)
		{
			if (a->Match(k))
			{
				Window->OnCommand(a->GetId(), 0, NULL);
				return true;
			}
		}
		
		if (k.Alt() &&
			!dynamic_cast<LMenuItem*>(v) &&
			!dynamic_cast<LSubMenu*>(v))
		{
			bool Hide = false;
			
			for (auto s: Items)
			{
				if (!s->Separator())
				{
					if (Hide)
					{
						// s->Info->HideSub();
					}
					else
					{
						char *n = s->Name();
						if (ValidStr(n))
						{
							char *Amp = strchr(n, '&');
							while (Amp && Amp[1] == '&')
							{
								Amp = strchr(Amp + 2, '&');
							}
							
							if (Amp)
							{
								char Accel = tolower(Amp[1]);
								char Press = tolower(k.c16);
								if (Accel == Press)
								{
									Hide = true;
								}
							}
						}
						
						if (Hide)
						{
							// s->Info->ShowSub();
						}
						else
						{
							// s->Info->HideSub();
						}
					}
				}
			}
			
			if (Hide)
			{
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////
GAccelerator::GAccelerator(int flags, int key, int id)
{
	Flags = flags;
	Key = key;
	Id = id;
}

bool GAccelerator::Match(GKey &k)
{
	int Press = (uint) k.c16;
	
#if 0
	printf("GAccelerator::Match %i(%c)%s%s%s = %i(%c)%s%s%s\n",
		   Press,
		   Press>=' '?Press:'.',
		   k.Ctrl()?" ctrl":"",
		   k.Alt()?" alt":"",
		   k.Shift()?" shift":"",
		   Key,
		   Key>=' '?Key:'.',
		   TestFlag(Flags, LGI_EF_CTRL)?" ctrl":"",
		   TestFlag(Flags, LGI_EF_ALT)?" alt":"",
		   TestFlag(Flags, LGI_EF_SHIFT)?" shift":""
		   );
#endif
	
	if (toupper(Press) == (uint)Key)
	{
		if
			(
			 ((TestFlag(Flags, LGI_EF_CTRL) ^ k.Ctrl()) == 0)
			 &&
			 ((TestFlag(Flags, LGI_EF_ALT) ^ k.Alt()) == 0)
			 &&
			 ((TestFlag(Flags, LGI_EF_SHIFT) ^ k.Shift()) == 0)
			 )
		{
			return true;
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////
GCommand::GCommand()
{
	Flags = GWF_VISIBLE;
	Id = 0;
	ToolButton = 0;
	MenuItem = 0;
	Accelerator = 0;
	TipHelp = 0;
	PrevValue = false;
}

GCommand::~GCommand()
{
	DeleteArray(Accelerator);
	DeleteArray(TipHelp);
}

bool GCommand::Enabled()
{
	if (ToolButton)
		return ToolButton->Enabled();
	if (MenuItem)
		return MenuItem->Enabled();
	return false;
}

void GCommand::Enabled(bool e)
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

bool GCommand::Value()
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

void GCommand::Value(bool v)
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
