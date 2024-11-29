
#include "lgi/common/Lgi.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/Palette.h"
#include "lgi/common/Com.h"

class LClipBoardPriv
{
public:
	LString Utf8;
	LAutoWString Wide;
};

#if 0
class LFileEnum : public LUnknownImpl<IEnumFORMATETC>
{
	int Idx;
	LArray<LClipBoard::FormatType> Types;

public:
	LFileEnum(LClipBoard::FormatType type)
	{
		Idx = 0;
		// Types.Add(type);
		Types.Add(CF_HDROP);

		AddInterface(IID_IEnumFORMATETC, (IEnumFORMATETC*)this);
	}

	~LFileEnum()
	{
		LgiTrace("%s:%i - ~LFileEnum()\n", _FL);
	}

    HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC *fmt, ULONG *pceltFetched)
	{
		if (!fmt || Idx >= Types.Length())
			return S_FALSE;

		fmt->cfFormat = Types[Idx];
		fmt->dwAspect = DVASPECT_CONTENT;
		fmt->lindex = -1;
		fmt->ptd = NULL;
		fmt->tymed = TYMED_HGLOBAL;

		if (pceltFetched) *pceltFetched = 1;
		Idx += celt;

		LgiTrace("%s:%i - Next(%i) returned '%s'\n", _FL, celt, LClipBoard::FmtToStr(fmt->cfFormat).Get());
		return S_OK;
	}
        
    HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
	{
		return S_OK;
	}
        
    HRESULT STDMETHODCALLTYPE Reset()
	{
		return S_OK;
	}

    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC **ppenum)
	{
		return E_NOTIMPL;
	}
};

struct LFileName : public LUnknownImpl<IUnknown>
{
	char16 *w;

	LFileName(STGMEDIUM *Med, const char *u)
	{
		Med->tymed = TYMED_FILE;
		Med->lpszFileName = w = Utf8ToWide(u);
		Med->pUnkForRelease = this;
	}

	~LFileName()
	{
		DeleteArray(w);
	}
};

class LFileData : public LUnknownImpl<IDataObject>
{
	int Cur;
	LClipBoard::FormatType Type, PrefDrop, ShellIdList;

public:
	LString::Array Files;

	LFileData(LString::Array &files) : Files(files)
	{
		// TraceRefs = true;
		Type = LClipBoard::StrToFmt(CFSTR_FILENAMEA);
		PrefDrop = LClipBoard::StrToFmt(CFSTR_PREFERREDDROPEFFECT);
		ShellIdList = LClipBoard::StrToFmt(CFSTR_SHELLIDLIST);
		Cur = 0;
		AddInterface(IID_IDataObject, (IDataObject*)this);

		LgiTrace("%s:%i - LFileData() = %p\n", _FL, this);
	}

	~LFileData()
	{
		LgiTrace("%s:%i - ~LFileData()\n", _FL);
	}

