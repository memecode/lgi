/*hdr
**      FILE:           LMenuGtk.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           19/4/2013
**      DESCRIPTION:    Gtk menus
**
**      Copyright (C) 2013, Matthew Allen
**              fret@memecode.com
*/
#include <math.h>

#include "Lgi.h"
#include "GToken.h"
#include "GDisplayString.h"

using namespace Gtk;

///////////////////////////////////////////////////////////////////////////////////////////////
static ::GArray<LSubMenu*> Active;

void SubMenuDestroy(LSubMenu *Item)
{
	// LgiTrace("DestroySub %p %p\n", Item, Item->Info);
	Item->Info = NULL;
}

LSubMenu::LSubMenu(const char *name, bool Popup)
{
	Menu = NULL;
	Parent = NULL;
	Info = NULL;
	_ContextMenuId = NULL;
	ActiveTs = 0;
	InLoop = false;
	
	if (name)
	{
		Info = GtkCast(Gtk::gtk_menu_new(), gtk_menu_shell, GtkMenuShell);
		// LgiTrace("CreateSub %p %p\n", this, Info);
		Gtk::g_signal_connect_data(	Info,
									"destroy",
									(Gtk::GCallback) SubMenuDestroy,
									this,
									NULL,
									Gtk::G_CONNECT_SWAPPED);
	}

	Active.Add(this);
}

LSubMenu::~LSubMenu()
{
	Active.Delete(this);

	while (Items.Length())
	{
		LMenuItem *i = Items[0];
		LgiAssert(i->Parent == this);
		DeleteObj(i);
	}
	
	if (Info)
	{
		#if GTK_MAJOR_VERSION == 2
		LgiAssert(Info->container.widget.object.parent_instance.g_type_instance.g_class);
		#endif
		Gtk::GtkWidget *w = GtkCast(Info.obj, gtk_widget, GtkWidget);
		Gtk::gtk_widget_destroy(w);
		Info = NULL;
	}
}

// This will be run in the GUI thread..
gboolean LSubMenuClick(GMouse *m)
{
	if (!m)
		return false;

	bool OverMenu = false, HasVisible = false;
	uint64 ActiveTs = 0;

	for (auto s: Active)
	{
		auto w = GTK_WIDGET(s->Handle());
		auto vis = gtk_widget_is_visible(w);
		if (vis)
		{
			// auto src = gtk_widget_get_screen(w);
			// auto hnd = gdk_screen_get_root_window(src);
			auto hnd = gtk_widget_get_window(w);

			ActiveTs = MAX(s->ActiveTs, ActiveTs);
			HasVisible = true;

			GdkRectangle a;
			gdk_window_get_frame_extents(hnd, &a);
			/*
			LgiTrace("SubClk down=%i, pos=%i,%i sub=%i,%i-%i,%i\n",
				m->Down(),
				m->x, m->y,
				a.x, a.y, a.width, a.height);
				*/

			GRect rc = a;
			if (rc.Overlap(m->x, m->y))
				OverMenu = true;
		}
	}

	if (m->Down() && !OverMenu && HasVisible && ActiveTs > 0)
	{
		uint64 Now = LgiCurrentTime();
		uint64 Since = Now - ActiveTs;
		LgiTrace("%s:%i - LSubMenuClick, Since=" LPrintfInt64 "\n", _FL, Since);

		if (Since > 500)
		{
			for (auto s: Active)
			{
				auto w = GTK_WIDGET(s->Handle());
				auto vis = gtk_widget_is_visible(w);
				if (vis && s->ActiveTs)
				{
					gtk_widget_hide(w);
					s->ActiveTs = 0;
				}
			}
		}
	}
	// else LgiTrace("LSubMenuClick Down=%i OverMenu=%i HasVIsible=%i ActiveTs=%i\n", m->Down(), OverMenu, HasVisible, ActiveTs > 0);

	DeleteObj(m);

	return false;
}

// This is not called in the GUI thread..
void LSubMenu::SysMouseClick(GMouse &m)
{
	GMouse *ms = new GMouse;
	*ms = m;
	g_idle_add((GSourceFunc) LSubMenuClick, ms);
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

		GtkWidget *item = GTK_WIDGET(i->Handle());
		LgiAssert(item);
		if (item)
		{
			gtk_menu_shell_append(Info, item);
			gtk_widget_show(item);
		}

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

		GtkWidget *item = GTK_WIDGET(i->Handle());
		LgiAssert(item);
		if (item)
		{
			gtk_menu_shell_append(Info, item);
			gtk_widget_show(item);
		}
		
		return i;
	}

	return 0;
}

