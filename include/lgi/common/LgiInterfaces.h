// \file
/// \author Matthew Allen
#ifndef _LGI_INTERFACES_H_
#define _LGI_INTERFACES_H_

// Includes
#include "lgi/common/Mem.h"
#include "lgi/common/Array.h"
#include "lgi/common/Colour.h"
#include "lgi/common/Cancel.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/LgiUiBase.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/TypeToString.h"

// Fwd defs
class LXmlTag;
class LMouseHook;
class LFont;
class LRect;
class LPoint;
class LRegion;
class LSurface;
class LMouse;
class LKey;
class LWindow;
class LVariant;
class LCss;
class LViewI;
class LView;
class LScriptArguments;
class LVolume;
class LDirectory;
class LError;

// Classes

class LDomI
{
public:
	virtual ~LDomI() {}

	virtual const char *GetClass() = 0;
	virtual bool GetValue(const char *Var, LVariant &Value) { return false; }
	virtual bool SetValue(const char *Var, LVariant &Value) { return false; }
	virtual bool CallMethod(const char *MethodName, LScriptArguments &Args) { return false; }
};

class LVirtualMachineI
{
public:
	virtual ~LVirtualMachineI() {}

	virtual void SetDebuggerEnabled(bool b) {}
	virtual void OnException(const char *File, int Line, ssize_t Address, const char *Msg) {}
};

/// Stream interface class
/// 
/// Defines the API
/// for all the streaming data classes. Allows applications to plug
/// different types of date streams into functions that take a LStream.
/// Typically this means being able to swap files with sockets or data
/// buffers etc.
/// 
class LgiClass LStreamI : virtual public LDomI
{
public:
	/// Open a connection
	/// \returns > zero on success
	virtual int Open
	(
		/// A string connection parameter
		const char *Str = 0,
		/// An integer connection parameter
		int Int = 0
	)
	{ return false; }

	/// Returns true is the stream is still open
	virtual bool IsOpen() { return false; }

	/// Closes the connection
	/// \returns > zero on success
	virtual int Close() { return 0; }

	/// \brief Gets the size of the stream
	/// \return The size or -1 on error (e.g. the information is not available)
	virtual int64 GetSize() { return -1; }

	/// \brief Sets the size of the stream
	/// \return The new size or -1 on error (e.g. the size is not set-able)
	virtual int64 SetSize(int64 Size) { return -1; }

	/// \brief Gets the current position of the stream
	/// \return Current position or -1 on error (e.g. the position is not known)
	virtual int64 GetPos() { return -1; }

	/// \brief Sets the current position of the stream
	/// \return The new current position or -1 on error (e.g. the position can't be set)
	virtual int64 SetPos(int64 Pos) { return -1; }

	/// \brief Read bytes out of the stream
	/// \return > 0 on succes, which indicates the number of bytes read
	virtual ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0) = 0;

	/// \brief Write bytes to the stream
	/// \return > 0 on succes, which indicates the number of bytes written
	virtual ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) = 0;

	/// \brief Creates a dynamically allocated copy of the same type of stream.
	/// This new stream is not connected to anything.
	/// \return The new stream or NULL on error.
	virtual LStreamI *Clone() { return 0; }

	virtual void ChangeThread() {}

	/// Utility: Read as LString.
	LString Read(ssize_t bufLen = -1)
	{
		LString s;

		if (bufLen >= 0)
		{
			s.Length(bufLen);
		}
		else
		{
			auto sz = GetSize();
			if (sz > 0)
				s.Length(sz);
			else
				s.Length(256);
		}
		LAssert(s.Length() > 0);

		// Read the data
		auto rd = Read(s.Get(), s.Length());
		if (rd < 0)
			s.Empty();
		else if (rd < (ssize_t) s.Length())
			s.Length(rd);
		
		// Make it's null terminated.
		if (s.Get())
			s.Get()[s.Length()] = 0;

		return s;
	}

	/// Utility: Write a LString
	size_t Write(const LString s)
	{
		return Write(s.Get(), s.Length());
	}
};

