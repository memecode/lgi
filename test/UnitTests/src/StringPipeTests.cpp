#include "lgi/common/Lgi.h"
#include "UnitTests.h"

class LStringPipeTestPriv
{
public:
};

LStringPipeTest::LStringPipeTest() : UnitTest("LStringPipeTest")
{
	d = new LStringPipeTestPriv;
}

LStringPipeTest::~LStringPipeTest()
{
	DeleteObj(d);
}

bool LStringPipeTest::Run()
{


	return true;
}
