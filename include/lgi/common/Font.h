/// \file
/// \author Matthew Allen

#ifndef _GDCFONT_H_
#define _GDCFONT_H_

#include "string.h"
#include "lgi/common/Rect.h"
#include "LgiOsClasses.h"
#include "lgi/common/Colour.h"
#include "lgi/common/Capabilities.h"
#include "lgi/common/Css.h"

#include <functional>

//////////////////////////////////////////////////////////////
// Defines

#ifndef WIN32

// font weights
#define FW_DONTCARE						0 
#define FW_THIN							100
#define FW_EXTRALIGHT					200
#define FW_ULTRALIGHT					200
#define FW_LIGHT						300
/// The default font weight
#define FW_NORMAL						400
#define FW_REGULAR						400
#define FW_MEDIUM						500
#define FW_SEMIBOLD						600
#define FW_DEMIBOLD						600
/// Bold font weight
#define FW_BOLD							700
#define FW_EXTRABOLD					800
#define FW_ULTRABOLD					800
#define FW_HEAVY						900
#define FW_BLACK						900

// quality
/// Default font quality
#define DEFAULT_QUALITY					0
/// Specifically anti-aliased font
#define ANTIALIASED_QUALITY				1
/// Specifically not anti-alias font
#define NONANTIALIASED_QUALITY			2

#elif defined WIN32

#define WESTEUROPE_CHARSET				BALTIC_CHARSET		// ??? don't know

#endif

// OS specific font type
#if defined(LGI_SDL)

	#define PrevOsChar(Ptr)	Ptr--
	#define NextOsChar(Ptr)	Ptr++

#elif defined __GTK_H__

	#include "LgiOsClasses.h"
	#define PrevOsChar(Ptr)	Ptr--
	#define NextOsChar(Ptr)	Ptr++

#elif defined(WIN32)

	#define PrevOsChar(Ptr)	Ptr--
	#define NextOsChar(Ptr)	Ptr++

#endif

#define LGI_WHITESPACE_WEIGHT			0.15f		// amount of foreground colour in visible whitespace
#define MAX_UNICODE						0x10FFFF	// maximum unicode char I can handle
#define _HasUnicodeGlyph(map, u)		( (map[(u)>>3] & (1 << ((u) & 7))) != 0  )

enum LPxToIndexType
{
	LgiTruncate,
	LgiNearest
};

//////////////////////////////////////////////////////////////
// Classes
class LFontType;

/// Font parameters collection
class LgiClass LTypeFace
{
protected:
	class LTypeFacePrivate *d;

	// Methods
	virtual void _OnPropChange(bool c) {} // if false then it's just a property change

public:
	LTypeFace();
	virtual ~LTypeFace();

	/// Sets the font face name
	void Face(const char *s);
	/// Sets the font size
	void Size(LCss::Len);
	/// Sets the point size
	void PointSize(int i);
	/// Sets the tab size in device units (pixels)
	void TabSize(int i);
	/// Sets the quality resquested, use one of #DEFAULT_QUALITY, #ANTIALIASED_QUALITY or #NONANTIALIASED_QUALITY.
	void Quality(int i);

	/// Sets the foreground colour as a 24 bit RGB value
	void Fore(LSystemColour c);
	void Fore(LColour c);

	void Back(LSystemColour c);
	void Back(LColour c);

    /// Get the whitespace rendering colour
    LColour WhitespaceColour();
    /// Sets the rendering colour of whitespace characters when LDisplayString::ShowVisibleTab() is true.
    void WhitespaceColour(LColour c);

	/// Sets the font's weight, use one the weight defines in LFont.h, e.g. #FW_NORMAL, #FW_BOLD
	void SetWeight(int Weight);
	/// Set a bold font
	void Bold(bool i) { SetWeight(i ? FW_BOLD : FW_NORMAL); }
	/// Sets the font to italic
	void Italic(bool i);
	/// Draws with an underline
	void Underline(bool i);
	/// Makes the text have no background.
	void Transparent(bool i);
	/// Turns glyph substitution on or off
	void SubGlyphs(bool i);

