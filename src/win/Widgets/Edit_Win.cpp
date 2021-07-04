// \file
// \author Matthew Allen (fret@memecode.com)
// \brief Native Win32 Edit Control

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/SkinEngine.h"
#include "lgi/common/Edit.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Widgets.h"

GAutoWString LgiAddReturns(const char16 *n)
{
	GAutoWString w;
	
	if (n && StrchrW(n, '\n'))
	{
		int Len = 0;
		const char16 *c;
		for (c = n; *c; c++)
		{
			if (*c == '\n')
				Len += 2;
			else if (*c != '\r')
				Len++;
		}
		if (w.Reset(new char16[Len+1]))
		{
			char16 *o = w;
			for (c = n; *c; c++)
			{
				if (*c == '\n')
				{
					*o++ = '\r';
					*o++ = '\n';
				}
				else if (*c != '\r')
				{
					*o++ = *c;
				}
			}
			*o = 0;
			LgiAssert(o - w == Len);
		}
	}
	
	return w;
}
///////////////////////////////////////////////////////////////////////////////////////////
class LEditPrivate
{
public:
	bool IgnoreNotify;
	bool NotificationProcessed;
	bool InEmptyMode;
	LCss::ColorDef NonEmptyColor;
	GAutoWString EmptyText;

	LEditPrivate()
	{
		IgnoreNotify = true;
		NotificationProcessed = false;
		InEmptyMode = false;
	}

