#include "lgi/common/Lgi.h"
#include "UnitTests.h"

void UnitTests()
{
	LArray<UnitTest*> Tests;

	Tests.Add(new LRangeTest);
	Tests.Add(new LContainers);
	Tests.Add(new LAutoPtrTest);
	Tests.Add(new LCssTest);
	Tests.Add(new LMatrixTest);
	Tests.Add(new LStringClassTest);
	Tests.Add(new LDateTimeTest);
	Tests.Add(new LStringTests);
	Tests.Add(new LBitsTest);
	Tests.Add(new JsonTest);
	Tests.Add(new NetworkTests);
	Tests.Add(new XmlTest);

	for (int i=0; i<Tests.Length(); i++)
	{
		bool b = Tests[i]->Run();
		if (b)
		{
			printf("Passed: %s\n", Tests[i]->Name());
		}
		else
		{
			printf("Error: %s test failed.\n", Tests[i]->Name());
			break;
		}
	}

	Tests.DeleteObjects();
}

int main(int args, const char **arg)
{
	OsAppArguments Args(args, arg);
	LApp a(Args, "LgiUnitTests");
	if (a.IsOk())
	{
		UnitTests();
	}
	LSleep(2000);
	return 0;
}