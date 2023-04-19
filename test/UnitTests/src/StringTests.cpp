#include "lgi/common/Lgi.h"
#include "lgi/common/TextConvert.h"

#include "UnitTests.h"

class PrivLStringTests
{
public:
};

LStringTests::LStringTests() : UnitTest("LStringPipeTest")
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

	const char *EncodeResult1 = "=?utf-8?B?QmV5dHVsbGFoIEdlbsOn?=";
	const char *EncodeResult2 = "=?iso-8859-9?Q?Beytullah_Gen=E7?=";

	List<char> CharsetPrefs;
	const char *Rfc2047Input = "Beytullah Genç";
	const char *Charset = "windows-1252";

	#if 0
	LAutoString result1( EncodeRfc2047(NewStr(Rfc2047Input), Charset, &CharsetPrefs));
	if (Stricmp(result1.Get(), EncodeResult1))
		return FAIL(_FL, "EncodeRfc2047");
	LString   result2 = LEncodeRfc2047(Rfc2047Input, Charset, &CharsetPrefs);
	if (Stricmp(result2.Get(), EncodeResult1))
		return FAIL(_FL, "LEncodeRfc2047");
	#endif

	CharsetPrefs.Add("iso-8859-9");

	LAutoString result3( EncodeRfc2047(NewStr(Rfc2047Input), Charset, &CharsetPrefs));
	LString   result4 = LEncodeRfc2047(Rfc2047Input, Charset, &CharsetPrefs);


	return true;
}