	/// \returns the font face
	const char *Face() const;
	/// \returns the point size (avoid, use 'Size' instead)
	int PointSize() const;
	/// \returns the size
	LCss::Len Size() const;
	/// \returns the tabsize in pixels
	int TabSize() const;
	/// \returns the quality setting
	int Quality() const;
	/// \returns the foreground colour.
	LColour Fore() const;
	/// \returns the background colour.
	LColour Back() const;
	/// \returns the font weight.
	int GetWeight() const;
	/// \returns true if this is a bold font.
	bool Bold() const { return GetWeight() >= FW_BOLD; }
	/// \returns true if this is a italic font.
	bool Italic() const;
	/// \returns true if this font is drawn with an underline.
	bool Underline() const;
	/// \returns true if no background will be drawn.
	bool Transparent() const;
	/// \returns true if glyph substitution will be done.
	bool SubGlyphs() const;
	/// \returns the amount of space above the baseline.
	double Ascent() const;
	/// \returns the amount of space below the baseline.
	double Descent() const;
	/// \returns the amount of normally unused space at the top of the Ascent.
	double Leading() const;

	/// \returns true if the font types are the same
	bool operator ==(const LTypeFace &t);

	/// Set the foreground and background in 24-bit colour.
	/// \sa LTypeFace::Fore() and LTypeFace::Back()
	virtual void Colour(LSystemColour Fore, LSystemColour Back = L_TRANSPARENT);

	/// Set the foreground and background colour.
	virtual void Colour(LColour Fore, LColour Back);
};

/// \brief Font class.
class LgiClass LFont :
	public LTypeFace
{
	friend class LFontSystem;
	friend class LDisplayString;

protected:
	class LFontPrivate *d;

	// Methods
	bool IsValid();
	void _OnPropChange(bool Change);
	char16 *_ToUnicode(char *In, ssize_t &Len);
	bool GetOwnerUnderline();

	virtual void _Measure(int &x, int &y, OsChar *Str, int Len);
	virtual int _CharAt(int x, OsChar *Str, int Len, LPxToIndexType Type);
	virtual void _Draw(LSurface *pDC, int x, int y, OsChar *Str, int Len, LRect *r, LColour &fore);

public:
	/// Construct from face/pt size.
	LFont
	(
		/// Font face name
		const char *face = 0,
		/// Size of the font
		LCss::Len size = LCss::LenInherit
	);
	/// Construct from OS font handle
	LFont(OsFont Handle);
	/// Construct from type information
	LFont(LFontType &Type);
	/// Copy constructor
	LFont(LFont &Fnt);
	virtual ~LFont();

	/// Creates a new font handle with the specified face / pt size
	virtual bool Create
	(
		/// The new font face
		const char *Face = 0,
		/// The pt size
		LCss::Len Size = LCss::LenInherit,
		/// Creating a font for a particular surface (e.g. printing).
		LSurface *pSurface = 0
	);

	/// Creates a new font from type infomation
	bool Create(LFontType *Type, LSurface *pSurface = NULL);

	/// Creates the font from CSS defs.
	bool CreateFromCss(const char *Css);
	
	/// Creates the font from a CSS object.
	bool CreateFromCss(LCss *Css);

	/// Returns CSS styles to describe font
	LString FontToCss();

	/// Clears any handles and memory associated with the object.
	virtual bool Destroy();
	void WarnOnDelete(bool w);

	/// Returns the OS font handle
	virtual OsFont Handle();

	/// Copies the font
	virtual LFont &operator =(const LFont &f);

	/// Returns the pixel height of the font
	virtual int GetHeight();

	/// Gets the creation parameter passed in (0 by default).
	LSurface *GetSurface();

	/// Get supported glyphs
	uchar *GetGlyphMap();

	/// Converts printable characters to unicode.
	LAutoString ConvertToUnicode(char16 *Input, ssize_t Len = -1);
	
	#if USE_CORETEXT
	CFDictionaryRef GetAttributes();
	#endif
};