/// Socket logging types..
enum LSocketLogTypes
{
	/// Do no logging
	NET_LOG_NONE = 0,
	/// Log a hex dump of everything
	NET_LOG_HEX_DUMP = 1,
	/// Log just the bytes
	NET_LOG_ALL_BYTES = 2
};

/// Virtual base class for a socket. See the documentation for LSocket for a more
/// through treatment of this object's API.
class LSocketI :
	virtual public LStreamI
{
public:
	enum SocketMsgType
	{
		SocketMsgNone,
		SocketMsgInfo,
		SocketMsgSend,
		SocketMsgReceive,
		SocketMsgWarning,
		SocketMsgError,
	};

	virtual ~LSocketI() {}

	/// Returns the actual socket (as defined by the OS)
	virtual OsSocket Handle(OsSocket Set = INVALID_SOCKET) { return INVALID_SOCKET; }

	// Cancel
	virtual LCancel *GetCancel() { return NULL; }
	virtual void SetCancel(LCancel *c) { }

	// Logging and utility
	virtual class LStreamI *GetLog() { return NULL; }

	// Host/Port meta data
		/// Returns the IP at this end of the socket
		virtual bool GetLocalIp
		(
			/// Ptr to a buffer of at least 16 bytes
			char *IpAddr
		) { return false; }
		/// Return the port at this end of the connection
		virtual int GetLocalPort() { return 0; }
		/// Gets the remote IP
		virtual bool GetRemoteIp(char *IpAddr) { return false; }
		/// Return the port at this end of the connection
		virtual int GetRemotePort() { return 0; }

	// Timeout
		/// Gets the current timeout for operations in ms
		virtual int GetTimeout() { return -1; }
		/// Sets the current timeout for operations in ms
		virtual void SetTimeout(int ms) {}
		/// Sets the continue token
		// virtual void SetContinue(bool *Token) {}


	// State
		/// True if there is data available to read.
		virtual bool IsReadable(int TimeoutMs = 0) { return false; }
		/// True if the socket can be written to.
		virtual bool IsWritable(int TimeoutMs = 0) { return false; }
		/// True if the socket can be accept.
		virtual bool CanAccept(int TimeoutMs = 0) { return false; }
		/// Returns whether the socket is set to blocking or not
		virtual bool IsBlocking() { return true; }
		/// Set whether the socket should block or not
		virtual void IsBlocking(bool block) {}
		/// Get the send delay setting
		virtual bool IsDelayed() { return true; }
		/// Set the send delay setting
		virtual void IsDelayed(bool Delay) {}

// UDP
	
	/// Get UPD mode
	virtual bool GetUdp() { return false; }
	/// Set UPD mode
	virtual void SetUdp(bool b) {}
	/// Read UPD packet
	virtual int ReadUdp(void *Buffer, int Size, int Flags, uint32_t *Ip = 0, uint16_t *Port = 0) { return 0; }
	/// Write UPD packet
	virtual int WriteUdp(void *Buffer, int Size, int Flags, uint32_t Ip, uint16_t Port) { return 0; }

// Server

	/// Listens on a given port for an incoming connection.
	virtual bool Listen(int Port = 0) { return false; }
	/// Accepts an incoming connection and connects the socket you pass in to the remote host.
	virtual bool Accept(LSocketI *c) { return false; }


// Event call backs

	/// Called when the connection is dropped
	virtual void OnDisconnect() {}
	/// Called when data is read
	virtual void OnRead(char *Data, ssize_t Len) {}
	/// Called when data is written
	virtual void OnWrite(const char *Data, ssize_t Len) {}
	/// Called when an error occurs
	virtual void OnError(int ErrorCode, const char *ErrorDescription) {}
	/// Called when some events happens
	virtual void OnInformation(const char *Str) {}

	/// Process an error
	virtual int Error(void *Param) { return 0; }
	virtual const char *GetErrorString() { return NULL; }
	
	LString LocalIp()
	{
		char Ip[32];
		return GetLocalIp(Ip) ? Ip : NULL;
	}
};

class LAppI
{
public:
	/// The idle function should return false to wait for more
	/// messages, otherwise it will be called continuously when no
	/// messages are available.
	typedef bool (*OnIdleProc)(void *Param);

