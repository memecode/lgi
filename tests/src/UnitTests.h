#ifndef _UNIT_TESTS_H_
#define _UNIT_TESTS_H_

class UnitTest : public LBase
{
public:
	UnitTest(const char *name)
	{
		Name(name);
	}
	virtual ~UnitTest() {}

	virtual bool Run() = 0;

	bool FAIL(char *File, int Line, char *Msg)
	{
		LAssert(0);
		printf("%s:%i - Error in %s: %s\n", File, Line, Name(), Msg);
		return false;
	}
};

class LAutoPtrTest : public UnitTest
{
	class LAutoPtrTestPriv *d;

public:
	LAutoPtrTest();
	~LAutoPtrTest();

	bool Run();
};

class LCssTest : public UnitTest
{
	class LCssTestPriv *d;

public:
	LCssTest();
	~LCssTest();

	bool Run();
};

class LMatrixTest : public UnitTest
{
	class LMatrixTestPriv *d;

public:
	LMatrixTest();
	~LMatrixTest();

	bool Run();
};

class LContainers : public UnitTest
{
	class LContainersPriv *d;

public:
	LContainers();
	~LContainers();

	bool Run();
};

class LStringClassTest : public UnitTest
{
	class LStringClassTestPriv *d;

public:
	LStringClassTest();
	~LStringClassTest();

	bool Run();
};

class LStringPipeTest : public UnitTest
{
	class LStringPipeTestPriv *d;

public:
	LStringPipeTest();
	~LStringPipeTest();

	bool Run();
};

class LDateTimeTest : public UnitTest
{
public:
	LDateTimeTest() : UnitTest("LDateTimeTest") {}
	~LDateTimeTest() {}

	bool Run()
	{
		#ifdef _DEBUG
		return LDateTime_Test();
		#else
		#error "Requires _DEBUG"
		#endif
	}
};

class LRangeTest : public UnitTest
{
public:
	LRangeTest() : UnitTest("LRangeTest") {}
	~LRangeTest() {}

	bool Run();
};

class JsonTest : public UnitTest
{
public:
	JsonTest();
	~JsonTest();

	bool Run();
};

class LBitsTest : public UnitTest
{
public:
	LBitsTest() : UnitTest("LBitsTest") {}
	~LBitsTest() {}

	bool Run();
};

#endif