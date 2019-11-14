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
NSString *const LBinaryDataPBoardType = @"com.memecode.lgi";

static void _dump(const char *verb, uchar *ptr, uint64_t len)
{
	printf("%s " LPrintfInt64 " bytes:\n", verb, len);
	for (int i=0; i<len; i++)
	{
		printf("%02.2x ", ptr[i]);
		if (i % 16 == 15) printf("\n");
	}
	printf("\n");
}

#define LBinaryData_Magic 'lgid'
struct LBinaryData_Hdr
{
	uint32_t Magic;
	uint32_t FormatLen;
	uint64_t DataLen;
	char Format[1];
	
	void *GetData()
	{
		return Format + FormatLen + 1;
	}
};

@implementation LBinaryData

- (id)init:(GString)fmt ptr:(uchar*)ptr len:(ssize_t)Len
{
	if ((self = [super init]) != nil)
	{
		GArray<char> mem;
		mem.Length(sizeof(LBinaryData_Hdr)+fmt.Length());
		LBinaryData_Hdr *h = (LBinaryData_Hdr*)mem.AddressOf();
		h->Magic = LBinaryData_Magic;
		h->FormatLen = (uint32_t)fmt.Length();
		h->DataLen = Len;
		strcpy(h->Format, fmt);
		
		NSMutableData *m;
		self.data = m = [[NSMutableData alloc] initWithBytes:mem.AddressOf() length:mem.Length()];
		[m appendBytes:ptr length:Len];
		
		// _dump("Pasting", ptr, h->DataLen);
	}
	
	return self;
}

- (id)init:(NSData*)d
{
	if ((self = [super init]) != nil)
	{
		self.data = d;
	}
	
	return self;
}

// Any of these parameters can be non-NULL if the caller doesn't care about them
- (bool)getData:(GString*)Format data:(GAutoPtr<uint8,true>*)Ptr len:(ssize_t*)Len
{
	if (!self.data)
	{
		LgiTrace("%s:%i - No data object.\n", _FL);
		return false;
	}

	GArray<char> mem;
	if (!mem.Length(MIN(self.data.length, 256)))
	{
		LgiTrace("%s:%i - Alloc failed.\n", _FL);
		return false;
	}
	[self.data getBytes:mem.AddressOf() length:mem.Length()];
	LBinaryData_Hdr *h = (LBinaryData_Hdr*)mem.AddressOf();
	if (h->Magic != LBinaryData_Magic)
	{
		LgiTrace("%s:%i - Data block missing magic.\n", _FL);
		return false;
	}

	if (Len)
		*Len = h->DataLen;
	if (Format)
		*Format = h->Format;
	
	if (Ptr)
	{
		if (!Ptr->Reset(new uint8_t[h->DataLen]))
		{
			LgiTrace("%s:%i - Failed to alloc " LPrintfInt64 " bytes.\n", _FL, h->DataLen);
			return false;
		}
		
		NSRange r;
		r.location = sizeof(LBinaryData_Hdr) + h->FormatLen;
		r.length = h->DataLen;
		[self.data getBytes:Ptr->Get() range:r];

		// _dump("Receiving", Ptr.Get(), h->DataLen);
	}

	return true;
}

- (nullable id)pasteboardPropertyListForType:(NSString *)type
{
	if ([type isEqualToString:LBinaryDataPBoardType])
		return self.data;
	
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


#define LGI_ClipBoardType "clipboard-binary"

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	auto data = [[LBinaryData alloc] init:LGI_ClipBoardType ptr:Ptr len:Len];
	NSArray *array = [NSArray arrayWithObject:data];
	[pasteboard clearContents];
	auto r = [pasteboard writeObjects:array];
	
	return r;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8,true> &Ptr, ssize_t *Len)
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	auto d = [pasteboard dataForType:LBinaryDataPBoardType];
	if (!d)
	{
		LgiTrace("%s:%i - No LBinaryDataPBoardType data.\n", _FL);
		return false;
	}

	auto data = [[LBinaryData alloc] init:d];
	if (!data)
	{
		LgiTrace("%s:%i - LBinaryData alloc failed.\n", _FL);
		return false;
	}
	
	auto Status = [data getData:NULL data:&Ptr len:Len];
	[data release];

	return Status;
}

