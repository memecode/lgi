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
	const uint8_t Rfc2047Input[] = { 0x42,0x65,0x79,0x74,0x75,0x6C,0x6C,0x61,0x68,0x20,0x47,0x65,0x6E, 0xE7, 0 }; // "Beytullah Genç" in windows-1252
	const uint8_t UtfInput[]     = { 0x42,0x65,0x79,0x74,0x75,0x6C,0x6C,0x61,0x68,0x20,0x47,0x65,0x6E, 0xC3,0xA7, 0 }; // "Beytullah Genç" in utf-8
	const char *Charset = "windows-1252";

	// No prefered charset testing:
	LAutoString result1( EncodeRfc2047(NewStr((char*)Rfc2047Input), Charset) );
	if (Stricmp(result1.Get(), EncodeResult1))
	{
		printf("result1='%s'\n", result1.Get());
		printf("EncodeResult1='%s'\n", EncodeResult1);
		return FAIL(_FL, "EncodeRfc2047");
	}
	LAutoString decode1( DecodeRfc2047(NewStr(result1)) );
	if (Strcmp((char*)UtfInput, decode1.Get()))
	{
		LString::Array a;
		for (uint8_t *c = (uint8_t*)decode1.Get(); *c; c++)
			a.New().Printf("%2.2x", *c);
		printf("decode1=%s\n", LString(",").Join(a).Get());
		a.Empty();
		for (uint8_t *c = (uint8_t*)UtfInput; *c; c++)
			a.New().Printf("%2.2x", *c);
		printf("UtfInput=%s\n", LString(",").Join(a).Get());

		return FAIL(_FL, "DecodeRfc2047");
	}

	LString   result2 = LEncodeRfc2047((char*)Rfc2047Input, Charset);
	if (Stricmp(result2.Get(), EncodeResult1))
		return FAIL(_FL, "LEncodeRfc2047");
	LAutoString decode2( DecodeRfc2047(NewStr(result2)) );
	if (Strcmp((char*)UtfInput, decode2.Get()))
		return FAIL(_FL, "DecodeRfc2047");

	// Redo tests with a charset preference set:
	CharsetPrefs.Add("iso-8859-9");
	LAutoString result3( EncodeRfc2047(NewStr(Rfc2047Input), Charset, &CharsetPrefs));
	if (Stricmp(result3.Get(), EncodeResult2))
		return FAIL(_FL, "EncodeRfc2047");
	LString     result4 = LEncodeRfc2047((char*)Rfc2047Input, Charset, &CharsetPrefs);
	if (Stricmp(result4.Get(), EncodeResult2))
		return FAIL(_FL, "EncodeRfc2047");

	// Quoted printable decode test:
	auto input5 = "=?UTF-8?q?=D0=92=D0=B0=D0=BC_=D0=BF=D1=80=D0=B8=D1=88=D0=BB=D0=BE_=D0=BD?= =?UTF-8?q?=D0=BE=D0=B2=D0=BE=D0=B5_=D1=81=D0=BE=D0=BE=D0=B1=D1=89=D0=B5?= =?UTF-8?q?=D0=BD=D0=B8=D0=B5?=";
	auto result5 = LDecodeRfc2047(input5);
	auto encodeResult5 = L"Вам пришло новое сообщение";
	LAutoWString decode5( Utf8ToWide(result5) );
	if (Stricmp(encodeResult5, decode5.Get()))
		return FAIL(_FL, "LDecodeRfc2047");

	// Mixing encoded and plain words:
	auto input6 = "test =?UTF-8?q?=D0=BE=D0=B2=D0=BE=D0=B5?= of words =?UTF-8?q?=D0=BE=D0=B2=D0=BE=D0=B5?=";
	auto result6 = LDecodeRfc2047(input6);
	auto encodeResult6 = L"test овое of words овое";
	LAutoWString decode6( Utf8ToWide(result6) );
	if (Stricmp(encodeResult6, decode6.Get()))
		return FAIL(_FL, "LDecodeRfc2047");

	return true;
}


