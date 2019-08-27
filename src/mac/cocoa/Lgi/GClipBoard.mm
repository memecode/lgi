// MacOSX Clipboard Implementation
#include "Lgi.h"
#include "GClipBoard.h"

#define kClipboardTextType				"public.utf16-plain-text"

class GClipBoardPriv
{
public:
	
	GClipBoardPriv()
	{
	}

	~GClipBoardPriv()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
	d = new GClipBoardPriv;
	Owner = o;
	Open = true;
}

GClipBoard::~GClipBoard()
{
	DeleteObj(d);
}

bool GClipBoard::Empty()
{
	LAutoPool Ap;
	bool Status = false;

	Txt.Empty();
	wTxt.Reset();

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];

	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	LAutoPool Ap;
	
	if (AutoEmpty)
	{
		Empty();
	}
	
	Txt = Str;
	wTxt.Reset();
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *array = [NSArray arrayWithObject:Txt.NsStr()];
	[pasteboard writeObjects:array];

	return Txt;
}

char *GClipBoard::Text()
{
	LAutoPool Ap;
	
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
	NSDictionary *options = [NSDictionary dictionary];
	NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];
	if (copiedItems != nil)
	{
		for (NSString *s in copiedItems)
		{
			Txt = [s UTF8String];
			break;
		}
		
		[copiedItems release];
	}

    // Do something with the contents...
	return Txt;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
		Empty();
	
	Txt = Str;
	wTxt.Reset();
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *array = [NSArray arrayWithObject:Txt.NsStr()];
	[pasteboard writeObjects:array];

	return Status;
}

char16 *GClipBoard::TextW()
{
	Text();
	wTxt.Reset(Utf8ToWide(Txt));
	return wTxt;
}

bool GClipBoard::Html(const char *doc, bool AutoEmpty)
{
	return false;
}

GString GClipBoard::Html()
{
	return GString();
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC && Owner)
	{
		LgiAssert(!"Not impl");
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	GSurface *pDC = NULL;

	LgiAssert(!"Not impl");
	
	return pDC;
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
		LgiAssert(!"Not impl");
	}

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8,true> &Ptr, ssize_t *Len)
{
	bool Status = false;

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *classes = [[NSArray alloc] initWithObjects:[NSPasteboardItem class], nil];
	NSDictionary *options = [NSDictionary dictionary];
	NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];
	if (copiedItems != nil)
	{
		for (NSPasteboardItem *i in copiedItems)
		{
			GString::Array Types;
			Types.SetFixedLength(false);
			for (id type in i.types)
				Types.New() = [type UTF8String];
			
			// Pick a type and decode it...
			printf("%s:%i - FIXME decode a binary type here.\n", _FL);
		}
		[copiedItems release];
	}

	return Status;
}

