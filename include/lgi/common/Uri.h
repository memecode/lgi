#pragma once

#include "lgi/common/StringClass.h"

/// Uri parser
class LgiClass LUri
{
public:
	LString sProtocol;
	LString sUser;
	LString sPass;
	LString sHost;
	int Port;
	LString sPath;
	LString sAnchor;

	/// Parser for URI's.
	LUri
	(
		/// Optional URI to start parsing
		const char *uri = 0
	);
	~LUri();

	bool IsProtocol(const char *p) { return sProtocol.Equals(p); }
	bool IsHttp() { return sProtocol.Equals("http") || sProtocol.Equals("https"); }
	bool IsFile() { return sProtocol.Equals("file"); }
	void SetFile(LString Path) { Empty(); sProtocol = "file"; sPath = Path; }
	const char *LocalPath();
	operator bool();

	/// Parse a URI into it's sub fields...
	bool Set(const char *uri);

	/// Re-constructs the URI
	LString ToString();

	/// Empty this object...
	void Empty();

	/// URL encode
	LString EncodeStr
	(
		/// The string to encode
		const char *s,
		/// [Optional] Any extra characters you want encoded
		const char *ExtraCharsToEncode = 0
	);

	/// URL decode
	LString DecodeStr(const char *s);

	/// Separate args into map
	typedef LHashTbl<StrKey<char,false>,LString> StrMap;
	StrMap Params();

	LUri &operator =(const LUri &u);
	LUri &operator =(const char *s) { Set(s); return *this; }
	LUri &operator +=(const char *s);
};

/// Proxy settings lookup
class LgiClass LProxyUri : public LUri
{
public:
	LProxyUri();
};