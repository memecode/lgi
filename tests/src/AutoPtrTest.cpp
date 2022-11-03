#include "lgi/common/Lgi.h"
#include "UnitTests.h"

class LAutoPtrTestPriv
{
};

LAutoPtrTest::LAutoPtrTest() : UnitTest("LAutoPtrTest")
{
}

LAutoPtrTest::~LAutoPtrTest()
{
}

LAutoString CreateAutoString(char **Ptr)
{
	return LAutoString(*Ptr = NewStr("Something"));
}

bool LAutoPtrTest::Run()
{
	char *Ptr;
	LAutoString s1(Ptr = NewStr("string1"));
	LAutoString s2 = s1;
	if (s1.Get() != 0)
	{
		return FAIL(_FL, "Initial auto ptr should be zero now.");
	}
	if (s2.Get() != Ptr)
	{
		return FAIL(_FL, "Ptrs should be equal.");
	}

	char *Ptr2 = 0;
	LAutoString s3 = CreateAutoString(&Ptr2);
	if (s3.Get() != Ptr2)
	{
		return FAIL(_FL, "Ptr not the same.");
	}
	if (strnicmp(s3, "Something", 9))
	{
		return FAIL(_FL, "Ptr contents are wrong.");
	}

	return true;
}

