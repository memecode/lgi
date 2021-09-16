#include "lgi/common/Lgi.h"
#include "lgi/common/Json.h"
#include "UnitTests.h"

class JsonTestPriv
{
};

JsonTest::JsonTest() : UnitTest("JsonTest")
{
}

JsonTest::~JsonTest()
{
}

bool JsonTest::Run()
{
	LJson j1(
		"{\n"
		"	\"heos\":\n"
		"	{\n"
		"		\"command\": \"player/get_players\",\n"
		"			\"result\" : \"success\",\n"
		"			\"message\" : \"\"\n"
		"	},\n"
		"	\"payload\":\n"
		"	[\n"
		"	{\n"
		"		\"name\": \"HEOS Bar\",\n"
		"		\"pid\" : 1429658919,\n"
		"		\"model\" : \"HEOS Bar\",\n"
		"		\"version\" : \"2.0.86\",\n"
		"		\"ip\" : \"192.168.1.42\",\n"
		"		\"network\" : \"wired\",\n"
		"		\"lineout\" : 0\n"
		"	}\n"
		"	]\n"
		"}");
	auto a = j1.GetArray("payload");
	if (a.Length() != 1)
		return FAIL(_FL, "Array length wrong.");

	auto elem = *a.begin();
	if (!elem.Get("pid").Equals("1429658919"))
		return FAIL(_FL, "Value wrong.");

	LgiTrace("json=%s\n", j1.GetJson().Get());

	return true;
}

