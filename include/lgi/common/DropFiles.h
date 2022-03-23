#ifndef __DROP_FILES__
#define __DROP_FILES__

#include "lgi/common/Variant.h"
#include "lgi/common/Array.h"
#include "lgi/common/XmlTree.h"

#if WINNATIVE
#include <Shlobj.h>
#elif defined MAC
#include "lgi/common/Net.h"
LgiFunc bool LMacFileToPath(LString &a);
#endif

class LDropFiles : public LArray<const char*>
{
	LString Fmt;

public:
	LDropFiles(LDragData &dd)
	{
		if (dd.IsFileDrop())
		{
			Fmt = dd.Format;
			for (unsigned i=0; i<dd.Data.Length(); i++)
				Init(dd.Data[i]);
		}
	}

    LDropFiles(LVariant &v)
	{
		Init(v);
	}

    ~LDropFiles()
	{
		DeleteArrays();
	}

	void Init(LVariant &v)
	{
		#if WINNATIVE
	
			DROPFILES *Df = v.IsBinary() && v.Value.Binary.Length >= sizeof(DROPFILES) ? (DROPFILES*)v.Value.Binary.Data : 0;
			if (Df)
			{
				void *FilesPtr = ((char*)Df) + Df->pFiles;
				if (Df->fWide)
				{
					char16 *f = (char16*)FilesPtr;
					while (*f)
					{
						char *Utf8 = WideToUtf8(f);
						if (Utf8) Add(Utf8);
						f += StrlenW(f) + 1;
					}
				}
				else
				{
					char *f = (char*)FilesPtr;
					while (*f)
					{
						Add((char*)LNewConvertCp("utf-8", f, LAnsiToLgiCp()));
						f += strlen(f) + 1;
					}
				}
			}
	
		#elif defined __GTK_H__
	
			if (v.IsBinary())
			{
				LString s((char*)v.Value.Binary.Data, v.Value.Binary.Length);
				auto Uri = s.SplitDelimit("\r\n");
				for (int i=0; i<Uri.Length(); i++)
				{
					char *File = Uri[i];

					// Remove leading 'File:'
					if (strnicmp(File, "file:", 5) == 0) File += 5;
					#if defined(WIN32)
					while (*File == '/')
						File++;
					#elif defined(LINUX)
					for (int i=0; i<2 && *File == '/'; i++)
						File++;
					#endif

					// Decode URI
					char *in = File, *out = File;
					while (*in)
					{
						if (*in == '%')
						{
							char h[3] = { in[1], in[2], 0 };
							*out++ = htoi(h);
							in += 3;
						}
						else
						{
							*out++ = *in++;
						}
					}
					*out++ = 0;

					// Check file exists..
					LFile::Path p(File);
					if (p.Exists())
					{
						#ifdef WINDOWS
						char *c;
						while (c = strchr(File, '/'))
							*c = '\\';
						#endif

						Add(NewStr(File));
					}
				}
			}
			else printf("LDropFiles: Type not binary.\n");

		#elif defined MAC

			LArray<LVariant*> a;
			if (v.Type == GV_LIST)
			{
				for (auto f: *v.Value.Lst)
				{
					a.Add(f);
				}
			}
			else
			{
				a.Add(&v);
			}
	
			for (int i=0; i<a.Length(); i++)
			{
				LVariant *v = a[i];
				LString s;
				if (v->Type == GV_STRING)
					s = v->Str();
				else if (v->Type == GV_BINARY)
					s.Set((char*)v->Value.Binary.Data, v->Value.Binary.Length);

				if (!s)
					return;

				if (Fmt.Equals("NSFilenamesPboardType"))
				{
					LXmlTree t;
					LXmlTag r;
					LMemStream ms(v->Value.Binary.Data, v->Value.Binary.Length, false);
					if (t.Read(&r, &ms))
					{
						auto Arr = r.GetChildTag("array");
						if (!Arr) return;
						for (auto c: Arr->Children)
						{
							if (c->IsTag("string"))
							{
								auto fn = c->GetContent();
								Add(NewStr(fn));
							}
						}
					}
				}
				else
				{
					LUri u(s);
					if
					(
						(
							u.sProtocol
							&&
							stricmp(u.sProtocol, "file") == 0
						)
						&&
						(
							!u.sHost
							||
							(
								ValidStr(u.sHost)
								&&
								stricmp(u.sHost, "localhost") == 0
							)
						)
					)
					{
						LString a = u.DecodeStr(u.sPath);
						
						if (a.Get() && !strncasecmp(a, "/.file/", 7))
							LMacFileToPath(a);
					
						if (a)
							Add(NewStr(a));
					}
					else if (!u.sProtocol &&
							LFileExists(s))
					{
						Add(NewStr(s));
					}
				}
			}

		#endif
	}
};

class GDropStreams : public LArray<LStreamI*>
{
public:
	GDropStreams(LDragData &dd)
	{
		#if defined(WINDOWS)
		for (unsigned i=0; i<dd.Data.Length(); i++)
		{
			LVariant &v = dd.Data[i];
			if (v.Type == GV_LIST)
			{
				for (auto f: *v.Value.Lst)
				{
					if (f->Type == GV_STREAM)
					{
						Add(f->Value.Stream.Ptr);
						f->Value.Stream.Own = false;
					}
				}
			}
		}
		#elif defined(MAC)
		#elif defined(LINUX)
		#endif
	}

	~GDropStreams()
	{
		DeleteObjects();
	}
};

#endif
