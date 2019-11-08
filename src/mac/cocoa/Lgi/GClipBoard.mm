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
		Empty();
	
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
	
	NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
	NSArray *copiedItems = [[NSPasteboard generalPasteboard]
								readObjectsForClasses:classes
								options:[NSDictionary dictionary]];
	if (copiedItems != nil)
	{
		for (NSString *s in copiedItems)
		{
			Txt = [s UTF8String];
			break;
		}
	}
	[classes release];

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
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	GSurface *pDC = NULL;


	return pDC;
}

// This is a custom type to wrap binary data.
NSString *const LBinaryDataPBoardType = @"com.memecode.lgi.binary";

@implementation LBinaryData

- (id)init:(uchar*)ptr len:(ssize_t)Len
{
	if ((self = [super init]) != nil)
	{
		self.data = [[NSData alloc] initWithBytes:ptr length:Len];
	}
	
	return self;
}

- (nullable id)pasteboardPropertyListForType:(NSString *)type
{
	if ([type isEqualToString:LBinaryDataPBoardType])
	{
		return self.data;
	}
	
	return nil;
}

- (NSArray<NSString *> *)writableTypesForPasteboard:(NSPasteboard *)pasteboard
{
	return [NSArray arrayWithObjects:LBinaryDataPBoardType, kUTTypeData, nil];
}

+ (NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard
{
	return NSPasteboardReadingAsData;
}

+ (NSArray<NSString *> *)readableTypesForPasteboard:(NSPasteboard *)pasteboard
{
	return [NSArray arrayWithObjects:LBinaryDataPBoardType, kUTTypeData, nil];
}
@end


bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	auto data = [[LBinaryData alloc] init:Ptr len:Len];
	NSArray *array = [NSArray arrayWithObject:data];
	[pasteboard clearContents];
	auto r = [pasteboard writeObjects:array];
	
	return r;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8,true> &Ptr, ssize_t *Len)
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	auto d = [pasteboard dataForType:LBinaryDataPBoardType];
	if (d)
	{
		if (Len)
			*Len = d.length;
		if (Ptr.Reset(new uint8_t[d.length]))
		{
			[d getBytes:Ptr.Get() length:d.length];
			return true;
		}
	}
	
	return false;
}

