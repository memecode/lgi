#include "lgi/common/Lgi.h"
#include "lgi/common/Html2.h"
#include "lgi/common/MemDC.h"
#include "../src/common/Text/HtmlPriv2.h"

using namespace Html2;

class DumpHtml : public LHtml
{
public:
	DumpHtml() : LHtml(100, 0, 0, 800, 600) {}

	void FindText(LTag *t, const char *needle)
	{
		if (!t)
			return;
		if (t->GetText())
		{
			LAutoString u(WideToUtf8(t->GetText()));
			if (u && strstr(u, needle))
				printf("FOUND '%s' TextPos.Length()=%u Pos=%i,%i Size=%i,%i\n",
					u.Get(), (unsigned)t->TextPos.Length(), t->Pos.x, t->Pos.y, t->Size.x, t->Size.y);
		}
		for (unsigned i = 0; i < t->Children.Length(); i++)
			FindText(dynamic_cast<LTag*>(t->Children[i]), needle);
	}

	void Run(const char *path)
	{
		LFile f;
		if (!f.Open(path, O_READ))
		{
			printf("Can't open %s\n", path);
			return;
		}
		auto len = f.GetSize();
		LAutoString buf(new char[len + 1]);
		f.Read(buf, (int)len);
		buf[len] = 0;

		Name(buf);
		SetPos(LRect(0, 0, 799, 599));
		Layout(true);

		FindText(Tag, "Thank you");

		LMemDC dc;
		if (dc.Create(800, 600, System32BitColourSpace))
		{
			OnPaint(&dc);
		}
		else
		{
			printf("Failed to create MemDC\n");
		}
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, "DumpTree");
	if (a.IsOk() && AppArgs.Args > 1)
	{
		DumpHtml *h = new DumpHtml();
		h->Run(AppArgs.Arg[1]);
	}
	else
	{
		printf("Usage: DumpTree <file.html>\n");
	}
	return 0;
}