	~LEditPrivate()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
LEdit::LEdit(int id, int x, int y, int cx, int cy, const char *name) :
	LControl(LGI_EDITBOX),
	ResObject(Res_EditBox)
{
	d = new LEditPrivate;
	LPoint Size = SizeOfStr((name)?name:(char*)"A");
	if (cx < 0) cx = Size.x + 6;
	if (cy < 0) cy = Size.y + 4;
	WndFlags |= GWF_SYS_BORDER;

	SetId(id);
	Sunken(true);
	if (name) Name(name);
	LRect r(x, y, x+max(cx, 10), y+max(cy, 10));
	SetPos(r);
	SetStyle(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP);
	SetFont(SysFont);

	if (SubClass)
		SubClass->SubClass("EDIT");
}

LEdit::~LEdit()
{
	DeleteObj(d);
}

void LEdit::Select(int Start, int Len)
{
	if (_View)
	{
		SendMessage(_View, EM_SETSEL, Start, Start+Len-1);
	}
}

GRange LEdit::GetSelectionRange()
{
	GRange r;
	if (!_View)
		return r;

	DWORD s = 0, e = 0;
	SendMessage(_View, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
	if (s == e)
		return r;

	r.Start = s;
	r.Len = e - s;
	return r;
}

bool LEdit::MultiLine()
{
	return TestFlag(GetStyle(), ES_MULTILINE);
}

void LEdit::MultiLine(bool m)
{
	int Flags = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN;
	if (m)
		SetStyle(GetStyle() | Flags);
	else
		SetStyle(GetStyle() & ~Flags);
}

bool LEdit::Password()
{
	return TestFlag(GetStyle(), ES_PASSWORD);
}

void LEdit::Password(bool m)
{
	if (m)
		SetStyle(GetStyle() | ES_PASSWORD);
	else
		SetStyle(GetStyle() & ~ES_PASSWORD);
}

int LEdit::SysOnNotify(int Msg, int Code)
{
	if (Msg == WM_COMMAND &&
		Code == EN_CHANGE &&
		!d->IgnoreNotify &&
		_View)
	{
		if (!d->InEmptyMode)
		{
			GAutoWString w = SysName();
			LBase::NameW(w);
		}
		
		// LgiTrace("LEdit::SysOnNotify EN_CHANGE inempty=%i, n16=%S\n", d->InEmptyMode, LBase::NameW());
		SendNotify(0);
	}

	return 0;
}

void LEdit::Value(int64 i)
{
	char Str[32];
	sprintf_s(Str, sizeof(Str), LPrintfInt64, i);
	Name(Str);
}

int64 LEdit::Value()
{
	auto n = Name();
	return (n) ? atoi64(n) : 0;
}

#define EDIT_PROCESSING 1

GMessage::Result LEdit::OnEvent(GMessage *Msg)
{
	#if EDIT_PROCESSING
	switch (Msg->m)
	{
		case WM_DESTROY:
		{
			Name();
			_View = 0;
			break;
		}
		case WM_SETTEXT:
		{
			if (d->InEmptyMode && SubClass)
			{
				// LgiTrace("LEdit WM_SETTEXT - calling parent, inempty=%i\n", d->InEmptyMode);
				return SubClass->CallParent(Handle(), Msg->m, Msg->a, Msg->b);
			}
			else
			{
				// LgiTrace("LEdit WM_SETTEXT - dropping through to LControl, inempty=%i.\n", d->InEmptyMode);
			}
			break;
		}
		case WM_GETDLGCODE:
		{
			int Code = 0;

			Code = DLGC_WANTALLKEYS;
			return Code;
		}
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		{
			if (!MultiLine() &&
				(Msg->a == VK_UP || Msg->a == VK_DOWN))
			{
				LView::OnEvent(Msg);
				return 1;
			}

			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_KEYDOWN:
		{
			if (!MultiLine() &&
				(
					Msg->a == VK_UP ||
					Msg->a == VK_DOWN ||
					Msg->a == VK_ESCAPE ||
					Msg->a == VK_BACK ||
					Msg->a == VK_RETURN
				))
			{
				LView::OnEvent(Msg);
				return 1;				
			}

			// if (Msg->a == VK_ESCAPE) SendNotify(GNotify_EscapeKey);


			if (Msg->a == VK_TAB && MultiLine())
			{
				// this causes the multi-line Edit control to
				// insert tabs into the text and not move
				// to the next field.
				return 0;
			}
			
			if (tolower((int)Msg->a) == 'u')
			{
				// Ctrl+U case change
				if (GetKeyState(VK_CONTROL) & 0xFF00)
				{
					DWORD Start = 0, End = 0;
					SendMessage(_View, EM_GETSEL, (WPARAM) &Start, (LPARAM) &End);
					int Chars = End - Start;

					char16 *n = (char16*)NameW();
					if (n)
					{
						if (GetKeyState(VK_SHIFT) & 0xFF00)
						{
							// upper case selection
							for (DWORD i=Start; i<Start+End; i++)
							{
								n[i] = toupper(n[i]);
							}
						}
						else
						{
							// lower case selection
							for (DWORD i=Start; i<Start+End; i++)
							{
								n[i] = tolower(n[i]);
							}
						}

						NameW(n);
						SendMessage(_View, EM_SETSEL, Start, End);
					}
				}
			}

			if (SysOnKey(this, Msg))
			{
				return 0;
			}
			break;
		}
		case WM_CHAR:
		{
			if ((Msg->a == VK_TAB || Msg->a == VK_RETURN) &&
				!MultiLine())
			{
				// single line edit controls make "click"
				// when they get a TAB char... this
				// avoids that.
				return 0;
			}
			break;
		}
	}
	#endif

	auto Status = LControl::OnEvent(Msg);

	#if EDIT_PROCESSING
	switch (Msg->Msg())
	{
		case WM_CREATE:
		{
			d->IgnoreNotify = false;
			LResources::StyleElement(this);
			break;
		}
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
		case WM_MOUSEMOVE:
		{
			LView::OnEvent(Msg);
			break;
		}
	}
	#endif

	return Status;
}

void LEdit::OnCreate()
{
	if (d->EmptyText)
		SysEmptyText();
}

void LEdit::OnFocus(bool f)
{
	if (d->EmptyText)
		SysEmptyText();
}

void LEdit::KeyProcessed()
{
	d->NotificationProcessed = true;
}

bool LEdit::OnKey(LKey &k)
{
	if (k.Down())
		d->NotificationProcessed = false;
	
	switch (k.vkey)
	{
	 	case LK_RETURN:
		{
			if (k.Down())
				SendNotify(GNotify_ReturnKey);
			else if (d->NotificationProcessed)
			{
				d->NotificationProcessed = false;
				return true;
			}
			if (MultiLine())
				return true;
			break;
		}
		case LK_ESCAPE:
		{
			if (k.Down())
				SendNotify(GNotify_EscapeKey);
			else if (d->NotificationProcessed)
			{
				d->NotificationProcessed = false;
				return true;
			}
			break;
		}
		case LK_BACKSPACE:
		{
			if (k.Down())
				SendNotify(GNotify_BackspaceKey);
			else if (d->NotificationProcessed)
			{
				d->NotificationProcessed = false;
				return true;
			}
			break;
		}
		case LK_DELETE:
		{
			if (k.Down())
				SendNotify(GNotify_DeleteKey);
			else if (d->NotificationProcessed)
			{
				d->NotificationProcessed = false;
				return true;
			}
			break;
		}
	}

	if
	(
		!k.IsChar &&
		k.Ctrl() &&
		(
			tolower(k.c16) == 'x' ||
			tolower(k.c16) == 'c' ||
			tolower(k.c16) == 'v'
		)
	)
	{
		// This makes sure any menu items labeled Ctrl+x, Ctrl+c and 
		// Ctrl+v don't fire as WELL as the built in windows clipboard
		// handlers.
		return true;
	}

	if
	(
		!MultiLine() &&
		(
			k.vkey == LK_TAB ||
			k.vkey == LK_RETURN  ||
			k.vkey == LK_ESCAPE
		)
	)
	{	
		return d->NotificationProcessed;
	}

	return false;
}

ssize_t LEdit::GetCaret(bool Cursor)
{
	DWORD Start, End = 0;
	if (_View)
        SendMessage(_View, EM_GETSEL, (GMessage::Param)&Start, (GMessage::Param)&End);
	return End;
}

void LEdit::SetCaret(size_t Pos, bool Select, bool ForceFullUpdate)
{
	if (_View)
		SendMessage(_View, EM_SETSEL, Pos, Pos);
}

/////////////////////////////////////////////////////////////////////////////////////////
GAutoWString LEdit::SysName()
{
	GAutoWString a;
	if (_View)
	{
		int Length = GetWindowTextLengthW(_View);
		if (Length)
		{
			if (a.Reset(new char16[Length+1]))
			{
				a[0] = 0;
				int Chars = GetWindowTextW(_View, a, Length+1);
				if (Chars >= 0)
				{
					// Remove return characters
					char16 *i = a;
					char16 *end = a + Chars;
					char16 *o = a;
					while (i < end)
					{
						if (*i != '\r')
							*o++ = *i;
						i++;
					}
					*o = 0;
				}
				else a.Reset();
			}
		}
	}
	return a;
}

bool LEdit::SysName(const char16 *n)
{
	if (!_View)
		return false;

	GAutoWString w = LgiAddReturns(n);
	if (w)
		n = w;

	return SetWindowTextW(_View, n ? n : L"") != FALSE;
}

const char *LEdit::Name()
{
	if (Handle() && !d->InEmptyMode)
	{
		GAutoWString w = SysName();
		LBase::NameW(w);
	}
	
	return LBase::Name();
}

bool LEdit::Name(const char *n)
{
	bool Old = d->IgnoreNotify;
	d->IgnoreNotify = true;

	LBase::Name(n);
	GAutoWString w = LgiAddReturns(LBase::NameW());
	if (w)
		LBase::NameW(w);
	bool Status = SysEmptyText();

	d->IgnoreNotify = Old;
	return Status;
}

const char16 *LEdit::NameW()
{
	if (Handle() && !d->InEmptyMode)
	{
		GAutoWString w = SysName();
		LBase::NameW(w);
	}

	return LBase::NameW();
}

bool LEdit::NameW(const char16 *s)
{
	bool Old = d->IgnoreNotify;
	d->IgnoreNotify = true;

	GAutoWString w = LgiAddReturns(s);
	LBase::NameW(w ? w : s);
	bool Status = SysEmptyText();

	d->IgnoreNotify = Old;
	return Status;
}

bool LEdit::SysEmptyText()
{
	bool Status = false;
	bool HasFocus = Focus();
	bool Empty = ValidStrW(d->EmptyText) &&
				!ValidStrW(LBase::NameW()) &&
				!HasFocus;

	// LgiTrace("LEdit::SysEmptyText Empty=%i W=%p Focus=%i\n", Empty, LBase::NameW(), HasFocus);
		
	if (Empty)
	{
		// Show empty text
		GColour c(L_LOW);
		if (!d->InEmptyMode)
		{
			d->NonEmptyColor = GetCss(true)->Color();
			d->InEmptyMode = true;
		}
		GetCss()->Color(LCss::ColorDef(LCss::ColorRgb, c.c32()));
		
		bool Old = d->IgnoreNotify;
		d->IgnoreNotify = true;
		Status = SysName(d->EmptyText);
		d->IgnoreNotify = Old;
		
		if (_View)
		{
			DWORD Style = GetWindowLong(_View, GWL_STYLE);
			SetWindowLong(_View, GWL_STYLE, Style & ~ES_PASSWORD);
			SendMessage(_View, EM_SETPASSWORDCHAR, 0, 0);
		}
	}
	else
	{
		// Show normal text
		if (d->InEmptyMode)
		{
			GetCss()->Color(d->NonEmptyColor);
			d->InEmptyMode = false;
		}

		if (_View)
		{
			uint32_t Style = GetStyle();
			bool HasPass = (Style & ES_PASSWORD) != 0;
			if (HasPass)
			{
				SetWindowLong(_View, GWL_STYLE, Style);
				SendMessage(_View, EM_SETPASSWORDCHAR, (WPARAM)'*', 0);
			}
		}

		Status = SysName(LBase::NameW());
	}
	
	Invalidate();
	return Status;
}

void LEdit::SetEmptyText(const char *EmptyText)
{
	d->EmptyText.Reset(Utf8ToWide(EmptyText));
	SysEmptyText();
}