	/// Destroys the object
	virtual ~LAppI() {}

	virtual bool IsOk() const = 0;
	
	/// Returns this processes ID
	virtual OsProcessId GetProcessId() = 0;
	
	/// Returns the thread currently running the active message loop
	virtual OsThreadId GetGuiThreadId() = 0;
	virtual bool InThread() = 0;

	/// Resets the arguments
	virtual void SetAppArgs(OsAppArguments &AppArgs) = 0;
	
	/// Returns the arguemnts
	virtual OsAppArguments *GetAppArgs() = 0;

	/// Returns the n'th argument as a heap string. Free with DeleteArray(...).
	virtual const char *GetArgumentAt(int n) = 0;
	
	/// Enters the message loop.
	virtual bool Run
	(
		/// [Optional] Idle callback
		OnIdleProc IdleCallback = 0,
		/// [Optional] User param for IdleCallback
		void *IdleParam = 0
	) = 0;
	
	/// Event called to process the command line
	virtual void OnCommandLine() = 0;
	
	/// Event called to process files dropped on the application
	virtual void OnReceiveFiles(LArray<const char*> &Files) = 0;

	/// Event called to process URLs given to the application
	virtual void OnUrl(const char *Url) = 0;
	
	/// Exits the event loop with the code specified
	virtual void Exit
	(
		/// The application exit code.
		int Code = 0
	) = 0;
	
	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	virtual bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// String to receive the value (if any) of the option
		LString &Value
	) = 0;

	/// \brief Parses the command line for a switch
	/// \return true if the option exists.
	virtual bool GetOption
	(
		/// The option to look for.
		const char *Option,
		/// The buffer to receive the value of the command line parameter or NULL if you don't care.
		char *Dst = 0,
		/// The buffer size in bytes
		int DstSize = 0
	) = 0;
	
	/// Gets the application conf stored in lgi.conf
	virtual LString GetConfig(const char *Tag) = 0;

	/// Sets a single tag in the config. (Not written to disk)
	virtual void SetConfig(const char *Var, const char *Val) = 0;
	
	/// Gets the control with the keyboard focus
	virtual LViewI *GetFocus() = 0;
	
	/// Gets the MIME type of a file
	virtual LString GetFileMimeType
	(
		/// The file to identify
		const char *File
	) = 0;
		
	/// Get a system metric
	virtual int32 GetMetric
	(
		/// One of #LGI_MET_DECOR_X, #LGI_MET_DECOR_Y
		LSystemMetric Metric
	) = 0;

	/// Get the mouse hook instance
	virtual LMouseHook *GetMouseHook() = 0;

	/// Returns the number of cpu cores or -1 if unknown.
	virtual int GetCpuCount() { return -1; }

	/// Gets the font cache
	virtual class LFontCache *GetFontCache() = 0;
};

class LEventSinkI
{
public:
	virtual ~LEventSinkI() {}
	virtual bool PostEvent(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0, int64_t TimeoutMs = -1) = 0;
};

class LEventTargetI
{
public:
	virtual ~LEventTargetI() {}
	virtual LMessage::Result OnEvent(LMessage *Msg) = 0;
};

class LEventsI : public LEventTargetI
{
public:
	virtual ~LEventsI() {}

	// Events
	virtual void OnMouseClick(LMouse &m) = 0;
	virtual void OnMouseEnter(LMouse &m) = 0;
	virtual void OnMouseExit(LMouse &m) = 0;
	virtual void OnMouseMove(LMouse &m) = 0;
	virtual bool OnMouseWheel(double Lines) = 0;
	virtual bool OnKey(LKey &k) = 0;
	virtual void OnAttach() = 0;
	virtual void OnCreate() = 0;
	virtual void OnDestroy() = 0;
	virtual void OnFocus(bool f) = 0;
	virtual void OnPulse() = 0;
	virtual void OnPosChange() = 0;
	virtual bool OnRequestClose(bool OsShuttingDown) = 0;
	virtual int OnHitTest(int x, int y) = 0;
	virtual void OnChildrenChanged(LViewI *Wnd, bool Attaching) = 0;
	virtual void OnPaint(LSurface *pDC) = 0;
	virtual int OnCommand(int Cmd, int Event, OsView Wnd) = 0;
	virtual int OnNotify(LViewI *Ctrl, LNotification Data) = 0;
};

