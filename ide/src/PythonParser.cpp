/*
Known bugs:


*/
#include "lgi/common/Lgi.h"
#include "ParserCommon.h"

#if 0
// #define DEBUG_FILE		"\\ape-apcp.c"
#define DEBUG_FILE		"apcp\\apcp\\apcp.h"
#define DEBUG_LINE		550
#endif

bool BuildPyDefnList(const char *FileName, char16 *Source, LArray<DefnInfo> &Defns, int LimitTo, bool Debug)
{
	if (!Source)
		return false;

	LString Src = Source;
	LString::Array Lines = Src.SplitDelimit("\n", -1, false);
	int Depth = 0;
	int Line = 1;
	LString ClsName;

	for (auto Ln : Lines)
	{
		if (Depth && Ln)
		{
			const char *s = Ln, *e = s;
			while (iswhite(*e))
				e++;
			if ((e - s) < Depth && *e && *e != '#')
				Depth = 0;
		}

		if (Ln.Find("def ") == Depth)
		{
			if (ClsName)
			{
				LString::Array a = Ln.Strip().SplitDelimit(" \t", 1);
				LAssert(a.Length() == 2);
				LString Fn = a[0] + " " + ClsName + "::" + a[1];
				Defns.New().Set(DefnFunc, FileName, Fn, Line);
			}
			else
			{
				Defns.New().Set(DefnFunc, FileName, Ln, Line);
			}
		}
		else if (Ln.Find("class ") == Depth)
		{
			LString::Array a = Ln.SplitDelimit(" \t(");
			LAssert(a.Length() >= 2);
			ClsName = a[1];
			auto ClsDesc = a[0] + " " + a[1];
			Defns.New().Set(DefnClass, FileName, ClsDesc, Line);
			Depth = 4;
		}
		Line++;
	}

	return Defns.Length() > 0;
}

