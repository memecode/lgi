#pragma once

#include <functional>

/// Charset definitions
enum LCharSetType
{
	CpNone,
	CpMapped,
	CpUtf8,
	CpUtf16,
	CpUtf32,
	CpIconv,
	CpWindowsDb
};

/// Charset information class
class LgiClass LCharset
{
public:
	/// Standard charset name
	const char *Charset;
	/// Human description
	const char *Description;
	/// 128 shorts that map the 0x80-0xff range to unicode
	short *UnicodeMap;
	/// Charset's name for calling iconv
	const char *IconvName;
	/// Comma separated list of alternate names used for this charset
	const char *AlternateNames;
	/// General type of the charset
	LCharSetType Type;

	/// Constructor
	LCharset(const char *cp = 0, const char *des = 0, short *map = 0, const char *alt = 0);

	/// Returns true if the charset is a unicode variant
	bool IsUnicode();
	/// Gets the iconv name
	const char *GetIconvName();
	/// Returns whether Lgi can convert to/from this charset at the moment.
	bool IsAvailable();
};

/// Charset table manager class
class LgiClass LCharsetSystem
{
	struct LCharsetSystemPriv *d;

public:
	static LCharsetSystem *Inst();

	LCharsetSystem();
	~LCharsetSystem();

	// Hooks
	std::function<LString(LString)> DetectCharset;

	// Get the charset info
	LCharset *GetCsInfo(const char *Cp);
	LCharset *GetCsList();
};

/// Returns information about a charset.
LgiFunc LCharset *LGetCsInfo(const char *Cs);

/// Returns the start of an array of supported charsets, terminated by
/// one with a NULL 'Charset' member. 
LgiFunc LCharset *LGetCsList();

/// Returns the charset that best fits the input data
LgiFunc const char *LUnicodeToCharset
(
	/// The input text
	const char *Utf8,
	/// The byte length of the input text
	ssize_t Len = -1,
	/// An optional list of preferred charsets to look through first
	List<char> *Prefs = 0
);