LSubMenu *LSubMenu::AppendSub(const char *Str, int Where)
{
	GBase::Name(Str);
	LMenuItem *i = new LMenuItem(Menu, this, Str, Where < 0 ? Items.Length() : Where, NULL);
	if (i)
	{
		i->Id(-1);
		Items.Insert(i, Where);

		GtkWidget *item = GTK_WIDGET(i->Handle());
		LgiAssert(item);
		if (item)
		{
			if (Where < 0)
				gtk_menu_shell_append(Info, item);
			else
				gtk_menu_shell_insert(Info, item, Where);
			gtk_widget_show(item);
		}

		i->Child = new LSubMenu(Str);
		if (i->Child)
		{
			i->Child->Parent = i;
			i->Child->Menu = Menu;
			i->Child->Window = Window;
			
			GtkWidget *sub = GTK_WIDGET(i->Child->Handle());
			LgiAssert(sub);
			if (i->Handle() && sub)
			{
				gtk_menu_item_set_submenu(i->Handle(), sub);
				gtk_widget_show(sub);
			}
			else
				LgiTrace("%s:%i Error: gtk_menu_item_set_submenu(%p,%p) failed\n", _FL, i->Handle(), sub);
		}
				
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

bool LSubMenu::IsContext(LMenuItem *Item)
{
	if (!_ContextMenuId)
	{
		LMenuItem *i = GetParent();
		LSubMenu *s = i ? i->GetParent() : NULL;
		if (s)
			// Walk up the chain of menus to find the top...
			return s->IsContext(Item);
		else
			// Ok we got to the top
			return false;
	}
	
	*_ContextMenuId = Item->Id();
	Gtk::gtk_main_quit();
	return true;
}

void GtkDeactivate(Gtk::GtkMenuShell *widget, LSubMenu *Sub)
{
	Sub->OnActivate(false);
}

void LSubMenu::OnActivate(bool a)
{
	if (!a)
	{
		if (_ContextMenuId)
			*_ContextMenuId = 0;
		
		if (InLoop)
		{
			Gtk::gtk_main_quit();
			InLoop = false;
		}
	}
}

int LSubMenu::Float(GView *From, int x, int y, int Button)
{
	#ifdef __GTK_H__
	GWindow *Wnd = From->GetWindow();
	if (!Wnd)
		return -1;
	Wnd->Capture(false);
	#else
	while (From && !From->Handle())
	{
		From = dynamic_cast<GView*>(From->GetParent());
	}		
	
	if (!From || !From->Handle())
	{
		printf("%s:%i - No menu handle\n", _FL);
		return -1;
	}
	
	if (From->IsCapturing())
		From->Capture(false);
	#endif

	ActiveTs = LgiCurrentTime();

	// This signal handles the case where the user cancels the menu by clicking away from it.
	Info.Connect("deactivate",  (GCallback)GtkDeactivate, this);
	
	auto Widget = GTK_WIDGET(Info.obj);
	gtk_widget_show_all(Widget);

	int MenuId = 0;
	_ContextMenuId = &MenuId;

	GdcPt2 Pos(x, y);
	gtk_menu_popup(GTK_MENU(Info.obj),
					NULL, NULL, NULL, NULL,
					Button,
					gtk_get_current_event_time());

	InLoop = true;
	gtk_main();
	_ContextMenuId = NULL;
	return MenuId;
}

LSubMenu *LSubMenu::FindSubMenu(int Id)
{
	for (auto i: Items)
	{
		LSubMenu *Sub = i->Sub();

		// printf("Find(%i) '%s' %i sub=%p\n", Id, i->Name(), i->Id(), Sub);
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
static GAutoString MenuItemParse(const char *s)
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
	
	return GAutoString(NewStr(buf));
}

static void MenuItemActivate(GtkMenuItem *MenuItem, LMenuItem *Item)
{
	Item->OnGtkEvent("activate");
}

static void MenuItemDestroy(GtkWidget *widget, LMenuItem *Item)
{
	Item->OnGtkEvent("destroy");
}

void LMenuItem::OnGtkEvent(::GString Event)
{
	if (Event.Equals("activate"))
	{
		if (!Sub() && !InSetCheck)
		{
			LSubMenu *Parent = GetParent();
		
			if (!Parent || !Parent->IsContext(this))
			{
				auto m = GetMenu();
				if (m)
				{
					// Attached to a menu, so send an event to the window
					GViewI *w = m->WindowHandle();
					if (w)
						w->PostEvent(M_COMMAND, Id());
					else
						LgiAssert(!"No window for menu to send to");
				}
				else
				{
					// Could be just a popup menu... in which case do nothing.				
				}
			}
		}
	}
	else if (Event.Equals("destroy"))
	{
		// LgiTrace("DestroyItem %p %p\n", this, Info);
		Info = NULL;
	}
}

LMenuItem::LMenuItem()
{
	d = NULL;
	Info = NULL;
	Child = NULL;
	Menu = NULL;
	Parent = NULL;
	InSetCheck = false;

	Handle(GTK_MENU_ITEM(gtk_separator_menu_item_new()));
	
	Position = -1;
	
	_Flags = 0;	
	_Icon = -1;
	_Id = 0;
}

LMenuItem::LMenuItem(LMenu *m, LSubMenu *p, const char *txt, int id, int Pos, const char *shortcut)
{
	d = NULL;
	GAutoString Txt = MenuItemParse(txt);
	GBase::Name(txt);
	Info = NULL;

	Handle(GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(Txt)));
	
	Child = NULL;
	Menu = m;
	Parent = p;
	InSetCheck = false;

	Position = Pos;

	_Flags = 0;	
	_Icon = -1;
	_Id = id;

	ShortCut = shortcut;
	ScanForAccel();
}

void LMenuItem::Handle(GtkMenuItem *mi)
{
	LgiAssert(Info == NULL);	
	if (Info != mi)
	{
		Info = mi;
		Gtk::g_signal_connect(Info, "activate", (Gtk::GCallback) MenuItemActivate, this);
		Gtk::g_signal_connect(Info, "destroy", (Gtk::GCallback) MenuItemDestroy, this);
	}
}

LMenuItem::~LMenuItem()
{
	Remove();
	DeleteObj(Child);
}

#if GtkVer(2, 24)
#include <gdk/gdkkeysyms-compat.h>
#endif

#ifndef GDK_F1
enum {
	GDK_F1 = 0xffbe,
	GDK_F2,
	GDK_F3,
	GDK_F4,
	GDK_F5,
	GDK_F6,
	GDK_F7,
	GDK_F8,
	GDK_F9,
	GDK_F10,
	GDK_F11,
	GDK_F12
};
#endif

Gtk::gint LgiKeyToGtkKey(int Key, const char *ShortCut)
{
	#ifdef GDK_a
	LgiAssert(GDK_a == 'a');
	LgiAssert(GDK_A == 'A');
	#endif
	
	/*
	if (Key >= 'a' && Key <= 'z')
		return Key;
	*/
	if (Key >= 'A' && Key <= 'Z')
		return Key;
	if (Key >= '0' && Key <= '9')
		return Key;
	
	switch (Key)
	{
		#ifdef GDK_Delete
		case LK_DELETE: return GDK_Delete;
		#endif
		#ifdef GDK_Insert
		case LK_INSERT: return GDK_Insert;
		#endif
		#ifdef GDK_Home
		case LK_HOME: return GDK_Home;
		#endif
		#ifdef GDK_End
		case LK_END: return GDK_End;
		#endif
		#ifdef GDK_Page_Up
		case LK_PAGEUP: return GDK_Page_Up;
		#endif
		#ifdef GDK_Page_Down
		case LK_PAGEDOWN: return GDK_Page_Down;
		#endif
		#ifdef GDK_BackSpace
		case LK_BACKSPACE: return GDK_BackSpace;
		#endif
		#ifdef GDK_Escape
		case LK_ESCAPE: return GDK_Escape;
		#else
		#warning "GDK_Escape not defined."
		#endif

		case LK_UP:
			return GDK_Up;
		case LK_DOWN:
			return GDK_Down;
		case LK_LEFT:
			return GDK_Left;
		case LK_RIGHT:
			return GDK_Right;

		case LK_F1:
			return GDK_F1;
		case LK_F2:
			return GDK_F2;
		case LK_F3:
			return GDK_F3;
		case LK_F4:
			return GDK_F4;
		case LK_F5:
			return GDK_F5;
		case LK_F6:
			return GDK_F6;
		case LK_F7:
			return GDK_F7;
		case LK_F8:
			return GDK_F8;
		case LK_F9:
			return GDK_F9;
		case LK_F10:
			return GDK_F10;
		case LK_F11:
			return GDK_F11;
		case LK_F12:
			return GDK_F12;
		case ' ':
			#ifdef GDK_space
			return GDK_space;
			#else
			return ' ';
			#endif
		default:
			LgiTrace("Unhandled menu accelerator: 0x%x (%s)\n", Key, ShortCut);
			break;
	}
	
	return 0;
}

bool LMenuItem::ScanForAccel()
{
	if (!Menu)
		return false;

	const char *Sc = ShortCut;
	if (!Sc)
	{
		char *n = GBase::Name();
		if (n)
		{
			char *Tab = strchr(n, '\t');
			if (Tab)
				Sc = Tab + 1;
		}
	}

	if (!Sc)
		return false;

	GToken Keys(Sc, "+-");
	if (Keys.Length() > 0)
	{
		int Flags = 0;
		char16 Key = 0;
		
		for (int i=0; i<Keys.Length(); i++)
		{
			char *k = Keys[i];
			if (stricmp(k, "Ctrl") == 0)
			{
				#ifdef MAC
				Flags |= LGI_EF_SYSTEM;
				#else
				Flags |= LGI_EF_CTRL;
				#endif
			}
			else if (stricmp(k, "Alt") == 0)
			{
				Flags |= LGI_EF_ALT;
			}
			else if (stricmp(k, "Shift") == 0)
			{
				Flags |= LGI_EF_SHIFT;
			}
			else if (stricmp(k, "Del") == 0 ||
					 stricmp(k, "Delete") == 0)
			{
				Key = LK_DELETE;
			}
			else if (stricmp(k, "Ins") == 0 ||
					 stricmp(k, "Insert") == 0)
			{
				Key = LK_INSERT;
			}
			else if (stricmp(k, "Home") == 0)
			{
				Key = LK_HOME;
			}
			else if (stricmp(k, "End") == 0)
			{
				Key = LK_END;
			}
			else if (stricmp(k, "PageUp") == 0)
			{
				Key = LK_PAGEUP;
			}
			else if (stricmp(k, "PageDown") == 0)
			{
				Key = LK_PAGEDOWN;
			}
			else if (stricmp(k, "Backspace") == 0)
			{
				Key = LK_BACKSPACE;
			}
			else if (stricmp(k, "Left") == 0)
			{
				Key = LK_LEFT;
			}
			else if (stricmp(k, "Up") == 0)
			{
				Key = LK_UP;
			}
			else if (stricmp(k, "Right") == 0)
			{
				Key = LK_RIGHT;
			}
			else if (stricmp(k, "Down") == 0)
			{
				Key = LK_DOWN;
			}
			else if (!stricmp(k, "Esc") || !stricmp(k, "Escape"))
			{
				Key = LK_ESCAPE;
			}
			else if (stricmp(k, "Space") == 0)
			{
				Key = ' ';
			}
			else if (k[0] == 'F' && isdigit(k[1]))
			{
				int Idx = atoi(k+1);
				Key = LK_F1 + Idx - 1;
			}
			else if (isalpha(k[0]))
			{
				Key = toupper(k[0]);
			}
			else if (isdigit(k[0]))
			{
				Key = k[0];
			}
			else
			{
				printf("%s:%i - Unknown shortcut part '%s'\n", _FL, k);
			}
		}
		
		if (Key)
		{
			Gtk::gint GtkKey = LgiKeyToGtkKey(Key, Sc);
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
				printf("%s:%i - No gtk key for '%s'\n", _FL, Sc);
			}
			
			auto Ident = Id();
			LgiAssert(Ident > 0);
			Menu->Accel.Insert( new GAccelerator(Flags, Key, Ident) );
		}
		else
		{
			printf("%s:%i - Accel scan failed, str='%s'\n", _FL, Sc);
			return false;
		}
	}

	return true;
}

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

	if (Info)
	{
		#if GTK_MAJOR_VERSION == 2
		LgiAssert(Info->item.bin.container.widget.object.parent_instance.g_type_instance.g_class);
		#endif
		// LgiTrace("Remove %p %p\n", this, Info);
		Gtk::GtkWidget *w = GtkCast(Info.obj, gtk_widget, GtkWidget);
		if (Gtk::gtk_widget_get_parent(w))
		{		
			Gtk::GtkContainer *c = GtkCast(Parent->Info.obj, gtk_container, GtkContainer);
			Gtk::gtk_container_remove(c, w);
		}
		Info = NULL;
	}

	LgiAssert(Parent->Items.HasItem(this));
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

struct MenuItemIndex
{
	Gtk::GtkWidget *w;
	int Current;
	int Index;
	
	MenuItemIndex()
	{
		Index = -1;
		Current = 0;
		w = NULL;
	}
};

static void
FindMenuItemIndex(Gtk::GtkWidget *w, Gtk::gpointer data)
{
	MenuItemIndex *d = (MenuItemIndex*)data;

	printf("w=%p d->w=%p name=%s cur=%i d->Index=%i\n", w, d->w, G_OBJECT_TYPE_NAME(w), d->Current, d->Index);
	if (w == d->w)
		d->Index = d->Current;
	d->Current++;
}

int
gtk_container_get_child_index(GtkContainer *c, GtkWidget *child)
{
	MenuItemIndex Idx;
	if (c && child)
	{
		Idx.w = child;
		gtk_container_foreach(c, FindMenuItemIndex, &Idx);
	}
	return Idx.Index;
}

bool LMenuItem::Replace(Gtk::GtkWidget *newWid)
{
	if (!newWid || !Info)
	{
		LgiTrace("%s:%i - Error: New=%p Old=%p\n", newWid, Info);
		return false;
	}

	// Get widget
	GtkWidget *w = GTK_WIDGET(Info.obj);
	
	// Is is attach already?
	if (gtk_widget_get_parent(w))
	{
		// Yes!
		GtkContainer *c = GTK_CONTAINER(Parent->Info.obj);
		if (c)
		{			
			// Find index
			int PIdx = Parent->Items.IndexOf(this);
			LgiAssert(PIdx >= 0);
		
			// Remove old item
			gtk_container_remove(c, w);
		
			// Add new item
			gtk_menu_shell_insert(Parent->Info, newWid, PIdx);
			gtk_widget_show(newWid);
		}
		else LgiTrace("%s:%i - GTK_CONTAINER failed.\n", _FL);
	}
	else
	{
		// Delete it
		g_object_unref(Info);
	}
	
	Handle(GTK_MENU_ITEM(newWid));
	return Info != NULL;
}

GImageList *LMenuItem::GetImageList()
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

gboolean
LgiMenuItemDraw(GtkWidget *widget, cairo_t *cr, LMenuItem *mi)
{
	auto cls = GTK_WIDGET_GET_CLASS(widget);
	cls->draw(widget, cr);
	mi->PaintIcon(cr);
	return true;
}

void LMenuItem::PaintIcon(Gtk::cairo_t *cr)
{
	if (_Icon < 0)
		return;
	auto il = GetImageList();
	if (!il)
		return;

	auto wid = GTK_WIDGET(Info.obj);
	GtkAllocation a;
	gtk_widget_get_allocation(wid, &a);

	GdkRGBA bk = {0};
	gtk_style_context_get_background_color(	gtk_widget_get_style_context(wid),
											gtk_widget_get_state_flags(wid),
											&bk);

	GScreenDC Dc(cr, a.width, a.height);
	il->Draw(&Dc, 7, 5, _Icon, bk.alpha ? GColour(bk) : GColour::White);
}

void LMenuItem::Icon(int i)
{
	_Icon = i;

	if (Info)
		Info.Connect("draw", (GCallback)LgiMenuItemDraw, this);
}

void LMenuItem::Checked(bool c)
{
	if (c)
		SetFlag(_Flags, ODS_CHECKED);
	else
		ClearFlag(_Flags, ODS_CHECKED);

	if (Info)
	{
		InSetCheck = true;
		
		// Is the item a checked menu item?
		if (!GTK_IS_CHECK_MENU_ITEM(Info.obj) && c)
		{
			// Create a checkable menu item...
			GAutoString Txt = MenuItemParse(Name());
			GtkWidget *w = gtk_check_menu_item_new_with_mnemonic(Txt);

			// Attach our signal
			gulong ret = g_signal_connect_data(	w,
												"activate",
												(GCallback) MenuItemActivate,
												this,
												NULL,
												G_CONNECT_SWAPPED);

			// Replace the existing menu item with this new one
			if (w)
				Replace(w);
			else
				LgiAssert(!"No new widget.");
		}
		
		if (GTK_IS_CHECK_MENU_ITEM(Info.obj))
		{
			// Now mark is checked
			GtkCheckMenuItem *chk = GTK_CHECK_MENU_ITEM(Info.obj);
			if (chk)
			{
				Gtk::gtk_check_menu_item_set_active(chk, c);
			}
		}
		
		InSetCheck = false;
	}
}

bool LMenuItem::Name(const char *n)
{
	bool Status = GBase::Name(n);	
	
	#if GtkVer(2, 16)
	LgiAssert(Info);
	GAutoString Txt = MenuItemParse(n);
	ScanForAccel();
	gtk_menu_item_set_label(Info, Txt);
	#else
	LgiTrace("Warning: can't set label after creation.");
	Status = false;
	#endif
	
	return Status;
}

void LMenuItem::Enabled(bool e)
{
	if (e)
		ClearFlag(_Flags, ODS_DISABLED);
	else
		SetFlag(_Flags, ODS_DISABLED);
	
	if (Info)
	{
		gtk_widget_set_sensitive(GtkCast(Info.obj, gtk_widget, GtkWidget), e);
 	}
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

class LMenuPrivate
{
public:
};

LMenu::LMenu(const char *AppName) : LSubMenu("", false)
{
	Menu = this;
	d = NULL;
	AccelGrp = Gtk::gtk_accel_group_new();
	Info = GtkCast(Gtk::gtk_menu_bar_new(), gtk_menu_shell, GtkMenuShell);
}

LMenu::~LMenu()
{
	Accel.DeleteObjects();
}

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
				// MenuFont.f->CodePage(SysFont->CodePage());
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
			{
				*MenuFont.f = *SysFont;
			}
		}
	}

	return MenuFont.f ? MenuFont.f : SysFont;
}

