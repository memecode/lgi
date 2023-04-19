#include "lgi/common/Lgi.h"
#include "lgi/common/Net.h"
#include "UnitTests.h"

class PrivNetworkTests
{
public:
};

NetworkTests::NetworkTests() : UnitTest("NetworkTests")
{
	d = new PrivNetworkTests;
}

NetworkTests::~NetworkTests()
{
	DeleteObj(d);
}

bool NetworkTests::Run()
{
	/*
	
	Things to test:

	LgiFunc char *InetGetHeaderField(const char *Headers, const char *Field, ssize_t Len = -1);
	LgiExtern LString LGetHeaderField(LString Headers, const char *Field);
	LgiFunc char *InetGetSubField(const char *s, const char *Field);
	LgiExtern LString LGetSubField(LString HeaderValue, const char *Field);

	LUri
	
	
	*/

	

	return true;
}
