#include "lgi/common/Lgi.h"
#include "lgi/common/TextConvert.h"

#include "UnitTests.h"

class PrivLStringTests
{
public:
};

LStringTests::LStringTests() : UnitTest("LStringTests")
{
	d = new PrivLStringTests;
}

LStringTests::~LStringTests()
{
	DeleteObj(d);
}

bool LStringTests::Run()
{
	/* Things to test:

	bool Is8Bit(const char *Text);

	[[deprecated]] char *DecodeBase64Str(char *Str, ssize_t Len = -1);
	LString LDecodeBase64Str(LString Str);

	[[deprecated]] char *DecodeQuotedPrintableStr(char *Str, ssize_t Len = -1);
	LString LDecodeQuotedPrintableStr(LString Str);

	[[deprecated]] char *DecodeRfc2047(char *Str);
	LString LDecodeRfc2047(LString Str);

	LString LEncodeRfc2047(LString Str, const char *Charset, List<char> *CharsetPrefs, ssize_t LineLength = 0);

	*/

	const char *EncodeResult1 = "=?iso-8859-1?Q?Beytullah_Gen=E7?=";
	const char *EncodeResult2 = "=?iso-8859-9?Q?Beytullah_Gen=E7?=";

	LString::Array CharsetPrefs;
	const char *Rfc2047Input = "Beytullah Genç";
	const char *UtfInput = "Beytullah GenÃ§";
	const char *Charset = "windows-1252";

	// No prefered charset testing:
	LAutoString result1( EncodeRfc2047(NewStr(Rfc2047Input), Charset) );
	if (Stricmp(result1.Get(), EncodeResult1))
		return FAIL(_FL, "EncodeRfc2047");
	LAutoString decode1( DecodeRfc2047(NewStr(result1)) );
	if (Strcmp(UtfInput, decode1.Get()))
		return FAIL(_FL, "DecodeRfc2047");

	LString   result2 = LEncodeRfc2047(Rfc2047Input, Charset);
	if (Stricmp(result2.Get(), EncodeResult1))
		return FAIL(_FL, "LEncodeRfc2047");
	LAutoString decode2( DecodeRfc2047(NewStr(result2)) );
	if (Strcmp(UtfInput, decode2.Get()))
		return FAIL(_FL, "DecodeRfc2047");

	// Redo tests with a charset preference set:
	CharsetPrefs.Add("iso-8859-9");
	LAutoString result3( EncodeRfc2047(NewStr(Rfc2047Input), Charset, &CharsetPrefs));
	if (Stricmp(result3.Get(), EncodeResult2))
		return FAIL(_FL, "EncodeRfc2047");
	LString     result4 = LEncodeRfc2047(Rfc2047Input, Charset, &CharsetPrefs);
	if (Stricmp(result4.Get(), EncodeResult2))
		return FAIL(_FL, "EncodeRfc2047");


	return true;
}


