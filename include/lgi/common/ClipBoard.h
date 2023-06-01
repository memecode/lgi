/// \file
#ifndef _GCLIPBOARD_H
#define _GCLIPBOARD_H

#include <functional>

/// Clipboard API
class LgiClass LClipBoard
{
	class LClipBoardPriv *d;
	LView *Owner;
	bool Open;
	LAutoPtr<LSurface> pDC;

	#if LGI_COCOA
	LString Txt;
	#else
	LAutoString Txt;
	#endif
	LAutoWString wTxt;

public:
	/// On windows, this equates to a CF_TEXT, CF_BITMAP, CF_DIB type #define
	typedef uint32_t FormatType;
	typedef std::function<void(LAutoPtr<LSurface> Img, LString Err)> BitmapCb;
	typedef std::function<void(LString Data, LString Err)> BinaryCb;
	static LString FmtToStr(FormatType Fmt);
	static FormatType StrToFmt(LString Fmt);

	/// Creates the clipboard access object.
	LClipBoard(LView *o);
	~LClipBoard();

	bool IsOpen() { return Open; }
	LClipBoard &operator =(LClipBoard &c);

	/// Empties the clipboard of it's current content.
	bool Empty();
	bool EnumFormats(LArray<FormatType> &Formats);

	// Text
	bool Text(const char *Str, bool AutoEmpty = true);
	char *Text(); // ptr returned is still owned by this object

	bool TextW(const char16 *Str, bool AutoEmpty = true);
	char16 *TextW(); // ptr returned is still owned by this object

	// HTML
	bool Html(const char *doc, bool AutoEmpty = true);
	LString Html();

	// Bitmap
	bool Bitmap(LSurface *pDC, bool AutoEmpty = true);
	void Bitmap(BitmapCb Callback);

	// Files
	LString::Array Files();
	bool Files(LString::Array &Paths, bool AutoEmpty = true);

	// Binary
	bool Binary(FormatType Format, uint8_t *Ptr, ssize_t Len, bool AutoEmpty);	// Set
	void Binary(FormatType Format, BinaryCb Callback);	// Get

	#if WINNATIVE
		LAutoPtr<LSurface> ConvertFromPtr(void *Ptr);
	#elif defined(LINUX)
		void FreeImage(unsigned char *pixels);
	#endif
};

#ifdef __OBJC__

	extern NSString *const LBinaryDataPBoardType;

	@interface LBinaryData : NSObject<NSPasteboardWriting,NSPasteboardReading>
	{
	}
	@property(assign) NSData *data;

	- (id)init:(LString)format ptr:(uchar*)ptr len:(ssize_t)Len;
	- (id)init:(NSData*)data;
	- (bool)getData:(LString*)format data:(LAutoPtr<uint8,true>*)Ptr len:(ssize_t*)Len var:(LVariant*)var;

	// Writer
	- (nullable id)pasteboardPropertyListForType:(NSString *)type;
	- (NSArray<NSString *> *)writableTypesForPasteboard:(NSPasteboard *)pasteboard;

	// Reader
	+ (NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard;
	+ (NSArray<NSString *> *)readableTypesForPasteboard:(NSPasteboard *)pasteboard;

	@end

#endif


#endif
