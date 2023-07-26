// MacOSX Clipboard Implementation
#include "lgi/common/Lgi.h"
#include "lgi/common/ClipBoard.h"

#define kClipboardTextType				"public.utf16-plain-text"

class LClipBoardPriv
{
public:
	
	LClipBoardPriv()
	{
	}

	~LClipBoardPriv()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
LClipBoard::LClipBoard(LView *o)
{
	d = new LClipBoardPriv;
	Owner = o;
	Open = true;
}

LClipBoard::~LClipBoard()
{
	DeleteObj(d);
}

LString LClipBoard::FmtToStr(LClipBoard::FormatType Fmt)
{
	return Fmt;
}

LClipBoard::FormatType LClipBoard::StrToFmt(LString Str)
{
	return Str;
}

bool LClipBoard::EnumFormats(LArray<FormatType> &Formats)
{
	LAutoPool Ap;

	LHashTbl<StrKey<char,false>, bool> map;
	auto pb = [NSPasteboard generalPasteboard];
	for (NSPasteboardItem *item in [pb pasteboardItems])
	{
		auto types = [item types];
		for (NSPasteboardType type in types)
			map.Add(LString(type), true);
	}
	for (auto p: map)
		Formats.Add(p.key);
	
	return Formats.Length() > 0;
}

bool LClipBoard::Empty()
{
	LAutoPool Ap;
	bool Status = false;

	Txt.Empty();
	wTxt.Reset();

	auto *pb = [NSPasteboard generalPasteboard];
	[pb clearContents];

	return Status;
}

bool LClipBoard::Text(const char *Str, bool AutoEmpty)
{
	LAutoPool Ap;
	
	LAssert(AutoEmpty); // If the pasteboard isn't emptied we can't set it's value
	Empty();
	
	Txt = Str;
	wTxt.Reset();
	
	auto *pb = [NSPasteboard generalPasteboard];
	auto *text = Txt.NsStr();
	[pb addTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
	auto result = [pb setString:text forType:NSPasteboardTypeString];
	LAssert(result);
	return result != 0;
}

char *LClipBoard::Text()
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

bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
		Empty();
	
	Txt = Str;
	wTxt.Reset();
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *array = [NSArray arrayWithObject:Txt.NsStr()];
	return [pasteboard writeObjects:array];
}

char16 *LClipBoard::TextW()
{
	Text();
	wTxt.Reset(Utf8ToWide(Txt));
	return wTxt;
}

bool LClipBoard::Html(const char *doc, bool AutoEmpty)
{
	LAutoPool Ap;

	if (AutoEmpty)
		Empty();
	
	Txt = doc;
	wTxt.Reset();
	
	auto attrStr = [[NSAttributedString alloc] initWithData:[Txt.NsStr() dataUsingEncoding:NSUTF8StringEncoding]
												options:@{NSDocumentTypeDocumentAttribute: NSHTMLTextDocumentType,
														  NSCharacterEncodingDocumentAttribute: @(NSUTF8StringEncoding)}
												documentAttributes:nil error:nil];
	
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSArray *array = [NSArray arrayWithObject:attrStr];
	return [pasteboard writeObjects:array];
}

LString LClipBoard::Html()
{
	LAutoPool Ap;
	
	NSArray *classes = [[NSArray alloc] initWithObjects:[NSAttributedString class], nil];
	NSArray *copiedItems = [[NSPasteboard generalPasteboard]
								readObjectsForClasses:classes
								options:[NSDictionary dictionary]];
	if (copiedItems != nil)
	{
		for (NSAttributedString *s in copiedItems)
		{
			NSDictionary *documentAttributes = @{NSDocumentTypeDocumentAttribute: NSHTMLTextDocumentType};
			NSData *htmlData = [s dataFromRange:NSMakeRange(0, s.length) documentAttributes:documentAttributes error:NULL];
			if (htmlData)
			{
				NSString *htmlString = [[NSString alloc] initWithData:htmlData encoding:NSUTF8StringEncoding];
				if (htmlString)
				{
					Txt = htmlString;
					[htmlString release];
					break;
				}
			}
		}
	}
	[classes release];

	return Txt;
}

bool LClipBoard::Bitmap(LSurface *pDC, bool AutoEmpty)
{
	return false;
}

void LClipBoard::Bitmap(BitmapCb Callback)
{
	LAutoPtr<LSurface> Img;
	if (Callback)
		Callback(Img, "Not implemented.");
}

void LClipBoard::Files(FilesCb Callback)
{
	LString::Array Files;
	if (Callback)
		Callback(Files, "Not implemented.");
}

bool LClipBoard::Files(LString::Array &Paths, bool AutoEmpty)
{
	return false;
}

// This is a custom type to wrap binary data.
NSString *const LBinaryDataPBoardType = @"com.memecode.lgi";

#if 0
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
#endif

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

- (id)init:(LString)fmt ptr:(uchar*)ptr len:(ssize_t)Len
{
	if ((self = [super init]) != nil)
	{
		LArray<char> mem;
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
- (bool)getData:(LString*)Format data:(LString*)Str var:(LVariant*)Var
{
	if (!self.data)
	{
		LgiTrace("%s:%i - No data object.\n", _FL);
		return false;
	}

	LArray<char> mem;
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

	// h->DataLen;
	if (Format)
		*Format = h->Format;
	
	if (Str)
	{
		if (!Str->Length(h->DataLen))
		{
			LgiTrace("%s:%i - Failed to alloc " LPrintfInt64 " bytes.\n", _FL, h->DataLen);
			return false;
		}
		
		NSRange r;
		r.location = sizeof(LBinaryData_Hdr) + h->FormatLen;
		r.length = h->DataLen;
		[self.data getBytes:Str->Get() range:r];

		// _dump("Receiving", Ptr.Get(), h->DataLen);
	}
	else if (Var)
	{
		Var->Empty();
		Var->Type = GV_BINARY;
		Var->Value.Binary.Length = h->DataLen;
		if ((Var->Value.Binary.Data = new char[h->DataLen]))
		{
			NSRange r;
			r.location = sizeof(LBinaryData_Hdr) + h->FormatLen;
			r.length = h->DataLen;
			[self.data getBytes:Var->Value.Binary.Data range:r];
		}
		else return false;
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

bool LClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
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

void LClipBoard::Binary(FormatType Format, BinaryCb Callback)
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	auto d = [pasteboard dataForType:LBinaryDataPBoardType];
	if (!d)
	{
		if (Callback) Callback(LString(), "No LBinaryDataPBoardType data.");
		return;
	}

	auto data = [[LBinaryData alloc] init:d];
	if (!data)
	{
		if (Callback) Callback(LString(), "LBinaryData alloc failed.");
		return;
	}
	
	LString str;
	[data getData:NULL data:&str var:NULL];
	[data release];

	if (Callback) Callback(str, LString());
}

