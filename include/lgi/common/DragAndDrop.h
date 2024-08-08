/// \file
/// \author Matthew Allen
#ifndef __DRAG_AND_DROP
#define __DRAG_AND_DROP

#include "lgi/common/Variant.h"

#define DEBUG_DND	0

#if WINNATIVE

#include <shlobj.h>
// #include "GCom.h"

#else

#ifndef S_OK
#define S_OK								0
#endif

#ifndef DROPEFFECT_NONE
#define DROPEFFECT_NONE						0x0
#endif

#ifndef DROPEFFECT_COPY
#define DROPEFFECT_COPY						0x1
#endif

#ifndef DROPEFFECT_MOVE
#define DROPEFFECT_MOVE						0x2
#endif

#ifndef DROPEFFECT_LINK
#define DROPEFFECT_LINK						0x4
#endif

#endif

#ifdef __GTK_H__
LgiFunc int GtkGetDndType(const char *Format);
LgiFunc const char *GtkGetDndFormat(int Type);
LgiFunc Gtk::GdkDragAction DropEffectToAction(int DropEffect);
#endif
LgiFunc const char *LMimeToUti(const char *Mime);

//////////////////////////////////////////////////////////////////////////////
struct LgiClass LDragData
{
	LString Format;
	LArray<LVariant> Data;
	
	bool IsFormat(LString Fmt)
	{
		return	Format.Get() != NULL &&
				Fmt != NULL &&
				!_stricmp(Format, Fmt);
	}

	void Swap(LDragData &o)
	{
		Format.Swap(o.Format);
		Data.Swap(o.Data);
	}
	
	LString ToString()
	{
		LStringPipe p;
		p.Print("fmt=%s,len=" LPrintfSizeT "\n", Format.Get(), Data.Length());
		for (size_t i=0; i<Data.Length(); i++)
		{
			p.Print("\n[" LPrintfSizeT "]=%s", i, Data[i].ToString().Get());
		}
		return p.NewLStr();
	}
	
	// \sa LDropFiles
	bool IsFileDrop()
	{
		return IsFormat(LGI_FileDropFormat)
			#ifdef MAC
			|| IsFormat("NSFilenamesPboardType")
			#endif
			;
	}
	
	bool IsFileStream()
	{
		return IsFormat(LGI_StreamDropFormat);
	}
	
	bool AddFileStream(const char *LeafName, const char *MimeType, LAutoPtr<LStreamI> Stream)
	{
		if (!LeafName || !MimeType || !Stream)
			return false;
		
		if (!Format)
			Format = LGI_StreamDropFormat;
		else if (!Format.Equals(LGI_StreamDropFormat))
			return false;
		
		Data.New() = LeafName;
		Data.New() = MimeType;
		Data.New().SetStream(Stream.Release(), true);
		
		return true;
	}

	const char *GetFileStreamName()
	{
		if (!IsFileStream() || Data.Length() != 3)
			return NULL;
		return Data[0].Str();
	}

	const char *GetFileStreamMimeType()
	{
		if (!IsFileStream() || Data.Length() != 3)
			return NULL;
		return Data[1].Str();
	}
};

class LgiClass LDragFormats
{
	friend class LDragDropSource;
	friend class LDataObject;
	friend class LDragDropTarget;

	bool Source;
	
	struct Fmt : public LString
	{
		bool Val;
		Fmt()
		{
			Val = false;
		}
	};
	
	LArray<Fmt> Formats;

public:
	LDragFormats(bool source);

	bool IsSource() { return Source; }
	void SetSource(bool s) { Source = s; }
	LString ToString();
	size_t Length() { return Formats.Length(); }
	void Empty() { Formats.Empty(); }
	const char *operator[](ssize_t Idx) { return Formats.IdxCheck(Idx) ? Formats[Idx].Get() : NULL; }
	
	bool HasFormat(const char *Fmt);

	void SupportsFileDrops();
	void SupportsFileStreams();
	void Supports(LString Fmt);
	LString::Array GetSupported();
};

/// A drag source class
class LgiClass LDragDropSource
#if WINNATIVE
	: public IDropSource, public IEnumFORMATETC
#endif
{
	friend class LDataObject;

protected:
	class LDndSourcePriv *d;
	
	#if WINNATIVE
	int Index;

	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

	// IEnumFORMATETC
	HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
	HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
	HRESULT STDMETHODCALLTYPE Reset();
	HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC **ppenum);

	// IDropSource
	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD InputState);
	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect);
	#endif

