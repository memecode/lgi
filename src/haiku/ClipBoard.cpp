// Clipboard Implementation
#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/ClipBoard.h"

#define DEBUG_CLIPBOARD					0
#define VAR_COUNT						16
#define LGI_CLIP_BINARY					"lgi.binary"
#define LGI_RECEIVE_CLIPBOARD_TIMEOUT	4000

struct ClipData : public LMutex
{
	::LVariant v[VAR_COUNT];
	
	ClipData() : LMutex("ClipData")
	{
	}
	
}	Data;

class LClipBoardPriv
{
public:
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
	return false;
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
	
	return Status;
}

char *LClipBoard::Text()
{
	char *t = 0;
	
	return t;
}

bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	LAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *LClipBoard::TextW()
{
	LAutoString u(Text());
	return Utf8ToWide(u);
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

	::LVariant *p = NULL;
	if (Data.Lock(_FL))
	{
		for (int i=0; i<VAR_COUNT; i++)
		{
			if (Data.v[i].Type == GV_NULL)
			{
				p = Data.v + i;
				p->SetBinary(Len, Ptr);
				break;
			}
		}
		Data.Unlock();
	}
	
	if (!p)
	{
		#if DEBUG_CLIPBOARD
		printf("%s:%i - no slots to store data\n", _FL);
		#endif
		return false;
	}
	
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

