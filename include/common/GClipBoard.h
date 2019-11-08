/// \file
#ifndef _GCLIPBOARD_H
#define _GCLIPBOARD_H

/// Clipboard API
class LgiClass GClipBoard
{
	class GClipBoardPriv *d;
	GView *Owner;
	bool Open;
	GAutoPtr<GSurface> pDC;

	#if LGI_COCOA
	GString Txt;
	#else
	GAutoString Txt;
	#endif
	GAutoWString wTxt;

public:
	/// On windows, this equates to a CF_TEXT, CF_BITMAP, CF_DIB type #define
	typedef uint32_t FormatType;
	static GString FmtToStr(FormatType Fmt);
	static FormatType StrToFmt(GString Fmt);

	/// Creates the clipboard access object.
	GClipBoard(GView *o);
	~GClipBoard();

	bool IsOpen() { return Open; }
	GClipBoard &operator =(GClipBoard &c);

	/// Empties the clipboard of it's current content.
	bool Empty();
	bool EnumFormats(GArray<FormatType> &Formats);

	// Text
	bool Text(char *Str, bool AutoEmpty = true);
	char *Text(); // ptr returned is still owned by this object

	bool TextW(char16 *Str, bool AutoEmpty = true);
	char16 *TextW(); // ptr returned is still owned by this object

	// HTML
	bool Html(const char *doc, bool AutoEmpty = true);
	GString Html();

	// Bitmap
	bool Bitmap(GSurface *pDC, bool AutoEmpty = true);
	GSurface *Bitmap();
	#if WINNATIVE
	GSurface *ConvertFromPtr(void *Ptr);
	#endif

	// Files
	GString::Array Files();
	bool Files(GString::Array &Paths, bool AutoEmpty = true);

	// Binary
	bool Binary(FormatType Format, uint8_t *Ptr, ssize_t Len, bool AutoEmpty);	// Set
	bool Binary(FormatType Format, GAutoPtr<uint8_t,true> &Ptr, ssize_t *Len);	// Get
};

#ifdef __OBJC__

	extern NSString *const LBinaryDataPBoardType;

	@interface LBinaryData : NSObject<NSPasteboardWriting,NSPasteboardReading>
	{
	}
	@property(assign) NSData *data;
	- (id)init:(uchar*)ptr len:(ssize_t)Len;

	// Writer
	- (nullable id)pasteboardPropertyListForType:(NSString *)type;
	- (NSArray<NSString *> *)writableTypesForPasteboard:(NSPasteboard *)pasteboard;

	// Reader
	+ (NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard;
	+ (NSArray<NSString *> *)readableTypesForPasteboard:(NSPasteboard *)pasteboard;

	@end

#endif


#endif
