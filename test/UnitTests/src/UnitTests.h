#pragma once

class UnitTest : public LBase
{
public:
	UnitTest(const char *name)
	{
		Name(name);
	}
	virtual ~UnitTest() {}

	virtual bool Run() = 0;

	bool FAIL(const char *File, int Line, const char *Msg)
	{
		LAssert(0);
		printf("%s:%i - Error in %s: %s\n", File, Line, Name(), Msg);
		return false;
	}
};

#define DECL_TEST(name) \
	class name : public UnitTest \
	{ \
		class Priv##name *d; \
	public: \
		name(); \
		~name(); \
		bool Run(); \
	}


DECL_TEST(LAutoPtrTest);
DECL_TEST(LCssTest);
DECL_TEST(LMatrixTest);
DECL_TEST(LContainers);
DECL_TEST(LStringClassTest);
DECL_TEST(LStringTests);
DECL_TEST(LRangeTest);
DECL_TEST(JsonTest);
DECL_TEST(LBitsTest);
DECL_TEST(NetworkTests);
DECL_TEST(XmlTest);

class LDateTimeTest : public UnitTest
{
public:
	LDateTimeTest() : UnitTest("LDateTimeTest") {}
	
	bool Run()
	{
		#ifdef _DEBUG
		return LDateTime::UnitTests();
		#else
		#error "Requires _DEBUG"
		#endif
	}
};