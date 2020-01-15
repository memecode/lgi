#include "Lgi.h"
#include "GDragAndDrop.h"

///////////////////////////////////////////////////////////////////////////////////////////////
GDragFormats::GDragFormats(bool source)
{
	Source = source;
}

GString GDragFormats::ToString()
{
	GStringPipe p(256);
	p.Print("{");
	for (auto &f: Formats)
		p.Print("%s,", f.Get());
	p.Print("}");
	return p.NewGStr();
}

void GDragFormats::SupportsFileDrops()
{
	#ifdef MAC
	Supports("NSFilenamesPboardType");
	#endif
	Supports(LGI_FileDropFormat);
}

void GDragFormats::SupportsFileStreams()
{
	Supports(LGI_StreamDropFormat);
}

bool GDragFormats::HasFormat(const char *Fmt)
{
	for (auto f: Formats)
		if (f.Equals(Fmt))
			return true;
	return false;
}

void GDragFormats::Supports(GString Fmt)
{
	if (Source)
	{
		// Drag sources just add the formats to the list
		if (!HasFormat(Fmt))
			Formats.New().Set(Fmt);
	}
	else
	{
		// Drag targets mark the supported formats
		for (auto &f: Formats)
		{
			if (f.Equals(Fmt))
				f.Val = true;
		}
	}
}

GString::Array GDragFormats::GetSupported()
{
	GString::Array a;
	a.SetFixedLength(false);
	for (auto &f: Formats)
		if (f.Val)
			a.Add(f);
	return a;
}