public:
	LDragDropSource();
	~LDragDropSource();

	/// Sets the icon used to show what is being dragged.
	bool SetIcon
	(
		/// The image to show. The caller should retain the object in memory
		/// until after the drag.
		LSurface *Img,
		/// If only a portion of the image is needed set this to the part to
		/// use. Otherwise NULL means use the whole image.
		LRect *SubRgn = NULL
	);

	/// Start a drag operation
	/// \returns The operation that took effect: #DROPEFFECT_NONE, #DROPEFFECT_COPY etc. 
	int Drag(LView *SourceWnd, OsEvent Event, int Effect, LSurface *Icon = NULL);

	/// Called when window is registered
	virtual void OnRegister(bool Suc) {}
	/// Called on start of drag op
	virtual void OnStartData() {}
	/// Called on end of drag op
	virtual void OnEndData() {}
	
	/// This is called when you are being asked for your
	/// data. You must provide it in the format specified
	/// and return it via the LVariant.
	virtual bool GetData
	(
		/// Fill out as many LDragData structures as you need.
		LArray<LDragData> &Data
	) { return false; }

	/// This is called to see what formats your support
	/// Insert into the list dynamically allocated strings
	/// that describe the formats you can supply data in.
	/// One of these will be chosen by the target and passed
	/// to 'GetData'
	///
	/// You can define your own formats by making up a string
	/// or you can use OS defined ones to interact with other
	/// apps in the system. e.g. Win32 defines "CF_HDROP" for file
	/// d'n'd. You can use 'FormatToInt' and 'FormatToStr' to
	/// convert to and from the OS's integer id's. This works
	/// the same on Win32 as Linux. But the system formats will
	/// obviously be different.
	///
	/// The default file drop format for each OS is defined as
	/// #LGI_FileDropFormat.
	virtual bool GetFormats
	(
		/// List of format you can provide. You should keep the format
		/// length to 4 bytes on the Mac.
		LDragFormats &Formats
	)
	{ return false; }

	/// Creates a file drop
	static bool CreateFileDrop(LDragData *OutputData, LMouse &m, LString::Array &Files);
};

/// A drag target class
class LgiClass LDragDropTarget
#if WINNATIVE
	: public IDropTarget
#endif
{
private:
	LView *To;
	LDragFormats Formats;

	#ifdef __GTK_H__
	friend Gtk::gboolean LWindowDragDataDrop(Gtk::GtkWidget *widget, Gtk::GdkDragContext *context, Gtk::gint x, Gtk::gint y, Gtk::guint time, class LWindow *Wnd);
	friend void LWindowDragDataReceived(Gtk::GtkWidget *widget, Gtk::GdkDragContext *context, Gtk::gint x, Gtk::gint y, Gtk::GtkSelectionData *data, Gtk::guint info, Gtk::guint time, LWindow *Wnd);
	friend struct LGtkDrop;
	LArray<LDragData> Data;
	#endif

	#ifdef WIN32
	LONG Refs;
	#endif

	#if WINNATIVE
	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

	// IDropTarget
	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	HRESULT STDMETHODCALLTYPE DragLeave(void);
	HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	#endif

protected:
	#if WINNATIVE
	// Data elements
	IDataObject *DataObject;

	// Tools
	bool OnDropFileGroupDescriptor(FILEGROUPDESCRIPTOR *Data, LString::Array &Files);
	#endif

public:
	LDragDropTarget();
	~LDragDropTarget();

	/// call this when you have a operating system view handle (e.g. HWND/Window/HIViewRef)
	void SetWindow(LView *To);

	// Override these

	/// Initialize event
	virtual void OnDragInit(bool Suc) {}
	/// Drag entered this target event
	virtual void OnDragEnter() {}
	/// Drag exited this target event
	virtual void OnDragExit() {}

	/// 'WillAccept' is called to see whether this target
	/// can cope with any of the data types being passed to it.
	/// Once you have decided what format you want the data in
	/// clear all the other formats from the list. The first
	/// format left in the list will be passed to the 'OnDrop'
	/// function.
	/// \returns #DROPEFFECT_NONE for failure or #DROPEFFECT_COPY, #DROPEFFECT_MOVE, #DROPEFFECT_LINK
	virtual int WillAccept
	(
		/// The list of formats the source provides, delete any you can't handle
		LDragFormats &Formats,
		/// The mouse pointer in view space co-ords
		LPoint Pt,
		/// The current keyboard mobifiers
		/// \sa #LGI_EF_CTRL, #LGI_EF_ALT, #LGI_EF_SHIFT
		int KeyState
	) = 0;

	/// 'OnDrop' is called when the user releases the data over
	/// your window. The data is going to be a binary LVariant
	/// in the format you accepted earlier.
	/// \returns #DROPEFFECT_NONE for failure or #DROPEFFECT_COPY, #DROPEFFECT_MOVE, #DROPEFFECT_LINK
	virtual int OnDrop
	(
		/// All the available data formats for this drop
		LArray<LDragData> &Data,
		/// The mouse coords
		LPoint Pt,
		/// The keyboard modifiers
		/// \sa #LGI_EF_CTRL, #LGI_EF_ALT, #LGI_EF_SHIFT
		int KeyState
	) = 0;

	#ifdef MAC
		#if LGI_COCOA
		#elif LGI_CARBON
		OSStatus OnDragWithin(LView *v, DragRef Drag);
		OSStatus OnDragReceive(LView *v, DragRef Drag);
		#endif
	#endif
};

#endif