class LViewLayoutInfo
{
public:
	constexpr static int FILL = -1;

	struct Range
	{
		// 0 if unknown, -1 for "all available"
		int32 Min, Max;

		Range()
		{
			Min = Max = 0;
		}
	};

	Range Width;
	Range Height;
};

class LgiClass LViewI :
	public LEventsI,
	public LEventSinkI,
	public virtual LDomI
{
	friend class LView;

public:
	// Handles
	#if LGI_VIEW_HANDLE
	virtual OsView Handle() const = 0;
	#endif
	virtual int AddDispatch() = 0;
	virtual OsWindow WindowHandle() { printf("LViewI::WindowHandle()\n"); return NULL; }
	virtual LView *GetGView() { return NULL; }

	// Heirarchy
	virtual bool Attach(LViewI *p) = 0;
	virtual bool AttachChildren() = 0;
	virtual bool Detach() = 0;
	virtual bool IsAttached() = 0;
	virtual LWindow *GetWindow() = 0;
	virtual LViewI *GetParent() = 0;
	virtual void SetParent(LViewI *p) = 0;
	virtual void Quit(bool DontDelete = false) = 0;
	virtual bool AddView(LViewI *v, int Where = -1) = 0;
	virtual bool DelView(LViewI *v) = 0;
	virtual bool HasView(LViewI *v) = 0;
	virtual LArray<LViewI*> IterateViews() = 0;

	// Threading
	virtual bool Lock(const char *file, int line, int TimeOut = -1) = 0;
	virtual void Unlock() = 0;
	virtual bool InThread() = 0;

	// Properties
	virtual bool Enabled() = 0;
	virtual void Enabled(bool e) = 0;
	virtual bool Visible() = 0;
	virtual void Visible(bool v) = 0;
	virtual bool Focus() = 0;
	virtual void Focus(bool f) = 0;
	virtual class LDragDropSource *DropSource(LDragDropSource *Set = NULL) = 0;
	virtual class LDragDropTarget *DropTarget(LDragDropTarget *Set = NULL) = 0;
	virtual bool DropTarget(bool t) = 0;
	virtual bool Sunken() = 0;
	virtual void Sunken(bool i) = 0;
	virtual bool Flat() = 0;
	virtual void Flat(bool i) = 0;
	virtual bool Raised() = 0;
	virtual void Raised(bool i) = 0;
	virtual bool GetTabStop() = 0;
	virtual void SetTabStop(bool b) = 0;

	// Style
    virtual LCss *GetCss(bool Create = false) = 0;
    virtual void SetCss(LCss *css) = 0;
    virtual bool SetColour(LColour &c, bool Fore) = 0;
	virtual LString CssStyles(const char *Set = NULL) { return LString(); }
	virtual LString::Array *CssClasses() { return NULL; }
	virtual LFont *GetFont() = 0;
	virtual void SetFont(LFont *Fnt, bool OwnIt = false) = 0;

	// Name and value
	virtual bool Name(const char *n) = 0;
	virtual bool NameW(const char16 *n) = 0;
	virtual const char *Name() = 0;
	virtual const char16 *NameW() = 0;
	virtual int64 Value() = 0;
	virtual void Value(int64 i) = 0;
	const char *GetClass() { return "LViewI"; } // mainly for debugging

	// Size and position	
	virtual LRect &GetPos() = 0;
	virtual LRect &GetClient(bool InClientSpace = true) = 0;
	virtual bool SetPos(LRect &p, bool Repaint = false) = 0;
	virtual int X() = 0;
	virtual int Y() = 0;
	virtual LPoint GetMinimumSize() = 0;
	virtual void SetMinimumSize(LPoint Size) = 0;	

	// Id
	virtual int GetId() = 0;
	virtual void SetId(int i) = 0;

	// Events and notification
	virtual void SendNotify(LNotification note) = 0;
	virtual LViewI *GetNotify() = 0;
	virtual void SetNotify(LViewI *n) = 0;

	// Mouse
	virtual LCursor GetCursor(int x, int y) = 0;
	virtual bool Capture(bool c) = 0;
	virtual bool IsCapturing() = 0;
	virtual bool GetMouse(LMouse &m, bool ScreenCoords = false) = 0;

	// Helper
	#if LGI_VIEW_HANDLE
	virtual LViewI *FindControl(OsView hnd) = 0;
	#endif
	virtual LViewI *FindControl(int Id) = 0;
	virtual int64 GetCtrlValue(int Id) = 0;
	virtual void SetCtrlValue(int Id, int64 i) = 0;
	virtual const char *GetCtrlName(int Id) = 0;
	virtual void SetCtrlName(int Id, const char *s) = 0;
	virtual bool GetCtrlEnabled(int Id) = 0;
	virtual void SetCtrlEnabled(int Id, bool Enabled) = 0;
	virtual bool GetCtrlVisible(int Id) = 0;
	virtual void SetCtrlVisible(int Id, bool Visible) = 0;
	virtual bool Pour(LRegion &r) = 0;

	template<class T>
	bool GetViewById(int Id, T *&Ptr)
	{
		LViewI *Ctrl = FindControl(Id);
		Ptr = dynamic_cast<T*>(Ctrl);
		#ifdef _DEBUG
		if (Ctrl != NULL && Ptr == NULL)
			LgiTrace("%s:%i - Can't cast '%s' to '%s' for CtrlId=%i.\n",
				_FL,
				Ctrl->GetClass(),
				type_name<decltype(Ptr)>().c_str(), Id);
		#endif
		return Ptr != NULL;
	}

	// Points
	virtual bool PointToScreen(LPoint &p) = 0;
	virtual bool PointToView(LPoint &p) = 0;
	virtual bool WindowVirtualOffset(LPoint *Offset) = 0;	
	virtual LViewI *WindowFromPoint(int x, int y, int DebugDepth = 0) = 0;
	virtual LPoint &GetWindowBorderSize() = 0;
	virtual bool IsOver(LMouse &m) = 0;

	// Misc
	virtual bool Invalidate(LRect *r = 0, bool Repaint = false, bool NonClient = false) = 0;
	virtual bool Invalidate(LRegion *r, bool Repaint = false, bool NonClient = false) = 0;
	virtual void SetPulse(int Ms = -1) = 0;
	virtual bool OnLayout(LViewLayoutInfo &Inf) = 0;

protected:
	virtual bool OnViewMouse(LView *v, LMouse &m) = 0;
	virtual bool OnViewKey(LView *v, LKey &k) = 0;
};

