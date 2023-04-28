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

	LString Headers =	"Subject: =?utf-8?Q?Hi=20Matthew=2C=C2=A0=20check=20out=20what=27s=20coming=20up=20at=20LifeSource?=\r\n"
						"Date: Thu, 27 Apr 2023 19:59:56 +0000\r\n"
						"Received: from localhost (localhost [127.0.0.1])\r\n"
						"	by mail145.atl271.mcdlv.net (Mailchimp) with ESMTP id 4Q6mmX4djgzDSH7tT\r\n"
						"	for <fret@memecode.com>; Thu, 27 Apr 2023 19:59:56 +0000 (GMT)\r\n"
						"Content-Type: multipart/alternative; boundary=\"_----------=_MCPart_2128447118\"";
	
	auto ExpectedReceived = "from localhost (localhost [127.0.0.1])\n"
							" by mail145.atl271.mcdlv.net (Mailchimp) with ESMTP id 4Q6mmX4djgzDSH7tT\n"
							" for <fret@memecode.com>; Thu, 27 Apr 2023 19:59:56 +0000 (GMT)";

	LAutoString h1(InetGetHeaderField(Headers, "Received"));
	// LgiTrace("h1='%s'\n", LString::Escape(h1).Get());
	if (Strcmp(h1.Get(), ExpectedReceived))
		return FAIL(_FL, "InetGetHeaderField returned wrong value.");

	auto h2 = LGetHeaderField(Headers, "Received");
	if (Strcmp(h1.Get(), ExpectedReceived))
		return FAIL(_FL, "LGetHeaderField returned wrong value.");

	auto ExpectedType = "multipart/alternative; boundary=\"_----------=_MCPart_2128447118\"";
	auto h3 = LGetHeaderField(Headers, "Content-Type");
	if (Strcmp(h3.Get(), ExpectedType))
		return FAIL(_FL, "LGetHeaderField returned wrong value.");

	auto ExpectedBoundary = "_----------=_MCPart_2128447118";
	LAutoString h4(InetGetSubField(h3, "boundary"));
	if (Strcmp(h4.Get(), ExpectedBoundary))
		return FAIL(_FL, "InetGetSubField returned wrong value.");

	auto h5 = LGetSubField(h3, "boundary");
	if (Strcmp(h5.Get(), ExpectedBoundary))
		return FAIL(_FL, "LGetSubField returned wrong value.");

	return true;
}
