#include "lunasvg/lunasvg.h"
#include "lgi/common/Lgi.h"

class SvgRender : public LFilter
{
public:
	const char *GetClass() override { return "SvgRender"; }
	Format GetFormat() override { return FmtSvg; }

	IoStatus ReadImage(LSurface *Out, LStream *In) override
	{
		if (!Out || !In)
		{
			LgiTrace("%s:%i - param err.\n", _FL);
			return IoError;
		}
			
		auto sz = In->GetSize();
		if (sz <= 0)
		{
			LgiTrace("%s:%i - empty/invalid stream (sz=%i).\n", _FL, (int)sz);
			return IoError;
		}

		LString str;
		str.Length(sz);
		auto rd = In->Read(str.Get(), str.Length());
		if (rd != (ssize_t)str.Length())
		{
			LgiTrace("%s:%i - short read (%i of %i).\n", _FL, (int)rd, (int)str.Length());
			return IoError;
		}

		if (auto doc = lunasvg::Document::loadFromData(str.Get(), str.Length()))
		{
			int sx = -1, sy = -1;
			if (Props)
			{
				LVariant req;
				if (Props->GetValue(LGI_FILTER_SIZE_REQ, req))
				{
					auto parts = LString(req.Str()).SplitDelimit(",");
					if (parts.Length() == 2)
					{
						sx = (int)parts[0].Int();
						sy = (int)parts[1].Int();						
					}
				}
			}
		
			// LgiTrace("%s:%i - sz=%i,%i.\n", _FL, sx, sy);
		    auto bmp = doc->renderToBitmap(sx, sy);
			// LgiTrace("%s:%i - bmp=%i,%i.\n", _FL, bmp.width(), bmp.height());
			
			if (Out->Create(bmp.width(), bmp.height(), System32BitColourSpace))
			{
				#ifndef MAC
				// GetRowStep() can be negative for bottom-up surfaces, so take the
				// magnitude before using it as a memcpy length.
				auto rowStep = Out->GetRowStep();
				auto bytes = MIN((ssize_t)bmp.stride(), rowStep < 0 ? -rowStep : rowStep);
				#endif
				// LgiTrace("%s:%i - strides: %i, %i\n", _FL, bmp.stride(), Out->GetRowStep());
				for (int y=0; y<Out->Y(); y++)
				{
					auto in = bmp.data() + (y * bmp.stride());
					if (auto out = (*Out)[y])
					{
						#ifdef MAC
							auto i = (LBgra32*)in;
							auto o = (System32BitPixel*)out;
							auto e = i + Out->X();
							while (i < e)
							{
								o->r = i->r; o->g = i->g; o->b = i->b; o->a = i->a;
								o++; i++;
							}
						#else
							memcpy(out, in, bytes);
						#endif
					}
				}
			}
		}
		else
		{
			printf("%s:%i - loadFromData failed:\n%s\n", _FL, str.Get());
			return IoError;
		}
		
		return IoSuccess;
	}
	
	IoStatus WriteImage(LStream *Out, LSurface *In) override
	{
		return IoUnsupportedFormat;
	}
};

struct LSvgFactory : public LFilterFactory
{
	LSvgFactory() : LFilterFactory("LSvgFactory") {}

	bool CheckFile(const char *File, int Access, const uchar *Hint)
	{
		auto ext = LGetExtension(File);
		return !Stricmp(ext, "svg");
	}

	LFilter *NewObject()
	{
		return new SvgRender;
	}
	
}	SvgFactory;
