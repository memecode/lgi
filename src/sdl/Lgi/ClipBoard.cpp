// Clipboard Implementation
#include "Lgi.h"
#include "LVariant.h"
#include "LClipBoard.h"

#define DEBUG_CLIPBOARD		0

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
	pDC = 0;
}

LClipBoard::~LClipBoard()
{
	DeleteObj(d);
}

bool LClipBoard::Empty()
{
	return false;
}

bool LClipBoard::Text(char *Str, bool AutoEmpty)
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

bool LClipBoard::TextW(char16 *Str, bool AutoEmpty)
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

LSurface *LClipBoard::Bitmap()
{
	return pDC;
}

bool LClipBoard::Binary(FormatType Format, uint8 *Ptr, ssize_t Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
	}

	return Status;
}

bool LClipBoard::Binary(FormatType Format, LAutoPtr<uint8> &Ptr, ssize_t *Len)
{
	bool Status = false;

	if (Ptr && Len)
	{
	}

	return Status;
}

