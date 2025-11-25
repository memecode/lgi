#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"

///////////////////////////////////////////////////////////////////////////////////////////////
LDragFormats::LDragFormats(bool source)
{
	Source = source;
}

LString LDragFormats::ToString()
{
	LString::Array a;
	for (auto &f: Formats)
		a.Add(f.Get());
	return LString(",").Join(a);
}

void LDragFormats::SupportsFileDrops()
{
	#ifdef MAC
	Supports("NSFilenamesPboardType");
	#endif
	Supports(LGI_FileDropFormat);
}

void LDragFormats::SupportsFileStreams()
{
	Supports(LGI_StreamDropFormat);
	#ifdef WINDOWS
	Supports(CFSTR_FILECONTENTS);
	#endif
}

bool LDragFormats::HasFormat(const char *Fmt)
{
	for (auto f: Formats)
		if (f.Equals(Fmt))
			return true;
	return false;
}

void LDragFormats::Supports(LString Fmt)
{
	auto debug = false; // Fmt.Equals("application/x-scribe-thing-list");

	if (Source)
	{
		// Drag sources just add the formats to the list
		if (!HasFormat(Fmt))
		{
			Formats.New().Set(Fmt);
			if (debug)
				DND_LOG("%s:%i - source Supports(%s) added.\n", _FL, Fmt.Get());
		}
	}
	else
	{
		// Drag targets mark the supported formats
		bool found = false;
		for (auto &f: Formats)
		{
			if (f.Equals(Fmt))
			{
				f.Val = found = true;
				break;
			}
		}
		
		if (debug)
			DND_LOG("%s:%i - target Supports(%s)=%i.\n", _FL, Fmt.Get(), found);
	}
}

LString::Array LDragFormats::GetAll()
{
	LString::Array a;
	a.SetFixedLength(false);
	for (auto &f: Formats)
		a.Add(f);
	return a;
}

LString::Array LDragFormats::GetSupported()
{
	LString::Array a;
	a.SetFixedLength(false);
	for (auto &f: Formats)
		if (f.Val)
			a.Add(f);
	return a;
}

