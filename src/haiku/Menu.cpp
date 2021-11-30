/*hdr
**      FILE:           Menu.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           29/11/2021
**      DESCRIPTION:    Haiku menus
**
**      Copyright (C) 2021, Matthew Allen
**              fret@memecode.com
*/
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ToolBar.h"

#define DEBUG_MENUS		0

#if DEBUG_MENUS
	#define LOG(...)	printf(__VA_ARGS__)
#else
	#define LOG(...)
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
static ::LArray<LSubMenu*> Active;

LSubMenu::LSubMenu(const char *name, bool Popup)
{
	Menu = NULL;
	Parent = NULL;
	Info = NULL;

	Active.Add(this);
}

LSubMenu::~LSubMenu()
{
	Active.Delete(this);

	while (Items.Length())
	{
		LMenuItem *i = Items[0];
		LAssert(i->Parent == this);
		DeleteObj(i);
	}
}

// This is not called in the GUI thread..
void LSubMenu::SysMouseClick(LMouse &m)
{
	LMouse *ms = new LMouse;
	*ms = m;
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
	LMenuItem *i = new LMenuItem(Menu, this, Str, Id, Where < 0 ? Items.Length() : Where, Shortcut);
	if (i)
	{
		i->Enabled(Enabled);

		Items.Insert(i, Where);

		return i;
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

		return i;
	}

	return 0;
}

LSubMenu *LSubMenu::AppendSub(const char *Str, int Where)
{
	LBase::Name(Str);
	LMenuItem *i = new LMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, NULL);
	if (i)
	{
		i->Id(-1);
		Items.Insert(i, Where);

		return i->Child;
	}

	return 0;
}

void LSubMenu::ClearHandle()
{
	Info = NULL;
	for (auto i: Items)
	{
		i->ClearHandle();
	}
}

void LSubMenu::Empty()
{
	LMenuItem *i;
	while ((i = Items[0]))
	{
		RemoveItem(i);
		DeleteObj(i);
	}
}

bool LSubMenu::RemoveItem(int i)
{
	return RemoveItem(Items.ItemAt(i));
}

bool LSubMenu::RemoveItem(LMenuItem *Item)
{
	if (Item && Items.HasItem(Item))
	{
		return Item->Remove();
	}

	return false;
}

int LSubMenu::Float(LView *From, int x, int y, int Button)
{
	return 0;
}

