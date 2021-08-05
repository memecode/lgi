// \file
// \author Matthew Allen (fret@memecode.com)
#include "lgi/common/Lgi.h"
#include "lgi/common/Edit.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/LgiRes.h"

///////////////////////////////////////////////////////////////////////////////////////////
// Edit
#include "lgi/common/TextView3.h"

class _OsFontType : public LFontType
{
public:
	LFont *Create(LSurface *pDC = NULL)
	{
		return LSysFont;
	}
};

static _OsFontType SysFontType;

class LEditPrivate
{
public:
	bool FocusOnCreate;
	bool Multiline;
	bool Password;
	bool NotificationProcessed;
	LAutoString EmptyTxt;
	
	LEditPrivate()
	{
		FocusOnCreate = false;
		NotificationProcessed = false;
	}
};

void LEdit::KeyProcessed()
{
	d->NotificationProcessed = true;
}

LEdit::LEdit(int id, int x, int y, int cx, int cy, const char *name) :
	#if WINNATIVE
	ResObject(Res_EditBox)
	#else
	LTextView3(id, x, y, cx, cy, &SysFontType)
	#endif
{
	#if !WINNATIVE
	_ObjName = Res_EditBox;
	SetUrlDetect(false);
	SetWrapType(TEXTED_WRAP_NONE);
	#endif
	_OsFontType Type;
	d = new LEditPrivate;

	LDisplayString Ds(LSysFont, (char*)(name?name:"A"));
	if (cx < 0) cx = Ds.X() + 6;
	if (cy < 0) cy = Ds.Y() + 4;

	d->Multiline = false;
	d->Password = false;

	Sunken(true);
	if (name) Name(name);

	LRect r(x, y, x+MAX(cx, 10), y+MAX(cy, 10));
	SetPos(r);
	LResources::StyleElement(this);
}

LEdit::~LEdit()
{
	DeleteObj(d);
}

void LEdit::SetEmptyText(const char *EmptyText)
{
	d->EmptyTxt.Reset(NewStr(EmptyText));
	Invalidate();
}

void LEdit::SendNotify(int Data)
{
	if (Data == LNotifyDocChanged)
		return LTextView3::SendNotify(0);
	else if (Data == LK_RETURN ||
			Data == LNotifyEscapeKey)
		return LTextView3::SendNotify(Data);
}

LRange LEdit::GetSelectionRange()
{
	LRange r;
	ssize_t Sel = LTextView3::GetCaret(false);
	ssize_t Cur = LTextView3::GetCaret();
	if (Sel < Cur)
	{
		r.Start = Sel;
		r.Len = Cur - Sel;
	}
	else
	{
		r.Start = Cur;
		r.Len = Sel - Cur;
	}
	return r;
}

void LEdit::Select(int Start, int Len)
{
	LTextView3::SetCaret(Start, false);
	LTextView3::SetCaret(Start + (Len > 0 ? Len : 0x7fffffff) - 1, true);
}

ssize_t LEdit::GetCaret(bool Cursor)
{
	return LTextView3::GetCaret(Cursor);
}

void LEdit::SetCaret(size_t Pos, bool Select, bool ForceFullUpdate)
{
	LTextView3::SetCaret(Pos, Select, ForceFullUpdate);
}

void LEdit::Value(int64 i)
{
	char Str[32];
	sprintf(Str, LPrintfInt64, i);
	Name(Str);
}

int64 LEdit::Value()
{
	auto n = Name();
	return (n) ? atoi(n) : 0;
}

bool LEdit::MultiLine()
{
	return d->Multiline;
}

void LEdit::MultiLine(bool m)
{
	d->Multiline = m;
}

bool LEdit::Password()
{
	return d->Password;
}

void LEdit::Password(bool m)
{
	SetObscurePassword(d->Password = m);
}

bool LEdit::SetScrollBars(bool x, bool y)
{
	// Prevent scrollbars if the control is not multiline.
	return LTextView3::SetScrollBars(x, y && d->Multiline);
}

void LEdit::OnPaint(LSurface *pDC)
{
    LTextView3::OnPaint(pDC);

    if (!ValidStr(Name()) && d->EmptyTxt && !Focus())
    {
        LFont *f = GetFont();
        LDisplayString ds(f, d->EmptyTxt);
        
        static int diff = -1;
        if (diff < 0)
        	diff = abs(LColour(L_MED).GetGray()-LColour(L_WORKSPACE).GetGray());
        
        f->Colour(diff < 30 ? L_LOW : L_MED, L_WORKSPACE);
        f->Transparent(true);
        ds.Draw(pDC, 2, 2);
    }
}

bool LEdit::OnKey(LKey &k)
{
	d->NotificationProcessed = false;
	
	switch (k.vkey)
	{
	 	case LK_RETURN:
	 	#ifndef LINUX
		case LK_KEYPADENTER:
		#endif
		{
			if (k.Down())
				SendNotify(LNotifyReturnKey);
			break;
		}
		case LK_ESCAPE:
		{
			if (k.Down())
				SendNotify(LNotifyEscapeKey);
			break;
		}
		case LK_BACKSPACE:
		{
			if (k.Down())
				SendNotify(LNotifyBackspaceKey);
			break;
		}
		case LK_DELETE:
		{
			if (k.Down())
				SendNotify(LNotifyDeleteKey);
			break;
		}
	}

	if
	(
		!d->Multiline &&
		(
			k.vkey == LK_TAB ||
			k.vkey == LK_RETURN  ||
			#ifndef LINUX
			k.vkey == LK_KEYPADENTER ||
			#endif
			k.vkey == LK_ESCAPE
		)
	)
	{	
		return d->NotificationProcessed;
	}
	
	return LTextView3::OnKey(k);
}

void LEdit::OnEnter(LKey &k)
{
	if (d->Multiline)
		LTextView3::OnEnter(k);
	else
		SendNotify(LNotifyReturnKey);
}

bool LEdit::Paste()
{
	LClipBoard Clip(this);

	LAutoWString Mem;
	char16 *t = Clip.TextW();
	if (!t) // ala Win9x
	{
		char *s = Clip.Text();
		if (s)
		{
			Mem.Reset(Utf8ToWide(s));
			t = Mem;
		}
	}

	if (!t)
		return false;

	if (SelStart >= 0)
	{
		DeleteSelection();
	}

	// remove '\r's
	char16 *in = t, *out = t;
	for (; *in; in++)
	{
		if (*in == '\n')
		{
			if (d->Multiline)
				*out++ = *in;
		}
		else if (*in != '\r')
		{
			*out++ = *in;
		}
	}
	*out++ = 0;

	// insert text
	size_t Len = StrlenW(t);
	Insert(Cursor, t, Len);
	LTextView3::SetCaret(Cursor+Len, false, true); // Multiline
	
	return true;
}

