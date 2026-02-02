// \file
// \author Matthew Allen
// \brief Native Win32 Combo Box

#include <stdlib.h>
#include <stdio.h>
#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Combo.h"

#define DEBUG_COMBOBOX	1365

LRect LCombo::Pad(8, 4, 24, 4);

class LComboPrivate
{
public:
	bool SortItems = false;
	LVariantType SubType = GV_NULL;
	LAutoString Buffer;
	LRect Pos;
	
	// Initialization data
	size_t Len = 0;
	bool Init = false;
	int64 Value = 0;
	LString Name;
	LArray<LString> Strs;
};

LCombo::LCombo(int id, LRect *pos) :
	ResObject(Res_ComboBox)
{
	d = new LComboPrivate;
	SetId(id);
	if (pos)
		SetPos(*pos);
	SetTabStop(true);
	SetStyle(GetStyle() | CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL);

	#if 0
	LString Cls = WC_COMBOBOX;
	SetClassW32(Cls);
	#else
	SetClassW32(GetClass());
	if (!SubClass)
		SubClass = LWindowsClass::Create(GetClass());
	if (SubClass)
		SubClass->SubClass("COMBOBOX");
	else
		LAssert(!"No subclass?");
	#endif
}

LCombo::~LCombo()
{
	DeleteObj(d);
}

void LCombo::OnAttach()
{
	/*
	if (!d->Init)
	{
		d->Init = true;
		for (unsigned n=0; n<d->Strs.Length(); n++)
		{
			LAutoWString s(Utf8ToWide(d->Strs[n]));
			SendMessage(Handle(), CB_INSERTSTRING, n, (LPARAM) (s ? s.Get() : L"(NULL)"));
		}

		SetFont(LSysFont);
		
		if (d->Name)
			Name(d->Name);
		else
			Value(d->Value);
	}
	*/
}

bool LCombo::Sort()
{
	return d->SortItems;
}

void LCombo::Sort(bool s)
{
	d->SortItems = s;
}

LVariantType LCombo::SubMenuType()
{
	return d->SubType;
}

void LCombo::SubMenuType(LVariantType Type)
{
	d->SubType = Type;
}

void LCombo::Value(int64 i)
{
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("LCombo::Value(" LPrintfInt64 ") this=%p, hnd=%p strs=%i\n",
			i, this, _View, d->Strs.Length());
	#endif

	d->Value = i;
	
	if (Handle())
	{
		if (d->Strs.Length() == 0 || i < 0)
			i = 0;
		if (i >= (ssize_t)d->Strs.Length())
			i = d->Strs.Length() - 1;

		SendMessage(Handle(), CB_SETCURSEL, i, 0);
	}
}