LSubMenu *LSubMenu::FindSubMenu(int Id)
{
	for (auto i: Items)
	{
		LSubMenu *Sub = i->Sub();

		// LOG("Find(%i) '%s' %i sub=%p\n", Id, i->Name(), i->Id(), Sub);
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
	::LString Shortcut;

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

static LAutoString MenuItemParse(const char *s)
{
	char buf[256], *out = buf;
	const char *in = s;
	while (in && *in && out < buf + sizeof(buf) - 1)
	{
		if (*in == '_')
		{
			*out++ = '_';
			*out++ = '_';
		}
		else if (*in != '&' || in[1] == '&')
		{
			*out++ = *in;
		}
		else
		{
			*out++ = '_';
		}
		
		in++;
	}
	*out++ = 0;
	
	char *tab = strrchr(buf, '\t');
	if (tab)
	{
		*tab++ = 0;
	}
	
	return LAutoString(NewStr(buf));
}

LMenuItem::LMenuItem()
{
	d = new LMenuItemPrivate();
	Info = NULL;
	Child = NULL;
	Menu = NULL;
	Parent = NULL;

	Position = -1;
	
	_Flags = 0;	
	_Icon = -1;
	_Id = 0;
}

LMenuItem::LMenuItem(LMenu *m, LSubMenu *p, const char *txt, int id, int Pos, const char *shortcut)
{
	d = NULL;
	LAutoString Txt = MenuItemParse(txt);
	LBase::Name(txt);
	Info = NULL;

	Child = NULL;
	Menu = m;
	Parent = p;
	Position = Pos;

	_Flags = 0;	
	_Icon = -1;
	_Id = id;

	ScanForAccel();
}

LMenuItem::~LMenuItem()
{
	Remove();
	DeleteObj(Child);
	DeleteObj(d);
}

#if 1

bool LMenuItem::ScanForAccel()
{
	if (!Menu)
		return false;

	return true;
}

#else

bool LMenuItem::ScanForAccel()
{
	::LString Accel;

	if (d->Shortcut)
	{
		Accel = d->Shortcut;
	}
	else
	{
		char *n = LBase::Name();
		if (n)
		{
			char *Tab = strchr(n, '\t');
			if (Tab)
				Accel = Tab + 1;
		}
	}

	if (!Accel)
		return false;

	auto Keys = Accel.SplitDelimit("-+");
	if (Keys.Length() == 0)
		return false;

	int Flags = 0;
	uchar Key = 0;
	bool AccelDirty = false;
	
	for (int i=0; i<Keys.Length(); i++)
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
		else if (k.Equals("System"))
		{
			Flags |= LGI_EF_SYSTEM;
		}
		else if (stricmp(k, "Del") == 0 ||
				 stricmp(k, "Delete") == 0)
		{
			Key = LK_DELETE;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Ins") == 0 ||
				 stricmp(k, "Insert") == 0)
		{
			Key = LK_INSERT;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Home") == 0)
		{
			Key = LK_HOME;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "End") == 0)
		{
			Key = LK_END;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "PageUp") == 0 ||
				 stricmp(k, "Page Up") == 0 ||
				 stricmp(k, "Page-Up") == 0)
		{
			Key = LK_PAGEUP;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "PageDown") == 0 ||
				 stricmp(k, "Page Down") == 0 ||
				 stricmp(k, "Page-Down") == 0)
		{
			Key = LK_PAGEDOWN;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Backspace") == 0)
		{
			Key = LK_BACKSPACE;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Up") == 0)
		{
			Key = LK_UP;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Down") == 0)
		{
			Key = LK_DOWN;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Left") == 0)
		{
			Key = LK_LEFT;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Right") == 0)
		{
			Key = LK_RIGHT;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Esc") == 0)
		{
			Key = LK_ESCAPE;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (stricmp(k, "Space") == 0)
		{
			Key = ' ';
		}
		else if (k[0] == 'F' && IsDigit(k[1]))
		{
			Key = LK_F1 + (int)k.LStrip("F").Int() - 1;
			Flags |= LGI_EF_IS_NOT_CHAR;
		}
		else if (IsAlpha(k[0]))
		{
			Key = toupper(k[0]);
		}
		else if (IsDigit(k[0]))
		{
			Key = k[0];
		}
		else if (strchr(",./\\[]`;\'", k[0]))
		{
			if (Flags & LGI_EF_CTRL)
			{
				switch (k[0])
				{
					case ';': Key = 186; break;
					case '=': Key = 187; break;
					case ',': Key = 188; break;
					case '_': Key = 189; break;
					case '.': Key = 190; break;
					case '/': Key = 191; break;
					case '`': Key = 192; break;
					case '[': Key = 219; break;
					case '\\': Key = 220; break;
					case ']': Key = 221; break;
					case '\'': Key = 222; break;
					default: LAssert(!"Unknown key."); break;
				}
			}
			else
			{
				Key = k[0];
			}
		}
		else
		{
			LAssert(!"Unknown Accel Part");
		}
	}

	if (Key)
	{
		Gtk::gint GtkKey = LgiKeyToGtkKey(Key, Accel);
		if (GtkKey)
		{
			GtkWidget *w = GtkCast(Info.obj, gtk_widget, GtkWidget);
			Gtk::GdkModifierType mod = (Gtk::GdkModifierType)
				(
					(TestFlag(Flags, LGI_EF_CTRL)   ? Gtk::GDK_CONTROL_MASK : 0) |
					(TestFlag(Flags, LGI_EF_SHIFT)  ? Gtk::GDK_SHIFT_MASK : 0)   |
					(TestFlag(Flags, LGI_EF_ALT)    ? Gtk::GDK_MOD1_MASK : 0)    |
					(TestFlag(Flags, LGI_EF_SYSTEM) ? Gtk::GDK_META_MASK : 0)
				);

			const char *Signal = "activate";

			gtk_widget_add_accelerator(	w,
										Signal,
										Menu->AccelGrp,
										GtkKey,
										mod,
										Gtk::GTK_ACCEL_VISIBLE
									);
			gtk_widget_show_all(w);
		}
		else
		{
			LOG("%s:%i - No gtk key for '%s'\n", _FL, Accel.Get());
		}
		
		auto Ident = Id();
		LAssert(Ident > 0);
		Menu->Accel.Insert( new GAccelerator(Flags, Key, Ident) );
		
		return true;
	}
	else
	{
		LOG("%s:%i - Accel scan failed, str='%s'\n", _FL, Accel.Get());
		return false;
	}
}

#endif

LSubMenu *LMenuItem::GetParent()
{
	return Parent;
}

void LMenuItem::ClearHandle()
{
	Info = NULL;
	if (Child)
		Child->ClearHandle();
}

bool LMenuItem::Remove()
{
	if (!Parent)
	{
		return false;
	}

	LAssert(Parent->Items.HasItem(this));
	Parent->Items.Delete(this);
	Parent = NULL;
	return true;
}

void LMenuItem::Id(int i)
{
	_Id = i;
}

void LMenuItem::Separator(bool s)
{
	if (s)
	{
		_Id = -2;
	}
}

LImageList *LMenuItem::GetImageList()
{
	if (GetMenu())
		return GetMenu()->GetImageList();

	// Search tree of parents for an image list...
	for (auto p = GetParent(); p; )
	{
		auto lst = p->GetImageList();
		if (lst)
			return lst;
			
		auto pmi = p->GetParent();
		if (pmi)
			p = pmi->GetParent();
		else
			break;
	}

	return NULL;
}

void LMenuItem::Icon(int i)
{
	_Icon = i;
}

void LMenuItem::Checked(bool c)
{
	if (c)
		SetFlag(_Flags, ODS_CHECKED);
	else
		ClearFlag(_Flags, ODS_CHECKED);
}

bool LMenuItem::Name(const char *n)
{
	bool Status = LBase::Name(n);	
	

	return Status;
}

void LMenuItem::Enabled(bool e)
{
	if (e)
		ClearFlag(_Flags, ODS_DISABLED);
	else
		SetFlag(_Flags, ODS_DISABLED);
	
}

void LMenuItem::Focus(bool f)
{
}

void LMenuItem::Sub(LSubMenu *s)
{
	Child = s;
}

void LMenuItem::Visible(bool i)
{
}

int LMenuItem::Id()
{
	return _Id;
}

const char *LMenuItem::Name()
{
	return LBase::Name();
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
	return !TestFlag(_Flags, ODS_DISABLED);
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
struct LMenuFont
{
	LFont *f;

	LMenuFont()
	{
		f = NULL;
	}

	~LMenuFont()
	{
		DeleteObj(f);
	}
}	MenuFont;

class LMenuPrivate
{
public:
};

LMenu::LMenu(const char *AppName) : LSubMenu("", false)
{
	Menu = this;
	d = NULL;
}

LMenu::~LMenu()
{
	Accel.DeleteObjects();
}

LFont *LMenu::GetFont()
{
	if (!MenuFont.f)
	{
		LFontType Type;
		if (Type.GetSystemFont("Menu"))
		{
			MenuFont.f = Type.Create();
			if (MenuFont.f)
			{
				// MenuFont.f->CodePage(LSysFont->CodePage());
			}
			else
			{
				LOG("LMenu::GetFont Couldn't create menu font.\n");
			}
		}
		else
		{
			LOG("LMenu::GetFont Couldn't get menu typeface.\n");
		}

		if (!MenuFont.f)
		{
			MenuFont.f = new LFont;
			if (MenuFont.f)
			{
				*MenuFont.f = *LSysFont;
			}
		}
	}

	return MenuFont.f ? MenuFont.f : LSysFont;
}

bool LMenu::Attach(LViewI *p)
{
	if (!p)
	{
		LAssert(0);
		return false;
	}
		
	LWindow *Wnd = p->GetWindow();
	if (!Wnd)
	{
		LAssert(0);
		return false;
	}
		
	Window = Wnd;
	
	return true;
}

bool LMenu::Detach()
{
	bool Status = false;
	return Status;
}

bool LMenu::SetPrefAndAboutItems(int a, int b)
{
	return false;
}

bool LMenu::OnKey(LView *v, LKey &k)
{
	if (k.Down())
	{
		for (auto a: Accel)
		{
			if (a->Match(k))
			{
				LAssert(a->GetId() > 0);
				Window->OnCommand(a->GetId(), 0, 0);
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
							if (Amp)
							{
								while (Amp && Amp[1] == '&')
									Amp = strchr(Amp + 2, '&');

								char16 Accel = tolower(Amp[1]);
								char16 Press = tolower(k.c16);
								if (Accel == Press)
									Hide = true;
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

bool GAccelerator::Match(LKey &k)
{
	int Press = (uint) k.vkey;
	
	if (k.vkey == LK_RSHIFT ||
		k.vkey == LK_LSHIFT ||
		k.vkey == LK_RCTRL ||
		k.vkey == LK_LCTRL ||
		k.vkey == LK_RALT ||
		k.vkey == LK_RALT)
	{
		return false;
	}
	
	#if 1
	LOG("GAccelerator::Match %i(%c)%s%s%s = %i(%c)%s%s%s%s\n",
		Press,
		Press>=' '?Press:'.',
		k.Ctrl()?" ctrl":"",
		k.Alt()?" alt":"",
		k.Shift()?" shift":"",
		Key,
		Key>=' '?Key:'.',
		TestFlag(Flags, LGI_EF_CTRL)?" ctrl":"",
		TestFlag(Flags, LGI_EF_ALT)?" alt":"",
		TestFlag(Flags, LGI_EF_SHIFT)?" shift":"",
		TestFlag(Flags, LGI_EF_SYSTEM)?" system":""
		);
	#endif

	if (toupper(Press) == (uint)Key)
	{
		if
		(
			((TestFlag(Flags, LGI_EF_CTRL) ^ k.Ctrl()) == 0) &&
			((TestFlag(Flags, LGI_EF_ALT) ^ k.Alt()) == 0) &&
			((TestFlag(Flags, LGI_EF_SHIFT) ^ k.Shift()) == 0) &&
			((TestFlag(Flags, LGI_EF_SYSTEM) ^ k.System()) == 0)
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
	Flags = GWF_VISIBLE;
	Id = 0;
	ToolButton = 0;
	MenuItem = 0;
	Accelerator = 0;
	TipHelp = 0;
	PrevValue = false;
}

LCommand::~LCommand()
{
	DeleteArray(Accelerator);
	DeleteArray(TipHelp);
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
