#include "lgi/common/Lgi.h"
#include "UnitTests.h"
#include "lgi/common/StringClass.h"

#ifdef LGI_UNIT_TESTS
int32 LString::RefStrCount = 0;
#endif

class LStringClassTestPriv
{
public:
};

LStringClassTest::LStringClassTest() : UnitTest("LStringClassTest")
{
	d = new LStringClassTestPriv;
}

LStringClassTest::~LStringClassTest()
{
	DeleteObj(d);
}

bool LStringClassTest::Run()
{
	// Constructor test
	{
		LString a("test");
		LString b = a;
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Assignment operator test
	{
		LString a("test");
		LString b;
		b = a;
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Split/Join tests
	{
		const char *Base = "123,456,789";
		LString a(Base);
		if (LString::RefStrCount != 1) return FAIL(_FL, "RefStrCount mismatch");
		LString::Array b = a.Split(",");
		if (LString::RefStrCount != 4) return FAIL(_FL, "RefStrCount mismatch");
		LString sep("-");
		if (LString::RefStrCount != 5) return FAIL(_FL, "RefStrCount mismatch");
		LString c = sep.Join(b);
		if (LString::RefStrCount != 6) return FAIL(_FL, "RefStrCount mismatch");
		if (c.Get() && stricmp(c.Get(), "123-456-789"))
			return FAIL(_FL, "Joined string incorrect");
		if (b[1].Int() != 456)
			return FAIL(_FL, "Int cast failure");

		LString d(",,123,345,,");
		b = d.SplitDelimit(",", -1, false);
		if (b.Length() != 6)
			return FAIL(_FL, "Wrong token count");

		b = d.SplitDelimit(",", -1, true);
		if (b.Length() != 2)
			return FAIL(_FL, "Wrong token count");

		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// C string assignment / cast testing
	{
		const char *cstr = "Test";
		LString a;
		a = cstr;
		if (_stricmp(cstr, a))
			return FAIL(_FL, "C String assignment failed");
		if (strcmp(cstr, a))
			return FAIL(_FL, "Original string modified");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Upper / lower case conversion
	{
		const char *cstr = "Test";
		LString a = cstr, b;
		b = a.Lower();
		if (strcmp(a, "Test") != 0 ||
			strcmp(b, "test"))
			return FAIL(_FL, "Lower failed");

		b = a.Upper();
		if (strcmp(a, "Test") != 0 ||
			strcmp(b, "TEST"))
			return FAIL(_FL, "Upper failed");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Find testing
	{
		LString a("This is a test string");
		LString b; // Empty
		if (a.Find("asdasdwer") >= 0)
			return FAIL(_FL, "Find failed");
		if (a.Find(NULL) >= 0)
			return FAIL(_FL, "Find failed");
		if (b.Find(NULL) >= 0)
			return FAIL(_FL, "Find failed");
		if (b.Find("Asdwegewr") >= 0)
			return FAIL(_FL, "Find failed");
		if (a.Find("This") != 0)
			return FAIL(_FL, "Find failed");
		if (a.Find("test") != 10)
			return FAIL(_FL, "Find failed");
		if (a.Find("string") != 15)
			return FAIL(_FL, "Find failed");
		if (a.Find("is") != 2)
			return FAIL(_FL, "Find failed");
		if (a.RFind("is") != 5)
			return FAIL(_FL, "RFind failed");
		if (a.RFind("string") != 15)
			return FAIL(_FL, "RFind failed");
		if (a.RFind(NULL) >= 0)
			return FAIL(_FL, "RFind failed");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Substring testing
	{
		const char *Base = "This is a test string";
		LString a(Base);
		int b;
		b = a(5);
		if (b != 'i')
			return FAIL(_FL, "Char substring failed");
		b = a(-1);
		if (b != 'g')
			return FAIL(_FL, "Char substring failed");
		LString c;
		c = a(5, 7);
		if (_stricmp(c, "is"))
			return FAIL(_FL, "Range substring failed");
		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Whitespace strip testing
	{
		const char *Base = "  \ta test. \t\r\n";
		LString a(Base);
		if (_stricmp(a.Strip(), "a test."))
			return FAIL(_FL, "Strip whitespace failed.");
		if (_stricmp(a.LStrip(), "a test. \t\r\n"))
			return FAIL(_FL, "Strip whitespace failed.");
		if (_stricmp(a.RStrip(), "  \ta test."))
			return FAIL(_FL, "Strip whitespace failed.");
		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Concatenation tests
	{
		const char *Base1 = "This is the first part. ";
		const char *Base2 = "This is the 2nd part.";
		LString a(Base1);
		LString b(Base2);
		LString c;
		c = a + b;
		if (strcmp(c, "This is the first part. This is the 2nd part."))
			return FAIL(_FL, "Concat string incorrect");

		LString d(Base1);
		d += b;
		if (strcmp(d, "This is the first part. This is the 2nd part."))
			return FAIL(_FL, "Concat string incorrect");
		
		if (strcmp(Base1, a))
			return FAIL(_FL, "Original string modified");
		if (strcmp(Base2, b))
			return FAIL(_FL, "Original string modified");
	}
	if (LString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	return true;
}