int64 LCombo::Value()
{
	if (Handle())
	{
		LRESULT r = SendMessage(Handle(), CB_GETCURSEL, 0, 0);
		if (r != CB_ERR)
		{
			d->Value = r;
			if (d->Strs.Length() == 0 || d->Value < 0)
				d->Value = 0;
			else if (d->Value >= (ssize_t)d->Strs.Length())
				d->Value = d->Strs.Length() - 1;
		}
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("LCombo::Value()=" LPrintfInt64 " this=%p, hnd=%p strs=%i\n",
			d->Value, this, _View, d->Strs.Length());
	#endif
	
	return d->Value;
}

bool LCombo::Name(const char *n)
{
	int Idx = IndexOf(n);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("LCombo::Name(%s) this=%p, hnd=%p idx=%i strs=%i\n",
			n, this, _View, Idx, d->Strs.Length());
	#endif

	if (Idx < 0)
	{
		LgiTrace("%s:%i - Can't find '%s' to set combo index.\n", _FL, n);
		return false;
	}

	Value(Idx);
	return true;
}

const char *LCombo::Name()
{
	if (d->Value >= 0 &&
		d->Value < (ssize_t)d->Strs.Length())
	{
		char *s = d->Strs[d->Value];

		#if defined(DEBUG_COMBOBOX)
		if (DEBUG_COMBOBOX==GetId())
			LgiTrace("LCombo::Name()=" LPrintfInt64 "=%s this=%p, hnd=%p strs=%i\n",
				d->Value, s, this, _View, d->Strs.Length());
		#endif

		return s;
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("LCombo::Name() " LPrintfInt64 "=out of range this=%p, hnd=%p strs=%i\n",
			d->Value, this, _View, d->Strs.Length());
	#endif
	
	return NULL;
}

LSubMenu *LCombo::GetMenu()
{
	LAssert(!"Impl me.");
	return 0;
}

void LCombo::SetMenu(LSubMenu *m)
{
	LAssert(!"Impl me.");
}

void LCombo::DoMenu()
{
	LAssert(!"Impl me.");
}

bool LCombo::Delete()
{
	int64 Idx = Value();
	
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete() Idx=" LPrintfInt64 "\n", _FL, this, _View, Idx);
	#endif

	return Delete(Idx);
}

bool LCombo::Delete(size_t i)
{
	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete(%i)\n", _FL, this, _View, i);
	#endif

	if (Handle())
	{
		LRESULT r = SendMessage(Handle(), CB_DELETESTRING, i, 0);
		if (r == CB_ERR)
			return false;
		d->Len = r;
	}			

	if (i >= 0 && i < d->Strs.Length())
	{
		d->Strs.DeleteAt(i, true);
		return true;
	}
	return false;
}

bool LCombo::Delete(const char *p)
{
	int Idx = IndexOf(p);

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("%s:%i %p.%p Delete(%s) Idx=%i\n", _FL, this, _View, p, Idx);
	#endif

	if (Idx < 0)
		return false;

	Value(Idx);
	return true;
}

bool LCombo::Insert(const char *p, int Index)
{
	if (!p)
		return false;

	if (_View && d->Init)
	{
		LAutoWString n(Utf8ToWide(p));
		if (!n)
		{
			LAssert("Utf8ToWide failed.");
			return false;
		}

		if (Index < 0)
			Index = (int)d->Len;

		LRESULT r = SendMessage(Handle(), CB_INSERTSTRING, Index, (LPARAM)n.Get());
		if (r == CB_ERR)
		{
			LAssert("CB_INSERTSTRING failed.");
			return false;
		}
			
		d->Len++;
	}

	#if defined(DEBUG_COMBOBOX)
	if (DEBUG_COMBOBOX==GetId())
		LgiTrace("LCombo::Insert(%s, %i) this=%p, hnd=%p strs=%i\n",
			p, Index, this, _View, d->Strs.Length());
	#endif
	d->Strs.AddAt(Index, p);

	return true;
}

size_t LCombo::Length()
{
	if (_View && d->Init)
		d->Len = SendMessage(_View, CB_GETCOUNT, 0, 0);
	
	return d->Strs.Length();
}

const char *LCombo::operator [](ssize_t i)
{
	if (i >= 0 && i < (ssize_t)d->Strs.Length())
		return d->Strs[i];
	
	LAssert(!"Out of range combo string access.");
	return NULL;
}

int LCombo::IndexOf(const char *str)
{
	if (!ValidStr(str))
		return -1;
	
	for (unsigned i=0; i<d->Strs.Length(); i++)
	{
		if (!_stricmp(str, d->Strs[i]))
			return i;
	}
	
	return -1;
}

bool LCombo::SetPos(LRect &p, bool Repaint)
{
	if (p == Pos)
		return true;
	
	d->Pos = p;
	d->Pos.y2 = d->Pos.y1 + 200;
	bool b = LControl::SetPos(d->Pos, Repaint);
	if (b)
		Pos = p;
	
	return b;
}

int LCombo::SysOnNotify(int Msg, int Code)
{
	// LgiTrace("%s:%i - LCombo::SysOnNotify %i %i\n", _FL, Msg==WM_COMMAND, Code);
	
	if (Msg == WM_COMMAND)
	{
		switch (Code)
		{
			case CBN_SELCHANGE:
			{
				uint64 Old = d->Value;
				if (Value() != Old)
					SendNotify(LNotifyValueChanged);
				break;
			}
			case CBN_DROPDOWN:
			{
				Focus(true);
				break;
			}
		}
	}
	
	return 0;
}

LMessage::Result LCombo::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_CREATE:
		{
			SetFont(LSysFont);
			break;
		}
		case WM_DESTROY:
		{
			Value();
			#if defined(DEBUG_COMBOBOX)
			if (DEBUG_COMBOBOX==GetId())
				LgiTrace("%s:%i %p.%p WM_DESTROY v=" LPrintfInt64 ")\n", _FL, this, _View, d->Value);
			#endif
			break;
		}
		#if 1
		case WM_MOUSEWHEEL:
		{
			// Force the mouse wheel to do something useful
			LRESULT c = SendMessage(_View, CB_GETCURSEL, 0, 0);
			LRESULT items = SendMessage(_View, CB_GETCOUNT, 0, 0);
			int16 delta = (int16) (Msg->A() >> 16);
			int new_cur = (int) (c - (delta / WHEEL_DELTA));
			SendMessage(_View, CB_SETCURSEL, limit(new_cur, 0, items-1), 0); 
			break;
		}
		#endif
	}

	LMessage::Result Status = LControl::OnEvent(Msg);
	
	switch (Msg->Msg())
	{
		case WM_PAINT:
		{
			if (!d->Init)
			{
				d->Init = true;
				for (unsigned n=0; n<d->Strs.Length(); n++)
				{
					LAutoWString s(Utf8ToWide(d->Strs[n]));
					SendMessage(Handle(), CB_INSERTSTRING, n, (LPARAM) (s ? s.Get() : L"(NULL)"));
				}

				if (d->Name)
					Name(d->Name);
				else
					Value(d->Value);

				SendNotify(LNotifyTableLayoutRefresh);
			}
			break;
		}
	}

	return Status;
}

void LCombo::Empty()
{
	if (_View)
		SendMessage(_View, CB_RESETCONTENT, 0, 0);

	d->Strs.Length(0);
	d->Len = 0;
}

bool LCombo::OnKey(LKey &k)
{
	switch (k.vkey)
	{
		case VK_UP:
		case VK_DOWN:
			return true;
	}
	
	return false;
}

bool LCombo::OnLayout(LViewLayoutInfo &Inf)
{
	auto fnt = GetFont();

	if (!Inf.Width.Max)
	{
		// Width calc
		int mx = 0;
		for (auto &s: d->Strs)
		{
			LDisplayString ds(fnt, s);
			mx = MAX(mx, ds.X());
		}
		Inf.Width.Min =
			Inf.Width.Max =
			mx + 36;
	}
	else if (!Inf.Height.Max)
	{
		Inf.Height.Min =
			Inf.Height.Max =
			fnt->GetHeight() + 8;
	}

	return true;
}

