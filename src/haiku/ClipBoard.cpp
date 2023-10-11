// Clipboard Implementation
#include "lgi/common/Lgi.h"
#include "lgi/common/Variant.h"
#include "lgi/common/ClipBoard.h"

#include <Clipboard.h>
#include <Path.h>

#define DEBUG_CLIPBOARD					0
#define LGI_CLIP_BINARY					"lgi.binary"

class LClipBoardPriv : public BClipboard
{
public:
	LString Txt;
	LAutoWString WTxt;
	
	LClipBoardPriv() : BClipboard(NULL)
	{
	}


	bool Text(const char *MimeType, const char *Str, bool AutoEmpty)
	{
		if (!Lock())
		{
			LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
			return false;
		}
		
		if (AutoEmpty)
		{
			Clear();
		}
	 
	    auto clip = Data();
	    if (!clip)
	    {
		    Unlock();
			LgiTrace("%s:%i - No clipboard data.\n", _FL);
			return false;
	    }

	    auto result = clip->AddString(MimeType, BString(Str));
	    if (result)
			printf("%s:%i - AddString=%i %s\n", _FL, result, strerror(result));
	 
	    result = Commit();
	    if (result)
			printf("%s:%i - Commit=%i %s\n", _FL, result, strerror(result));

	    Unlock();

		return result == B_OK;
	}	

	char *Text(const char *MimeType)
	{
		if (!Lock())
		{
			LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
			return NULL;
		}
		
	    auto clip = Data();
	 
		BString s;
		auto result = clip->FindString(MimeType, &s);
	    if (result)
	    {
			const void *data = NULL;
			ssize_t bytes;
			result = clip->FindData(MimeType, B_MIME_TYPE, &data, &bytes);
			if (result == B_OK)
			{
				Txt.Set((char*)data, bytes);
			}
			else
			{
				printf("%s:%i - FindString(%s)=%i %s\n", _FL, MimeType, result, strerror(result));
				clip->PrintToStream();
			}
		}
		else
		{		
			Txt = s;
		}
	 
	    Unlock();
			
		return Txt;
	}
	
};

///////////////////////////////////////////////////////////////////////////////////////////////
LClipBoard::LClipBoard(LView *o)
{
	d = new LClipBoardPriv;
	Owner = o;
	Open = false;

}

LClipBoard::~LClipBoard()
{
	DeleteObj(d);
}

LString LClipBoard::FmtToStr(FormatType Fmt)
{
	return Fmt;
}

LClipBoard::FormatType LClipBoard::StrToFmt(LString Str)
{
	return Str;
}

bool LClipBoard::Empty()
{
	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		return false;
	}

    auto result = d->Clear();
	if (result)
		printf("%s:%i - clear=%i %s\n", _FL, result, strerror(result));
    result = d->Commit();
	if (result)
		printf("%s:%i - commit=%i %s\n", _FL, result, strerror(result));
		
    d->Unlock();

	return true;
}

bool LClipBoard::EnumFormats(LArray<FormatType> &Formats)
{
	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		return false;
	}

	auto data = d->Data();
	if (data)
	{
		for (int32 i=0; i<data->CountNames(B_ANY_TYPE); i++)
		{
			char *name = NULL;
			type_code type;
			int32 count = 0;
			auto s = data->GetInfo(B_ANY_TYPE, i, &name, &type, &count);
			if (s == B_OK)
			{
				auto t = LgiSwap32(type);
				
				if (type == B_REF_TYPE)
				{
					entry_ref ref;
					auto r = data->FindRef(name, &ref);
					if (r == B_OK)
					{
						BEntry e(&ref);
						BPath p;
						e.GetPath(&p);
						printf("Ref: '%s' type=%4.4s, count=%i, path=%s\n", name, &t, count, p.Path());
					}
				}
				else if (type == B_INT32_TYPE)
				{
					int32 val = -1;
					auto r = data->FindInt32(name, &val);
					auto v = LgiSwap32(val);
					printf("Int32: '%s' type=%4.4s, count=%i val=%i/0x%x, '%4.4s'\n", name, &t, count, val, val, &v);
				}
				else
				{		
					printf("General: '%s' type=%4.4s, count=%i\n", name, &t, count);
				}
				
				if (name)
					Formats.Add(name);
			}
		}
	}

    d->Unlock();

	return Formats.Length() > 0;
}

bool LClipBoard::Html(const char *doc, bool AutoEmpty)
{
	return d->Text("text/html", doc, AutoEmpty);
}

LString LClipBoard::Html()
{
	return d->Text("text/plain");
}

bool LClipBoard::Text(const char *Str, bool AutoEmpty)
{
	return d->Text("text/plain", Str, AutoEmpty);
}

char *LClipBoard::Text()
{
	return d->Text("text/plain");
}

// Wide char versions for plain text
bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	LAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *LClipBoard::TextW()
{
	auto u = Text();
	d->WTxt.Reset(Utf8ToWide(u));
	return d->WTxt;
}

bool LClipBoard::Bitmap(LSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	return Status;
}

void LClipBoard::Bitmap(BitmapCb Callback)
{
}

void LClipBoard::Files(FilesCb Callback)
{
	if (!Callback)
		return;
	
	LString::Array files;

	if (!d->Lock())
	{
		LgiTrace("%s:%i - Can't lock BClipboard.\n", _FL);
		Callback(files, "Can't lock clipboard.");
		return;
	}

	auto data = d->Data();
	if (!data)
	{
		d->Unlock();
		Callback(files, "No data on clipboard.");
		return;
	}
	
	for (int32 i=0; i<data->CountNames(B_ANY_TYPE); i++)
	{
		char *name = NULL;
		type_code type;
		int32 count = 0;
		auto s = data->GetInfo(B_ANY_TYPE, i, &name, &type, &count);
		if (s == B_OK && type == B_REF_TYPE)
		{
			entry_ref ref;
			auto r = data->FindRef(name, &ref);
			if (r != B_OK)
				continue;

			BEntry e(&ref);
			BPath p;
			if (e.GetPath(&p) != B_OK)
				continue;
			
			files.Add(p.Path());
		}
	}

    d->Unlock();

	Callback(files, LString());
}

bool LClipBoard::Files(::LString::Array &a, bool AutoEmpty)
{
	LAssert(!"impl me.");
	return false;
}

bool LClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	return false;
}

void LClipBoard::Binary(FormatType Format, BinaryCb Callback)
{
}

