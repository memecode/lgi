// Clipboard Implementation
#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/ClipBoard.h"

#include <Clipboard.h>

#define DEBUG_CLIPBOARD					0
#define LGI_CLIP_BINARY					"lgi.binary"

class LClipBoardPriv : public BClipboard
{
public:
	LString Txt;
	LAutoWString WTxt;
	
	LClipBoardPriv() : BClipboard(NULL)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
LClipBoard::LClipBoard(LView *o)
{
	d = new LClipBoardPriv;
	Owner = o;
	Open = false;

}

LClipBoard::~LClipBoard()
{
	DeleteObj(d);
}

bool LClipBoard::Empty()
{
	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		return false;
	}

    d->Clear();
    d->Unlock();

	return true;
}

bool LClipBoard::EnumFormats(::LArray<FormatType> &Formats)
{
	return false;
}

bool LClipBoard::Html(const char *doc, bool AutoEmpty)
{
	return false;
}

::LString LClipBoard::Html()
{
	return ::LString();
}

bool LClipBoard::Text(const char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}

	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		return false;
	}
 
    auto clip = d->Data();

    clip->AddData("text/plain", B_MIME_TYPE, Str, strlen(Str));
 
    Status = d->Commit() == B_OK;
    if (!Status)
        LgiTrace("%s:%i - Could not commit data to clipboard.\n", _FL);
 
    d->Unlock();

	return Status;
}

char *LClipBoard::Text()
{
	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		return NULL;
	}
	
    auto clip = d->Data();
 
	const char *string = NULL;
	ssize_t stringLen = 0;
    clip->FindData("text/plain", B_MIME_TYPE, (const void **)&string, &stringLen);
    d->Txt.Set(string, stringLen);
 
    d->Unlock();
		
	return d->Txt;
}

// Wide char versions for plain text
bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	LAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *LClipBoard::TextW()
{
	auto u = Text();
	d->WTxt.Reset(Utf8ToWide(u));
	return d->WTxt;
}

bool LClipBoard::Bitmap(LSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	return Status;
}

LAutoPtr<LSurface> LClipBoard::Bitmap()
{
	LAutoPtr<LSurface> img;

	return img;
}

bool LClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	return false;
}

::LString::Array LClipBoard::Files()
{
	::LString::Array a;
	return a;
}

bool LClipBoard::Files(::LString::Array &a, bool AutoEmpty)
{
	return false;
}

struct ReceiveData
{
	LAutoPtr<uint8_t,true> *Ptr;
	ssize_t *Len;
};

bool LClipBoard::Binary(FormatType Format, LAutoPtr<uint8_t,true> &Ptr, ssize_t *Len)
{
	return false;
}