	HRESULT STDMETHODCALLTYPE GetData(FORMATETC *Fmt, STGMEDIUM *Med)
	{
		LString sFmt = LClipBoard::FmtToStr(Fmt->cfFormat);
		LgiTrace("%s:%i - GetData(%s) starting...\n", _FL, sFmt.Get());
		
		if (!Med)
			return E_INVALIDARG;

		if (Fmt->cfFormat == PrefDrop)
		{
			Med->tymed = TYMED_HGLOBAL;
			Med->hGlobal = GlobalAlloc(GHND, sizeof(DWORD));
			DWORD* data = (DWORD*)GlobalLock(Med->hGlobal);
			*data = DROPEFFECT_COPY;
			GlobalUnlock(Med->hGlobal);
			Med->pUnkForRelease = NULL;
		}
		else if (Fmt->cfFormat == Type)
		{
			new LFileName(Med, Files[Cur++]);
		}
		else if (Fmt->cfFormat == CF_HDROP)
		{
			LDragDropSource Src;
			LDragData Data;
			LMouse m;
			if (!Src.CreateFileDrop(&Data, m, Files))
			{
				LgiTrace("%s:%i - CreateFileDrop failed.\n", _FL);
				return E_FAIL;
			}

			LVariant &d = Data.Data[0];
			Med->tymed = TYMED_HGLOBAL;
			Med->hGlobal = GlobalAlloc(GHND, d.Value.Binary.Length);
			if (Med->hGlobal == NULL)
			{
				LgiTrace("%s:%i - GlobalAlloc failed.\n", _FL);
				return E_FAIL;
			}

			char* data = (char*)GlobalLock(Med->hGlobal);
			memcpy(data, d.Value.Binary.Data, d.Value.Binary.Length);
			GlobalUnlock(Med->hGlobal);
			Med->pUnkForRelease = NULL;
		}
		else if (Fmt->cfFormat == ShellIdList)
		{
			LgiTrace("%s:%i - GetData ShellIdList not supported.\n", _FL);
			return E_NOTIMPL;

			/*
			LPIDA Data = NULL;
			ITEMIDLIST *IdList = NULL;
			size_t Size = sizeof(CIDA) + (Files.Length() * sizeof(UINT)) + (Files.Length() * sizeof(ITEMIDLIST));
			
			Med->hGlobal = GlobalAlloc(GHND, Size);
			if (Med->hGlobal == NULL)
				return E_FAIL;
			Data = (LPIDA) GlobalLock(Med->hGlobal);

			Data->cidl = Files.Length();
			LPITEMIDLIST *Parent = (LPITEMIDLIST) (Data + 1);
			Data->aoffset[0] = (char*)Parent - (char*)Data;

			LPointer p;
			p.vp = Parent + 1;
			for (unsigned i=0; i<Files.Length(); i++)
			{
				Data->aoffset[i+1]
			}

			GlobalUnlock(Med->hGlobal);
			Med->pUnkForRelease = NULL;
			*/
		}
		else
		{
			LgiTrace("%s:%i - GetData(%s) not supported.\n", _FL, sFmt.Get());
			return DV_E_FORMATETC;
		}

		LgiTrace("%s:%i - GetData(%s) OK.\n", _FL, sFmt.Get());
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium)
	{
		LgiTrace("%s:%i - GetDataHere not impl\n", _FL);
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *Fmt)
	{
		if (Fmt->cfFormat == Type ||
			Fmt->cfFormat == CF_HDROP)
			return S_OK;
		
		LString sFmt = LClipBoard::FmtToStr(Fmt->cfFormat);
		LgiTrace("%s:%i - QueryGetData(%s) not supported.\n", _FL, sFmt.Get());
		return DV_E_FORMATETC;
	}

	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *Fmt, FORMATETC *pformatetcOut)
	{
		LgiTrace("%s:%i - GetCanonicalFormatEtc not impl\n", _FL);
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetData(FORMATETC *Fmt, STGMEDIUM *pmedium, BOOL fRelease)
	{
		LString sFmt = LClipBoard::FmtToStr(Fmt->cfFormat);
		LgiTrace("%s:%i - SetData(%s)\n", _FL, sFmt.Get());		
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC **enumFmt)
	{
		if (dir == DATADIR_GET)
		{
			if (*enumFmt = new LFileEnum(Type))
				(*enumFmt)->AddRef();
			LgiTrace("%s:%i - Returning LFileEnum obj.\n", _FL);
			return S_OK;
		}
		else if (dir == DATADIR_SET)
		{
		}

		LgiTrace("%s:%i - EnumFormatEtc error\n", _FL);
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
	{
		LgiTrace("%s:%i - DAdvise not impl\n", _FL);
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection)
	{
		LgiTrace("%s:%i - DUnadvise not impl\n", _FL);
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **ppenumAdvise)
	{
		LgiTrace("%s:%i - EnumDAdvise not impl\n", _FL);
		return E_NOTIMPL;
	}
};
#endif

void LClipBoard::Files(FilesCb Callback)
{
	LString::Array f;
	LString errMsg;

	if (Open)
	{
		CloseClipboard();
		Open = FALSE;
	}

	LPDATAOBJECT pObj = NULL;
	auto r = OleGetClipboard(&pObj);
	if (FAILED(r))
	{
		errMsg = "OleGetClipboard failed.";

		LArray<FormatType> Fmts;
		if (EnumFormats(Fmts))
		{
			for (auto f : Fmts)
			{
				auto s = FmtToStr(f);
				LgiTrace("ClipFmt: %s\n", s.Get());
			}
		}
	}
	else
	{
		LArray<CLIPFORMAT> Fmts;
		IEnumFORMATETC *pEnum = NULL;
		r = pObj->EnumFormatEtc(DATADIR_GET, &pEnum);
		if (FAILED(r))
		{
			errMsg = "EnumFormatEtc failed.";
		}
		else
		{
			FORMATETC Fmt;
			r = pEnum->Next(1, &Fmt, NULL);
			while (r == S_OK)
			{
				Fmts.Add(Fmt.cfFormat);
				LgiTrace("Got format: 0x%x = %s\n", Fmt.cfFormat, FmtToStr(Fmt.cfFormat).Get());

				if (Fmt.cfFormat == CF_HDROP)
				{
					STGMEDIUM Med;
					auto Res = pObj->GetData(&Fmt, &Med);
					if (Res == S_OK)
					{
						switch (Med.tymed)
						{
							case TYMED_HGLOBAL:
							{
								auto Sz = GlobalSize(Med.hGlobal);
								DROPFILES *p = (DROPFILES*)GlobalLock(Med.hGlobal);
								if (p)
								{
									LPointer End;
									End.c = (char*)p + Sz;
									if (p->fWide)
									{
										wchar_t *w = (wchar_t*) ((char*)p + p->pFiles);
										while (w < End.w && *w)
										{
											f.Add(w);
											w += Strlen(w) + 1;
										}
									}
									else
									{
										char *n = (char*)p + p->pFiles;
										while (n < End.c && *n)
										{
											auto u = LFromNativeCp(n);
											f.Add(u.Get());
											n += Strlen(n) + 1;
										}
									}
								}

								/*
								int Count = DragQueryFileW(hDrop, -1, NULL, 0);

								for (int i=0; i<Count; i++)
								{
									char16 FileName[256];
									if (DragQueryFileW(hDrop, i, FileName, sizeof(FileName)-1) > 0)
									{
										FileNames.Add(WideToUtf8(FileName));
									}
								}
								*/

								GlobalUnlock(Med.hGlobal);
								break;
							}
						}
					}
				}

				r = pEnum->Next(1, &Fmt, NULL);
			}

			pEnum->Release();
		}

		pObj->Release();
	}

	if (Callback)
		Callback(f, errMsg);
}

bool LClipBoard::Files(LString::Array &Paths, bool AutoEmpty)
{
	LDragDropSource Src;
	LDragData Output;
	LMouse m;
	if (Owner)
		Owner->GetMouse(m, true);

	if (!Src.CreateFileDrop(&Output, m, Paths))
		return false;

	LVariant &v = Output.Data[0];
	if (v.Type != GV_BINARY)
		return false;

	HGLOBAL hMem = GlobalAlloc(GMEM_ZEROINIT|GMEM_MOVEABLE|GMEM_DDESHARE, v.Value.Binary.Length);
    auto *p = GlobalLock(hMem);
    CopyMemory(p, v.Value.Binary.Data, v.Value.Binary.Length);
    GlobalUnlock(hMem);

	OpenClipboard(NULL);
	if (AutoEmpty)
		EmptyClipboard();
	auto r = SetClipboardData(CF_HDROP, hMem);
	CloseClipboard();
	
	return r != NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////
LString LClipBoard::FmtToStr(FormatType Fmt)
{
	TCHAR n[256] = {0};
	int r = GetClipboardFormatName(Fmt, n, CountOf(n));
	if (!r)
	{
		switch (Fmt)
		{
			case CF_TEXT:
				return "CF_TEXT";
			case CF_BITMAP:
				return "CF_BITMAP";
			case CF_UNICODETEXT:
				return "CF_UNICODETEXT";
			case CF_OEMTEXT:
				return "CF_OEMTEXT";
			#ifdef CF_HDROP
			case CF_HDROP:
				return "CF_HDROP";
			#endif
			#ifdef CF_LOCALE
			case CF_LOCALE:
				return "CF_LOCALE";
			#endif
			default:
				LAssert(!"Not impl.");
				break;
		}
	}

	return n;
}

LClipBoard::FormatType LClipBoard::StrToFmt(LString Fmt)
{
	return RegisterClipboardFormatA(Fmt);
}

///////////////////////////////////////////////////////////////////////////////////////////////
LClipBoard::LClipBoard(LView *o)
{
    d = new LClipBoardPriv;
	Open = false;
	Owner = o;
	if (Owner)
		Open = OpenClipboard(Owner->Handle()) != 0;
}

LClipBoard::~LClipBoard()
{
	if (Open)
	{
		CloseClipboard();
		Open = FALSE;
	}
	DeleteObj(d);
}

LClipBoard &LClipBoard::operator =(LClipBoard &c)
{
    LAssert(0);
    return *this;
}

bool LClipBoard::EnumFormats(LArray<FormatType> &Formats)
{
	UINT Idx = 0;
	UINT Fmt;
	while (Fmt = EnumClipboardFormats(Idx++))
		Formats.Add(Fmt);
	return Formats.Length() > 0;
}

bool LClipBoard::Empty()
{
	d->Utf8.Empty();
	d->Wide.Reset();
	return EmptyClipboard() != 0;
}

// Text
bool LClipBoard::Text(const char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (Str)
	{
		auto Native = LToNativeCp(Str);
		if (Native)
		{
			Status = Binary(CF_TEXT, (uchar*)Native.Get(), strlen(Native)+1, AutoEmpty);
		}
		else LgiTrace("%s:%i - Conversion to native cs failed.\n", _FL);
	}
	else LgiTrace("%s:%i - No text.\n", _FL);

	return Status;
}

char *LClipBoard::Text()
{
	Binary(CF_TEXT, [this](auto str, auto err)
	{
		d->Utf8 = LFromNativeCp(str);
	});

	return d->Utf8;
}

bool LClipBoard::TextW(const char16 *Str, bool AutoEmpty)
{
	if (Str)
	{
		auto Len = StrlenW(Str);
		return Binary(CF_UNICODETEXT, (uchar*) Str, (Len+1) * sizeof(ushort), AutoEmpty);
	}

	return false;
}

char16 *LClipBoard::TextW()
{
	Binary(CF_UNICODETEXT, [this](auto str, auto err)
	{
		d->Wide.Reset(NewStrW((char16*) str.Get(), str.Length() / 2));
	});

	return d->Wide;
}

#ifndef CF_HTML
#define CF_HTML RegisterClipboardFormatA("HTML Format")
#endif

ssize_t EndOfElement(LString &s, const char *elem)
{
	if (!s.Get())
		return -1;
	LString e;
	e.Printf("<%s", elem);
	auto pos = s.Find(e);
	if (pos < 0)
		return -1;
	while (pos < (ssize_t)s.Length() && s(pos) != '>')
		pos++;
	return s(pos) == '>' ? pos+1 : -1;
}

ssize_t StartOfElement(LString &s, const char *elem)
{
	if (!s.Get())
		return -1;
	LString e;
	e.Printf("</%s", elem);
	return s.RFind(e);
}

bool LClipBoard::Html(const char *Doc, bool AutoEmpty)
{
	if (!Doc)
		return false;

	// Does 'Doc' have <html> element wrapper?
	LString content;
	auto hasHtml = Stristr(Doc, "<html");
	if (hasHtml)
		content = Doc;
	else
		content.Printf("<html><body>\n%s\n</body></html>", Doc);

	LString s;
	s.Printf("Version:0.9\r\n"
			"StartHTML:");
	auto startIdx = s.Length();
	s += "00000000\r\nEndHTML:";
	auto endIdx = s.Length();
	s += "00000000\r\nStartFragment:";
	auto fragmentStartIdx = s.Length();
	s += "00000000\r\nStartEnd:";
	auto fragmentEndIdx = s.Length();
	s += "00000000\r\n";
	auto contentStart = s.Length();
	auto fragmentStart = contentStart;
	s += content;
	auto contentEnd = s.Length();
	auto fragmentEnd = contentEnd;

	ssize_t pos;
	if ((pos = EndOfElement(s, "body")) >= 0)
		fragmentStart = pos;
	if ((pos = EndOfElement(s, "html")) >= 0)
		fragmentStart = pos;

	if ((pos = StartOfElement(s, "body")) >= 0)
		fragmentEnd = pos;
	if ((pos = StartOfElement(s, "html")) >= 0)
		fragmentEnd = pos;

	auto setIndex = [&s](size_t placeholder, size_t contentIdx)
	{
		LString n;
		n.Printf("%08i", (int)contentIdx);
		memcpy(s.Get() + placeholder, n.Get(), n.Length());
	};

	setIndex(startIdx, contentStart);
	setIndex(endIdx, contentEnd);
	setIndex(fragmentStartIdx, fragmentStart);
	setIndex(fragmentEndIdx, fragmentEnd);

	// LgiTrace("html->clip='%s'\n", s.Get());
	return Binary(CF_HTML, (uchar*) s.Get(), s.Length(), AutoEmpty);
}

LString LClipBoard::Html()
{
	LString Txt;
	Binary(CF_HTML, [&Txt](auto str, auto err)
	{
		Txt = str;
	});

	if (!Txt)
		return NULL;

	auto Ln = Txt.Split("\n", 20);
	ssize_t Start = -1, End = -1;
	for (auto l : Ln)
	{
		auto p = l.Strip().Split(":", 1);
		if (p.Length() == 0)
			;
		else if (p[0].Equals("StartHTML"))
			Start = (ssize_t)p[1].Int();
		else if (p[0].Equals("EndHTML"))
			End = (ssize_t)p[1].Int();
		
		if (Start >= 0 && End >= 0)
			break;
	}
	if (Start <= 0 || End <= 0)
		return false;
	
	// LgiTrace("Txt='%s'\n", Txt.Get());
	auto Content = Txt(Start, End);
	return Content.Strip();
}

// Bitmap
bool LClipBoard::Bitmap(LSurface *pDC, bool AutoEmpty)
{
	bool Status = FALSE;

	Empty();
	if (pDC)
	{
		int Bytes = BMPWIDTH(pDC->X() * pDC->GetBits());
		int Colours = (pDC->GetBits()>8) ? ((pDC->GetBits() != 24) ? 3 : 0) : 1 << pDC->GetBits();
		int HeaderSize = sizeof(BITMAPINFOHEADER);
		int PaletteSize = Colours * sizeof(RGBQUAD);
		int MapSize = Bytes * pDC->Y();
		int TotalSize = HeaderSize + PaletteSize + MapSize;

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, TotalSize);
		if (hMem)
		{
			BITMAPINFO *Info = (BITMAPINFO*) GlobalLock(hMem);
			if (Info)
			{
				// Header
				Info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				Info->bmiHeader.biWidth = pDC->X();
				Info->bmiHeader.biHeight = pDC->Y();
				Info->bmiHeader.biPlanes = 1;
				Info->bmiHeader.biBitCount = pDC->GetBits();
				
				if (pDC->GetBits() == 16 ||
					pDC->GetBits() == 32)
				{
					Info->bmiHeader.biCompression = BI_BITFIELDS;
				}
				else
				{
					Info->bmiHeader.biCompression = BI_RGB;
				}

				Info->bmiHeader.biSizeImage = MapSize;
				Info->bmiHeader.biXPelsPerMeter = 0;
				Info->bmiHeader.biYPelsPerMeter = 0;
				Info->bmiHeader.biClrUsed = 0;
				Info->bmiHeader.biClrImportant = 0;

				if (pDC->GetBits() <= 8)
				{
					// Palette
					LPalette *Pal = pDC->Palette();
					RGBQUAD *Rgb = Info->bmiColors;
					if (Pal)
					{
						auto p = (*Pal)[0];
						if (p)
						{
							for (int i=0; i<Colours; i++, p++, Rgb++)
							{
								Rgb->rgbRed = p->r;
								Rgb->rgbGreen = p->g;
								Rgb->rgbBlue = p->b;
								Rgb->rgbReserved = p->a;
							}
						}
					}
					else
					{
						memset(Rgb, 0, Colours * sizeof(RGBQUAD));
					}
				}
				else
				{
					int *Primaries = (int*) Info->bmiColors;
					
					switch (pDC->GetBits())
					{
						case 16:
						{
							Primaries[0] = Rgb16(255, 0, 0);
							Primaries[1] = Rgb16(0, 255, 0);
							Primaries[2] = Rgb16(0, 0, 255);
							break;
						}
						case 32:
						{
							Primaries[0] = Rgba32(255, 0, 0, 0);
							Primaries[1] = Rgba32(0, 255, 0, 0);
							Primaries[2] = Rgba32(0, 0, 255, 0);
							break;
						}
					}
				}

				// Bits
				uchar *Dest = ((uchar*)Info) + HeaderSize + PaletteSize;
				for (int y=pDC->Y()-1; y>=0; y--)
				{
					uchar *s = (*pDC)[y];
					if (s)
					{
						memcpy(Dest, s, Bytes);
					}
					Dest += Bytes;
				}

				#if 0
				LFile f;
				if (f.Open("c:\\tmp\\out.bmp", O_WRITE))
				{
					f.SetSize(0);
					f.Write(Info, TotalSize);
					f.Close();
				}
				#endif

				Status = SetClipboardData(CF_DIB, hMem) != 0;
			}
			else
			{
				GlobalFree(hMem);
			}
		}
	}
	return Status;
}

LAutoPtr<LSurface> LClipBoard::ConvertFromPtr(void *Ptr)
{
	LAutoPtr<LSurface> pDC;
	BITMAPINFO *Info = (BITMAPINFO*) Ptr;
	if
	(
		Info &&
		pDC.Reset(new LMemDC(_FL)) &&
		(Info->bmiHeader.biCompression == BI_RGB ||
		Info->bmiHeader.biCompression == BI_BITFIELDS)
	)
	{
		if
		(
			pDC->Create
			(
				Info->bmiHeader.biWidth,
				Info->bmiHeader.biHeight,
				LBitsToColourSpace(max(Info->bmiHeader.biPlanes * Info->bmiHeader.biBitCount, 8))
			)
		)
		{
			int Colours = 0;
			char *Source = (char*) Info->bmiColors;

			// do palette
			if (pDC->GetBits() <= 8)
			{
				if (Info->bmiHeader.biClrUsed > 0)
					Colours = Info->bmiHeader.biClrUsed;
				else
					Colours = 1 << pDC->GetBits();

				LPalette *Pal = new LPalette(NULL, Colours);
				if (Pal)
				{
					auto d = (*Pal)[0];
					RGBQUAD *s = (RGBQUAD*) Source;
					if (d)
					{
						for (int i=0; i<Colours; i++, d++, s++)
						{
							d->r = s->rgbRed;
							d->g = s->rgbGreen;
							d->b = s->rgbBlue;
							d->a = s->rgbReserved;
						}
					}
					Source = (char*) s;
					pDC->Palette(Pal);
				}
			}

			if (Info->bmiHeader.biCompression == BI_BITFIELDS)
				Source += sizeof(DWORD) * 3;

			// do pixels
			int Bytes = BMPWIDTH(pDC->X() * pDC->GetBits());
			for (int y=pDC->Y()-1; y>=0; y--)
			{
				uchar *d = (*pDC)[y];
				if (d)
					memcpy(d, Source, Bytes);
				Source += Bytes;
			}
		}
	}

	return pDC;
}

void LClipBoard::Bitmap(BitmapCb Callback)
{
	if (!Callback)
		return;

	HGLOBAL hMem = GetClipboardData(CF_DIB);
	LAutoPtr<LSurface> pDC;
	if (void *Ptr = GlobalLock(hMem))
	{
		pDC = ConvertFromPtr(Ptr);
		GlobalUnlock(hMem);
	}

	Callback(pDC, pDC ? NULL : "No bitmap on clipboard.");
}

bool LClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	bool Status = FALSE;

	if (AutoEmpty)
	{
		Empty();
	}

	if (Ptr && Len > 0)
	{
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, Len);
		if (hMem)
		{
			char *Data = (char*) GlobalLock(hMem);
			if (Data)
			{
				memcpy(Data, Ptr, Len);
				GlobalUnlock(hMem);
				Status = SetClipboardData(Format, hMem) != 0;
				if (!Status)
					LgiTrace("%s:%i - SetClipboardData failed.\n", _FL);
			}
			else
			{
				GlobalFree(hMem);
			}
		}
		else LgiTrace("%s:%i - Alloc failed.\n", _FL);
	}
	else LgiTrace("%s:%i - No data to set.\n", _FL);

	return Status;
}

void LClipBoard::Binary(FormatType Format, BinaryCb Callback)
{
	if (!Callback)
		return;

	HGLOBAL hMem = GetClipboardData(Format);
	if (!hMem)
	{
		Callback(LString(), "GetClipboardData failed.");
		return;
	}

	auto Len = GlobalSize(hMem);
	uchar *Data = (uchar*) GlobalLock(hMem);
	if (Data)
	{
		LString Ptr;
		if (Ptr.Length(Len + sizeof(char16)))
		{
			memcpy(Ptr.Get(), Data, Len);
			memset(Ptr.Get() + Len, 0, sizeof(char16)); // terminate with zeros
			Ptr.Length(Len);
			
			Callback(Ptr, LString());
		}
		else Callback(LString(), "Alloc failed.");
	}
	else Callback(LString(), "GlobalLock failed.");

	GlobalUnlock(hMem);
}