class LMemoryPoolI
{
public:
	virtual ~LMemoryPoolI() {}

	virtual void *Alloc(size_t Size) = 0;
	virtual void Free(void *Ptr) = 0;
	virtual void Empty() = 0;
};

// File system interface
class LFileSystemI
{
public:
	virtual ~LFileSystemI() {}
	
	virtual void OnDeviceChange(char *Reserved = NULL) = 0;
	virtual LVolume *GetRootVolume() = 0;
	virtual LAutoPtr<LDirectory> GetDir(const char *Path) = 0;
	virtual bool Copy(const char *From, const char *To, LError *Status = NULL, std::function<bool(uint64_t pos, uint64_t total)> callback = NULL) = 0;
	virtual bool Delete(const char *FileName, LError *err = NULL, bool ToTrash = true) = 0;
	virtual bool Delete(LArray<const char*> &Files, LArray<LError> *Status = NULL, bool ToTrash = true) = 0;
	virtual bool CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded = false, LError *Err = NULL) = 0;
	virtual bool RemoveFolder(const char *PathName, bool Recurse = false) = 0;
	virtual LString GetCurrentFolder() = 0;
	virtual bool SetCurrentFolder(const char *PathName) = 0;
	virtual bool Move(const char *OldName, const char *NewName, LError *Err = NULL) = 0;
};

#endif