/// Font type information and system font query tools.
class LgiClass LFontType
{
	friend class LFont;
	friend class LTypeFace;

protected:
	#if WINNATIVE
	LOGFONTW Info;
	#else
	LTypeFace Info;
	#endif
	LString Buf;

public:
	LFontType(const char *face = 0, int pointsize = 0);
	virtual ~LFontType();

	#ifdef WINNATIVE
	LOGFONTW *Handle() { return &Info; }
	#else
	LTypeFace *Handle() { return &Info; }
	#endif

	/// Gets the type face name
	const char *GetFace();
	
	/// Sets the type face name
	void SetFace(const char *Face);
	
	/// Sets the point size
	int GetPointSize();
	
	/// Sets the point size
	void SetPointSize(int PointSize);

	/// Put a user interface for the user to edit the font def
	void DoUI(LView *Parent, std::function<void(LFontType*)> Callback);
	
	/// Describe the font to the user as a string
	bool GetDescription(char *Str, int SLen);
	
	/// Read/Write the font def to storage
	// bool Serialize(ObjProperties *Options, char *OptName, bool Write);
	/// Read/Write the font def to storage
	bool Serialize(LDom *Options, const char *OptName, bool Write);

	/// Read the font from the LGI config
	bool GetConfigFont(const char *Tag);
	
	/// Read the font from LGI config (if there) and then the system settings
	bool GetSystemFont(const char *Which);

	/// Read from OS reference
	bool GetFromRef(OsFont Handle);

	/// Create a font based on this font def
	virtual LFont *Create(LSurface *pSurface = NULL);
};

#ifndef LGI_STATIC
/// Overall font system class
class LgiClass LFontSystem : public LCapabilityClient
{
	friend class LApp;
	friend class LDisplayString;

	static LFontSystem *Me;

private:
	// System Font List
	LString::Array AllFonts;
	LString::Array SubFonts; // Fonts yet to be scanned for substitution

	// Missing Glyph Substitution
	uchar Lut[MAX_UNICODE+1];
	LFont *Font[256];
	class LFontSystemPrivate *d;

public:
	/// Get a pointer to the font system.
	static LFontSystem *Inst();

	// Object
	LFontSystem();
	~LFontSystem();

	/// Enumerate all installed fonts
	bool EnumerateFonts(LString::Array &Fonts);

	/// Returns whether the current Lgi implementation supports glyph sub
	bool GetGlyphSubSupport();
	/// Returns whether glyph sub is currently turned on
	bool GetDefaultGlyphSub();
	/// Turns the glyph sub feature on or off
	void SetDefaultGlyphSub(bool i);
	/// Returns a font that can render the specified unicode code point
	LFont *GetGlyph
	(
		/// A utf-32 character
		uint32_t u,
		/// The base font used for rendering
		LFont *UserFont
	);
	/// This looks for a font that can contains the most glyphs for a given string,
	/// ideally it can render the whole thing. But the next best alternative is returned
	/// when no font matches all characters in the string.
	LFont *GetBestFont(char *Str);
	/// Add a custom font to the glyph lookup table
	bool AddFont(LAutoPtr<LFont> Fnt);

	#ifdef __GTK_H__
	
	// Pango stuff
	Gtk::PangoFontMap *GetFontMap();
	Gtk::PangoContext *GetContext();
	
	#endif

	void ResetLibCheck();
	/// \returns true if iconv services are available.
	bool HasIconv(bool Quiet);
	/// Converts a string into a buffer using iconv
	ssize_t IconvConvert(const char *OutCs, char *Out, ssize_t OutLen, const char *InCs, const char *&In, ssize_t InLen);
	/// Converts a string into a stream using iconv
	ssize_t IconvConvert(const char *OutCs, LStreamI *Out, const char *InCs, const char *&In, ssize_t InLen);
};
#endif

#ifdef LINUX
extern bool _GetSystemFont(char *FontType, char *Font, int FontBufSize, int &PointSize);
#endif


#endif
