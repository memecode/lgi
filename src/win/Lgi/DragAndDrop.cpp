/*
**	FILE:			GDragAndDrop.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/98
**	DESCRIPTION:	Drag and drop support
**
**	Copyright (C) 1998-2003, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/Com.h"

#define DND_DEBUG_TRACE 0

class LDataObject : public GUnknownImpl<IDataObject>
{
	friend class LDndSourcePriv;
	LDragDropSource *Source;

public:
	LDataObject(LDragDropSource *source);
	~LDataObject();

	// IDataObject
	HRESULT STDMETHODCALLTYPE GetData(FORMATETC *pFormatEtc, STGMEDIUM *PMedium);
	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *pFormatEtc);
	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppFormatEtc);

	// Not impl
	HRESULT STDMETHODCALLTYPE SetData(FORMATETC *pFormatetc, STGMEDIUM *pmedium, BOOL fRelease) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *PMedium) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *pFormatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **ppenumAdvise) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC * pFormatetcIn, FORMATETC * pFormatetcOut) { return E_NOTIMPL; }
};

class LDndSourcePriv
{
public:
	LDragData FileStream;
	LString CurDataInUse;
	LArray<LDragData> CurData;
	LDataObject *InDrag = NULL;

	LDndSourcePriv()
	{
	}

	~LDndSourcePriv()
	{
		LAssert(!CurDataInUse);
		if (InDrag)
			InDrag->Source = NULL;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
int MapW32FlagsToLgi(int W32Flags)
{
	int f = 0;

	if (W32Flags & MK_CONTROL) f |= LGI_EF_CTRL;
	if (W32Flags & MK_SHIFT)   f |= LGI_EF_SHIFT;
	if (W32Flags & MK_ALT)     f |= LGI_EF_ALT;
	if (W32Flags & MK_LBUTTON) f |= LGI_EF_LEFT;
	if (W32Flags & MK_MBUTTON) f |= LGI_EF_MIDDLE;
	if (W32Flags & MK_RBUTTON) f |= LGI_EF_RIGHT;

	return f;
}

int FormatToInt(LString s)
{
	if (s && stricmp(s, "CF_HDROP") == 0) return CF_HDROP;
	return RegisterClipboardFormatA(s);
}

char *FormatToStr(int f)
{
	static char b[128];
	if (GetClipboardFormatNameA(f, b, sizeof(b)) > 0) return b;
	if (f == CF_HDROP) return "CF_HDROP";
	return 0;
}

LDataObject::LDataObject(LDragDropSource *source)
{
	AddInterface(IID_IDataObject, this);
	Source = source;
	Source->d->InDrag = this;
	Source->OnStartData();
}

LDataObject::~LDataObject()
{
	if (Source)
	{
		Source->OnEndData();
		Source->d->InDrag = NULL;
	}
}

HRESULT LDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppFormatEtc)
{
	*ppFormatEtc = dynamic_cast<IEnumFORMATETC*>(Source);
	return (*ppFormatEtc) ? S_OK : E_OUTOFMEMORY;
}

HRESULT LDataObject::GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
{
	if (!pFormatEtc || !pMedium)
		return E_INVALIDARG;

	LString InFormat = FormatToStr(pFormatEtc->cfFormat);
	bool IsFileContents = InFormat.Find("FileContents") >= 0;

	HRESULT Ret = DV_E_FORMATETC;
	int CurFormat = 0;
	LDragFormats Formats(true);
	if (Source->d->CurDataInUse)
	{
		LAssert(!"Something is using CurData");
		return E_UNEXPECTED;
	}
	else Source->d->CurData.Length(0);
	Source->GetFormats(Formats);
	for (auto f: Formats.Formats)
	{
		int n = FormatToInt(f);
		if (n == pFormatEtc->cfFormat)
		{
			CurFormat = n;
			if (Source->d->CurDataInUse)
			{
				LAssert(!"CurData in use.");
			}
			else
			{
				LDragData &CurData = Source->d->CurData.New();
				CurData.Format = f;
			}
		}
	}
	Formats.Empty();

	#if DND_DEBUG_TRACE
	LgiTrace(	"GetData 0 pFormatEtc{'%s',%p,%i,%i,%i} pMedium{%i} CurrentFormat=%s\n",
				InFormat.Get(),
				pFormatEtc->ptd,
				pFormatEtc->dwAspect,
				pFormatEtc->lindex,
				pFormatEtc->tymed,
				pMedium->tymed,
				FormatToStr(CurFormat));
	#endif

	if (IsFileContents)
	{
		int asd=0;
	}

	if (Source->d->CurData.Length())
	{
		// the users don't HAVE to use this...
		Source->d->CurDataInUse.Printf("%s:%i", _FL);
		Source->GetData(Source->d->CurData);

		ZeroObj(*pMedium);

		uchar *Ptr = NULL;
		ssize_t Size = 0;
		FILEGROUPDESCRIPTORW grp;
		LDragData &CurData = Source->d->CurData[0];

		if (IsFileContents && Source->d->FileStream.IsFileStream())
		{
			auto &Contents = Source->d->FileStream;
			if (Contents.Data.Length() == 3)
			{
				auto &v = Contents.Data[2];
				if (v.Type == GV_STREAM)
				{
					pMedium->tymed = TYMED_ISTREAM;
					pMedium->pstm = new LStreamWrap(Contents.GetFileStreamName(), v.Value.Stream.Release());
					pMedium->pstm->AddRef();
					
					#if 0
					pMedium->pUnkForRelease = PMedium->pstm;
					pMedium->pUnkForRelease->AddRef();
					#endif

					Ret = S_OK;
				}
				else LAssert(!"Not a stream object?");
			}
			else LAssert(!"Incorrect file stream object count.");
		}
		else if (CurData.Data.Length() > 0)
		{
			if (CurData.IsFileStream())
			{
				if (CurData.Data.Length() == 3)
				{
					// Save the file stream objects, we'll need them for the CFSTR_FILECONTENTS handler below.
					auto &Contents = Source->d->FileStream;
					Contents.Swap(CurData);

					ZeroObj(grp);
					FILEDESCRIPTORW &f = grp.fgd[grp.cItems++];
					f.dwFlags = FD_UNICODE | FD_PROGRESSUI;

					auto &v = Contents.Data[2];
					if (v.Type == GV_STREAM)
					{
						auto Stream = v.Value.Stream.Ptr;
						if (Stream)
						{
							ssize_t Sz = Stream->GetSize();
							if (Sz > 0)
							{
								f.dwFlags |= FD_FILESIZE;
								f.nFileSizeHigh = Sz >> 32;
								f.nFileSizeLow = Sz & 0xffffffff;
							}

							LAutoWString w(Utf8ToWide(Contents.GetFileStreamName()));
							wcsncpy_s(f.cFileName, w, MAX_PATH_LEN);

							pMedium->tymed = TYMED_HGLOBAL;
							Ptr = (uchar*)&grp;
							Size = sizeof(grp);
						}
						else LAssert(!"Stream is NULL.");
					}
					else LAssert(!"Not a stream variant.");
				}
				else LAssert(!"Incorrect file stream object count.");
			}
			else
			{
				LVariant &Data = CurData.Data[0];
				switch (Data.Type)
				{
					case GV_NULL:
						break;
					case GV_BINARY:
					{
						pMedium->tymed = TYMED_HGLOBAL;
						Ptr = (uchar*)Data.Value.Binary.Data;
						Size = Data.Value.Binary.Length;
						break;
					}
					case GV_STRING:
					{
						pMedium->tymed = TYMED_HGLOBAL;
						Ptr = (uchar*)Data.Value.String;
						Size = Ptr ? strlen((char*)Ptr) + 1 : 0;
						break;
					}
					case GV_VOID_PTR:
					{
						pMedium->tymed = TYMED_HGLOBAL;
						Ptr = (uchar*)&Data.Value.Ptr;
						Size = sizeof(Data.Value.Ptr);
						break;
					}
					default:
					{
						// Unsupported format...
						LAssert(0);
						break;
					}
				}
			}

			if (pMedium->tymed == TYMED_HGLOBAL &&
				Ptr &&
				Size > 0)
			{
				pMedium->hGlobal = GlobalAlloc(GHND, Size);
				if (pMedium->hGlobal)
				{
					void *g = GlobalLock(pMedium->hGlobal);
					if (g)
					{
						memcpy(g, Ptr, Size);
						GlobalUnlock(pMedium->hGlobal);
						Ret = S_OK;
					}
					else Ret = E_OUTOFMEMORY;
				}
				else Ret = E_OUTOFMEMORY;
			}

			#if DND_DEBUG_TRACE
			LgiTrace("GetData 2, Ret=%i\n", Ret);
			#endif
		}

		Source->d->CurDataInUse.Empty();
	}

	return Ret;
}

HRESULT LDataObject::QueryGetData(FORMATETC *pFormatEtc)
{
	HRESULT Ret = E_INVALIDARG;

	if (pFormatEtc)
	{
		Ret = S_OK;

		LDragFormats Formats(true);
		Source->GetFormats(Formats);
		bool HaveFormat = false;
		for (auto i: Formats.Formats)
		{
			if (pFormatEtc->cfFormat == FormatToInt(i))
			{
				HaveFormat = true;
			}
		}
		Formats.Empty();

		if (!HaveFormat)
		{
			#if DND_DEBUG_TRACE
			char *TheirFormat = FormatToStr(pFormatEtc->cfFormat);
			LgiTrace("QueryGetData didn't have '%s' format.\n", TheirFormat);
			#endif
			Ret = DV_E_FORMATETC;
		}
		else if (pFormatEtc->tymed != TYMED_HGLOBAL)
		{
			#if DND_DEBUG_TRACE
			LgiTrace("QueryGetData no TYMED!\n");
			#endif
			Ret = DV_E_TYMED;
		}
	}
	
	return Ret;
}

/////////////////////////////////////////////////////////////////////////////////////////
LDragDropSource::LDragDropSource()
{
	d = new LDndSourcePriv;
	Index = 0;

	OnRegister(true);
}

LDragDropSource::~LDragDropSource()
{
	DeleteObj(d);
}

bool LDragDropSource::CreateFileDrop(LDragData *OutputData, LMouse &m, LString::Array &Files)
{
	if (!OutputData || !Files.First())
		return false;

	size_t Size = sizeof(DROPFILES) + sizeof(char16);

	List<char16> NativeW;
	for (auto File : Files)
	{
		LAutoWString f(Utf8ToWide(File));
		if (f)
		{
			auto Len = StrlenW(f) + 1;
			Size += Len * sizeof(char16);
			NativeW.Insert(f.Release());
		}
	}

	DROPFILES *Dp = (DROPFILES*) new char[Size];
	bool Status = false;
	if (Dp)
	{
		Dp->pFiles = sizeof(DROPFILES);
		Dp->fWide = TRUE;
		Dp->fNC = true;
		Dp->pt.x = m.x;
		Dp->pt.y = m.y;

		char16 *f = (char16*) (((char*)Dp) + Dp->pFiles);
		for (auto File: NativeW)
		{
			auto Len = StrlenW(File) + 1;
			StrcpyW(f, File);
			f += Len;
		}
		*f++ = 0;

		Status = OutputData->Data[0].SetBinary(Size, (uchar*)Dp);
		if (Status)
			OutputData->Format = LGI_FileDropFormat;
		DeleteArray((char*&)Dp);
	}

	NativeW.DeleteArrays();

	OutputData->Format = LGI_FileDropFormat;

	return Status;
}

bool LDragDropSource::SetIcon(LSurface *Img, LRect *SubRgn)
{
	return false;
}

int LDragDropSource::Drag(LView *SourceWnd, OsEvent Event, int Effect, LSurface *Icon)
{
	LAssert(SourceWnd != 0);
	if (!SourceWnd)
		return -1;

	DWORD dwEffect = DROPEFFECT_NONE;

	static bool InDrag = false;
	if (!InDrag) // Don't allow recursive drags...
	{
		InDrag = true;

		Reset();
		IDataObject *Data = 0;
		if (QueryInterface(IID_IDataObject, (void**)&Data) == S_OK)
		{
			int Ok = ::DoDragDrop(Data, this, Effect, &dwEffect) == DRAGDROP_S_DROP;
			Data->Release();

			if (!Ok)
				Effect = DROPEFFECT_NONE;
		}

		InDrag = false;
	}
	else
	{
		LgiTrace("%s:%i - Already in drag?\n", _FL);
	}

	return dwEffect;
}

ULONG STDMETHODCALLTYPE
LDragDropSource::AddRef()
{
	return 1;
}

ULONG STDMETHODCALLTYPE
LDragDropSource::Release()
{
	return 0;
}

HRESULT
STDMETHODCALLTYPE
LDragDropSource::QueryInterface(REFIID iid, void **ppv)
{
	*ppv=NULL;
	if (IID_IUnknown==iid)
	{
		*ppv=(void*)(IUnknown*)(IDataObject*)this;
	}

	if (IID_IEnumFORMATETC==iid)
	{
		*ppv=(void*)(IEnumFORMATETC*) this;
	}

	if (IID_IDataObject==iid)
	{
		LDataObject *Obj = new LDataObject(this);
		if (Obj)
		{
			*ppv= (void*)Obj;
			Obj->AddRef();
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}

	if (IID_IDropSource==iid)
	{
		*ppv=(void*)(IDropSource*) this;
	}

	if (NULL==*ppv)
	{
		return E_NOINTERFACE;
	}

	AddRef();
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
LDragDropSource::Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched)
{
	HRESULT Ret = S_FALSE;

	#if DND_DEBUG_TRACE
	LgiTrace("Next[%i]=", Index);
	#endif

	LDragFormats Formats(true);
	if (rgelt &&
		GetFormats(Formats))
	{
		auto i = Formats[Index];

		if (pceltFetched)
		{
			*pceltFetched = i ? 1 : 0;
		}

		if (i)
		{
			#if DND_DEBUG_TRACE
			LgiTrace("\t%s", i);
			#endif

			rgelt->cfFormat = FormatToInt(i);
			rgelt->ptd = 0;
			rgelt->dwAspect = DVASPECT_CONTENT;
			rgelt->lindex = -1;
			rgelt->tymed = TYMED_HGLOBAL;
			Index++;

			Ret = S_OK;
		}
	}

	return Ret;
}

HRESULT STDMETHODCALLTYPE
LDragDropSource::Skip(ULONG celt)
{
	Index += celt;
	#if DND_DEBUG_TRACE
	LgiTrace("Skip Index=%i", Index);
	#endif
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE
LDragDropSource::Reset()
{
	Index = 0;
	#if DND_DEBUG_TRACE
	LgiTrace("Reset Index=%i", Index);
	#endif
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
LDragDropSource::Clone(IEnumFORMATETC **ppenum)
{
	if (ppenum)
	{
		*ppenum = NULL;
		if (*ppenum)
		{
			return S_OK;
		}
		return E_OUTOFMEMORY;
	}
	return E_INVALIDARG;
}

HRESULT
STDMETHODCALLTYPE
LDragDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD InputState)
{
	HRESULT Ret = S_OK;

	if (fEscapePressed)
	{
		Ret = DRAGDROP_S_CANCEL;
	}
	else if (!(InputState & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)))
	{
		Ret = DRAGDROP_S_DROP;
	}
	
	#ifdef DND_DEBUG_TRACE
	// LgiTrace("QueryContinueDrag, fEscapePressed=%i, InputState=%i, Result=%i", fEscapePressed, InputState, Ret);
	#endif

	return Ret;
}

HRESULT
STDMETHODCALLTYPE
LDragDropSource::GiveFeedback(DWORD dwEffect)
{
	return DRAGDROP_S_USEDEFAULTCURSORS;
}

////////////////////////////////////////////////////////////////////////////////////////////
LDragDropTarget::LDragDropTarget() : Formats(true)
{
	To = 0;

	#if WINNATIVE
	Refs = 0;
	DataObject = 0;
	#endif
}

LDragDropTarget::~LDragDropTarget()
{
}

void LDragDropTarget::SetWindow(LView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		To->DropTarget(this);
		Status = To->DropTarget(true);
		#ifdef MAC
		if (To->WindowHandle())
		#else
		if (To->Handle())
		#endif
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", __FILE__, __LINE__);
		}
	}
}

ULONG STDMETHODCALLTYPE LDragDropTarget::AddRef()
{
	return InterlockedIncrement(&Refs); 
}

ULONG STDMETHODCALLTYPE LDragDropTarget::Release()
{
	return InterlockedDecrement(&Refs); 
}

HRESULT STDMETHODCALLTYPE LDragDropTarget::QueryInterface(REFIID iid, void **ppv)
{
	*ppv=NULL;

	if (IID_IUnknown==iid)
	{
		*ppv=(void*)(IUnknown*)(IDataObject*) this;
	}

	if (IID_IDropTarget==iid)
	{
		*ppv=(void*)(IDropTarget*) this;
	}

	if (NULL==*ppv)
	{
		return E_NOINTERFACE;
	}

	AddRef();
	
	return NOERROR;
}

HRESULT STDMETHODCALLTYPE LDragDropTarget::DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	HRESULT Result = E_UNEXPECTED;

	*pdwEffect = DROPEFFECT_NONE;

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(To->Handle(), &p);
	LPoint Pt(p.x, p.y);

	// Clean out format list
	Formats.Empty();
	Formats.SetSource(true);

	// Something from another app, enum the formats.
	IEnumFORMATETC *FormatETC = 0;
	if (pDataObject->EnumFormatEtc(DATADIR_GET, &FormatETC) == S_OK &&
		FormatETC)
	{
		ULONG Fetched = 0;
		FORMATETC Format;

		// Ask what formats are being dropped
		FormatETC->Reset();
		while (FormatETC->Next(1, &Format, &Fetched) == S_OK)
		{
			if (Fetched == 1)
			{
				char *s = FormatToStr(Format.cfFormat);
				if (s)
					Formats.Supports(s);
			}
		}
		FormatETC->Release();
	}

	// Process the format list
	if (Formats.Length() > 0)
	{
		// Ask the app what formats it supports.
		// It deletes those it doesn't and leaves the formats it can handle
		Formats.SetSource(false);
		*pdwEffect = WillAccept(Formats, Pt, MapW32FlagsToLgi(grfKeyState));
		// LgiTrace("WillAccept=%i\n", *pdwEffect);
		Result = S_OK;
	}

	OnDragEnter();
	return Result;
}

HRESULT STDMETHODCALLTYPE LDragDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	if (pdwEffect)
	{
		POINT p;
		p.x = pt.x;
		p.y = pt.y;
		ScreenToClient(To->Handle(), &p);
		LPoint Pt(p.x, p.y);
		*pdwEffect = WillAccept(Formats, Pt, MapW32FlagsToLgi(grfKeyState));
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE LDragDropTarget::DragLeave()
{
	OnDragExit();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE LDragDropTarget::Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	HRESULT Result = E_UNEXPECTED;

	LAssert(To != NULL);

	DataObject = pDataObject;

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(To->Handle(), &p);
	LPoint Pt(p.x, p.y);

	LArray<LDragData> Data;	
	for (auto FormatName: Formats.Formats)
	{
		LString Str;
		bool IsStreamDrop = !_stricmp(FormatName, Str = LGI_StreamDropFormat);
		bool IsFileContents = !_stricmp(FormatName, Str = CFSTR_FILECONTENTS);
		LDragData &CurData = Data.New();
		CurData.Format = FormatName;

		FORMATETC Format;
		Format.cfFormat = FormatToInt(FormatName);
		Format.dwAspect = DVASPECT_CONTENT;
		Format.lindex = IsFileContents ? 0 : -1;
		Format.tymed = TYMED_ISTREAM | TYMED_HGLOBAL;
		Format.ptd = 0;

		STGMEDIUM Medium;
		ZeroObj(Medium);

		HRESULT Err;
		if ((Err = pDataObject->GetData(&Format, &Medium)) == S_OK)
		{
			Result = S_OK;
			switch (Medium.tymed)
			{
				case TYMED_HGLOBAL:
				{
					auto Size = GlobalSize(Medium.hGlobal);
					void *Ptr = GlobalLock(Medium.hGlobal);
					if (Ptr)
					{
						if (IsStreamDrop)
						{
							// Do special case handling of CFSTR_FILEDESCRIPTORW here,
							// because we need to call GetData more times to get the file
							// contents as well... and the layers after this don't have the
							// capability to do it.
							
							// For each file...
							LPFILEGROUPDESCRIPTORW Gd = (LPFILEGROUPDESCRIPTORW)Ptr;
							for (UINT i = 0; i < Gd->cItems; i++)
							{
								FORMATETC Fmt;
								Fmt.cfFormat = FormatToInt(CFSTR_FILECONTENTS);
								Fmt.dwAspect = DVASPECT_CONTENT;
								Fmt.lindex = i;
								Fmt.tymed = TYMED_ISTREAM;
								Fmt.ptd = 0;

								STGMEDIUM Med;
								ZeroObj(Med);
								
								// Get the stream for it...
								HRESULT res = pDataObject->GetData(&Fmt, &Med);
								if (SUCCEEDED(res) &&
									Med.tymed == TYMED_ISTREAM)
								{
									IStreamWrap *w = new IStreamWrap(Med.pstm);
									if (w)
									{
										// Wrap a LStream around it so we can talk to it...
										LVariant *s = &CurData.Data.New();
										s->Type = GV_STREAM;
										s->Value.Stream.Ptr = w;
										s->Value.Stream.Own = true;

										// Store the name/size as well...
										w->Desc.Reset(new FILEDESCRIPTORW(Gd->fgd[i]));
									}
								}
							}
						}
						else
						{
							// Default handling
							LVariant *s = &CurData.Data.New();
							s->SetBinary(Size, Ptr);
						}
						
						GlobalUnlock(Ptr);
					}
					break;
				}
				case TYMED_ISTREAM:
				{
					LVariant *s = &CurData.Data.New();
					s->Type = GV_STREAM;
					s->Value.Stream.Own = true;
					s->Value.Stream.Ptr = new IStreamWrap(Medium.pstm);
					break;
				}
				default:
				{
					// unsupported TYMED
					Result = E_UNEXPECTED;
					break;
				}
			}
		}
	}

	if (Data.Length() > 0)
	{
		OnDrop(Data, Pt, MapW32FlagsToLgi(grfKeyState));
	}

	OnDragExit();

	return Result;
}

bool LDragDropTarget::OnDropFileGroupDescriptor(FILEGROUPDESCRIPTOR *Data, LString::Array &Files)
{
	bool Status = false;

	if (Data && Data->cItems > 0 && DataObject)
	{
		for (UINT i=0; i<Data->cItems; i++)
		{
			FORMATETC Format;
			Format.cfFormat = RegisterClipboardFormat(CFSTR_FILECONTENTS);
			Format.dwAspect = DVASPECT_CONTENT;
			Format.lindex = i;
			Format.tymed = TYMED_ISTORAGE | TYMED_HGLOBAL;
			Format.ptd = 0;

			STGMEDIUM Medium;
			ZeroObj(Medium);

			if (DataObject->GetData(&Format, &Medium) == S_OK)
			{
				size_t Size = 0;
				void *Ptr = 0;

				// Get data
				if (Medium.tymed == TYMED_HGLOBAL)
				{
					Size = GlobalSize(Medium.hGlobal);
					Ptr = GlobalLock(Medium.hGlobal);
				}
				else if (Medium.tymed == TYMED_ISTREAM)
				{
					STATSTG Stat;
					ZeroObj(Stat);
					// Stat.grfStateBits = -1;
					if (Medium.pstm->Stat(&Stat, STATFLAG_DEFAULT) == S_OK)
					{
						Size = Stat.cbSize.QuadPart;
						Ptr = new char[Size];
						if (Ptr)
						{
							ulong Read;
							if (Medium.pstm->Read(Ptr, (ULONG)Size, &Read) == S_OK)
							{
								Size = Read;
							}
						}
					}
				}

				// Process data..
				if (Ptr)
				{
					char Path[256];
					LString Str;
					LGetSystemPath(LSP_TEMP, Path, sizeof(Path));
					LMakePath(Path, sizeof(Path), Path, Str = Data->fgd[i].cFileName);

					LFile f;
					if (f.Open(Path, O_WRITE))
					{
						f.Write(Ptr, Size);
						Files.Add(Path);
						Status = true;
					}
				}

				// Clean up
				if (Medium.tymed == TYMED_HGLOBAL)
				{
					GlobalUnlock(Ptr);
				}
				else if (Medium.tymed == TYMED_ISTREAM)
				{
					DeleteArray(Ptr);
				}
			}
		}
	}

	return Status;
}

