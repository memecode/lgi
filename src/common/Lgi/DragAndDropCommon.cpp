#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"

///////////////////////////////////////////////////////////////////////////////////////////////
GDragFormats::GDragFormats(bool source)
{
	Source = source;
}

LString GDragFormats::ToString()
{
	LStringPipe p(256);
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

void GDragFormats::Supports(LString Fmt)
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

LString::Array GDragFormats::GetSupported()
{
	LString::Array a;
	a.SetFixedLength(false);
	for (auto &f: Formats)
		if (f.Val)
			a.Add(f);
	return a;
}