bool LMenu::Attach(GViewI *p)
{
	if (!p)
	{
		LgiAssert(0);
		return false;
	}
		
	GWindow *Wnd = p->GetWindow();
	if (!Wnd)
	{
		LgiAssert(0);
		return false;
	}
		
	Window = Wnd;
	if (Wnd->_VBox)
	{
		LgiAssert(!"Already has a menu");
		return false;
	}

	
	Gtk::GtkWidget *menubar = GtkCast(Info.obj, gtk_widget, GtkWidget);

	Wnd->_VBox = Gtk::gtk_box_new(Gtk::GTK_ORIENTATION_VERTICAL, 0);

	Gtk::GtkBox *vbox = GtkCast(Wnd->_VBox, gtk_box, GtkBox);
	Gtk::GtkContainer *wndcontainer = GtkCast(Wnd->Wnd, gtk_container, GtkContainer);

	g_object_ref(Wnd->_Root);
	
	gtk_container_remove(wndcontainer, Wnd->_Root);
	gtk_box_pack_start(vbox, menubar, false, false, 0);
	gtk_box_pack_end(vbox, Wnd->_Root, true, true, 0);
	gtk_container_add(wndcontainer, Wnd->_VBox);

	g_object_unref(Wnd->_Root);

	gtk_widget_show_all(GtkCast(Wnd->Wnd, gtk_widget, GtkWidget));
	
	gtk_window_add_accel_group(Wnd->Wnd, AccelGrp);
	
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

bool LMenu::OnKey(GView *v, GKey &k)
{
	if (k.Down())
	{
		for (auto a: Accel)
		{
			if (a->Match(k))
			{
				LgiAssert(a->GetId() > 0);
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
							while (Amp && Amp[1] == '&')
							{
								Amp = strchr(Amp + 2, '&');
							}

							char16 Accel = tolower(Amp[1]);
							char16 Press = tolower(k.c16);
							if (Accel == Press)
							{
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

bool GAccelerator::Match(GKey &k)
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
	
	#if 0
	printf("GAccelerator::Match %i(%c)%s%s%s = %i(%c)%s%s%s%s\n",
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
