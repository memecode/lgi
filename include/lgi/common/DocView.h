/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief This is the base data and code for all the text controls (inc. HTML)

#pragma once

#include <functional>

#include "lgi/common/Variant.h"
#include "lgi/common/Notifications.h"
#include "lgi/common/Thread.h"
#include "lgi/common/Layout.h"

// Word wrap

enum LDocWrapType
{
	/// No word wrapping
	TEXTED_WRAP_NONE = 0,
	/// Dynamically wrap line to editor width
	TEXTED_WRAP_REFLOW = 1,
};

// Util macros

/// Returns true if 'c' is whitespace
#define IsWhiteSpace(c)				((c) < 126 && strchr(LDocView::WhiteSpace, c) != 0)
/// Returns true if 'c' is a delimiter
#define IsDelimiter(c)				((c) < 126 && strchr(LDocView::Delimiters, c) != 0)
/// Returns true if 'c' is a letter or number
#define IsText(c)					(IsDigit(c) || IsAlpha(c) || (c) == '_')
/// Returns true if 'c' is word boundry
#define IsWordBoundry(c)			(strchr(LDocView::WhiteSpace, c) || strchr(LDocView::Delimiters, c))
/// Returns true if 'c' is alphanumeric or a digit
#define AlphaOrDigit(c)				(IsDigit(c) || IsAlpha(c))
/// Returns true if 'c' is a valid URL character
#define UrlChar(c)					( \
										strchr(LDocView::UrlDelim, (c)) || \
										AlphaOrDigit((c)) || \
										((c) >= 256) \
									)
/// Returns true if 'c' is email address character
#define EmailChar(c)				(strchr("._-:+", (c)) || AlphaOrDigit((c)))

LgiFunc char16 *ConvertToCrLf(char16 *Text);

/// This class contains information about a link.
/// \sa LDetectLinks
struct LLinkInfo
{
	ssize_t Start;
	ssize_t Len;
	bool Email;
	
	void Set(ssize_t start, ssize_t len, bool email)
	{
		Start = start;
		Len = len;
		Email = email;
	}
};

// Call back class to handle viewer events
class LDocView;

/// An environment class to handle requests from the text view to the outside world.
class LgiClass
LDocumentEnv : public LThreadOwner
{
	LArray<LDocView*> Viewers;

public:
	LDocumentEnv(LDocView *v = 0);
	virtual ~LDocumentEnv();

	enum LoadType
	{
		LoadError,
		LoadNotImpl,
		LoadImmediate,
		LoadDeferred,
	};

	struct
	#ifdef MAC
	LgiClass
	#endif
	LoadJob : public LThreadJob
	{
		enum PrefFormat
		{
			FmtNone,
			FmtStream,
			FmtSurface,
			FmtFilename,
		};
		
		enum JobStatus
		{
			JobInit,
			JobOk,
			JobErr_Uri,
			JobErr_Path,
			JobErr_FileOpen,
			JobErr_GetUri,
			JobErr_NoCachedFile,
			JobErr_ImageFilter,
			JobErr_NoMem,
		};
		
		// View data
		LDocumentEnv *Env;
		void *UserData;
		uint32_t UserUid;
		PrefFormat Pref;

		// Input data
		LAutoString Uri;
		LAutoString PostData;

		// Output data
		LAutoPtr<LStreamI> Stream;
		LAutoPtr<LSurface> pDC;
		LString Filename;
		LString Error;
		JobStatus Status;
		LString MimeType, ContentId;

		LoadJob(LThreadTarget *o) : LThreadJob(o)
		{
			Env = NULL;
			UserUid = 0;
			UserData = NULL;
			Pref = FmtNone;
			Status = JobInit;
		}
		
		LStreamI *GetStream()
		{
			if (!Stream && Filename)
			{
				LFile *file = new LFile;
				if (file && file->Open(Filename, O_READ))
					Stream.Reset(file);
				else
					DeleteObj(file);
			}
			
			return Stream;
		}
	};

	LoadJob *NewJob()
	{
		return new LoadJob(this);
	}

	bool AttachView(LDocView *v)
	{
		if (!v)
			return false;
		if (!Lock(_FL))
			return false;
		LAssert(!Viewers.HasItem(v));
		Viewers.Add(v);
		Unlock();
		return true;
	}

	bool DetachView(LDocView *v)
	{
		if (!v)
			return false;
		if (!Lock(_FL))
			return false;
		LAssert(Viewers.HasItem(v));
		Viewers.Delete(v);
		Unlock();
		return true;
	}
	
	int NextUid();

	/// Creating a context menu, usually when the user right clicks on the 
	/// document.
	virtual bool AppendItems(LSubMenu *Menu, const char *Param, int Base = 1000) { return false; }
	
	/// Do something when the menu items created by LDocumentEnv::AppendItems 
	/// are clicked.
	virtual bool OnMenu(LDocView *View, int Id, void *Context) { return false; }
	
	/// Asks the env to get some data linked from the document, e.g. a css file or an iframe source etc.
	/// If the GetContent implementation takes ownership of the job pointer then it should set 'j' to NULL.
	virtual LoadType GetContent(LoadJob *&j) { return LoadNotImpl; }
	/// After the env's thread loads the resource it calls this to pass it to the doc
	void OnDone(LAutoPtr<LThreadJob> j);
	
	/// Handle a click on URI
	virtual bool OnNavigate(LDocView *Parent, const char *Uri) { return false; }
	/// Handle a form post
	virtual bool OnPostForm(LDocView *Parent, const char *Uri, const char *Data) { return false; }

	/// Process dynamic content, returning a dynamically allocated string
	/// for the result of the executed script. Dynamic content is enclosed
	/// between &lt;? and ?&gt;.
	virtual LString OnDynamicContent(LDocView *Parent, const char *Code) { return LString(); }

	/// Some script was received, the owner should compile it
	virtual bool OnCompileScript(LDocView *Parent, char *Script, const char *Language, const char *MimeType) { return false; }

	/// Some script needs to be executed, the owner should compile it
	virtual bool OnExecuteScript(LDocView *Parent, char *Script) { return false; }
};

/// Default text view environment
///
/// This class defines the default behavior of the environment,
/// However you will need to instantiate this yourself and call
/// SetEnv with your instance. i.e. it's not automatic.
class LgiClass LDefaultDocumentEnv :
	public LDocumentEnv
{
public:
	LoadType GetContent(LoadJob *&j);
	bool OnNavigate(LDocView *Parent, const char *Uri);
};

/// Find params
class LDocFindReplaceParams
{
public:
	virtual ~LDocFindReplaceParams() {}
};

/// TextView class is a base for all text controls
class LgiClass LDocView :
	public LLayout,
	virtual public LDom
{
	friend class LDocumentEnv;

protected:
	LDocumentEnv *Environment = NULL;
	LString Charset;

public:
	// Static
	static const char *WhiteSpace;
	static const char *Delimiters;
	static const char *UrlDelim;

	///////////////////////////////////////////////////////////////////////
	// Properties
	#define _TvMenuProp(Type, Name, Default)			\
	protected:											\
		Type Name = Default;							\
	public:												\
		virtual void Set##Name(Type i) { Name=i; }		\
		Type Get##Name() { return Name; }

	_TvMenuProp(uint16, WrapAtCol, 0)
	_TvMenuProp(bool, UrlDetect, true)
	_TvMenuProp(bool, ReadOnly, false)
	_TvMenuProp(LDocWrapType, WrapType, TEXTED_WRAP_REFLOW)
	_TvMenuProp(uint8_t, TabSize, 4)
	_TvMenuProp(uint8_t, IndentSize, 4)
	_TvMenuProp(bool, HardTabs, true)
	_TvMenuProp(bool, ShowWhiteSpace, false)
	_TvMenuProp(bool, ObscurePassword, false)
	_TvMenuProp(bool, CrLf, false)
	_TvMenuProp(bool, AutoIndent, true)
	_TvMenuProp(bool, FixedWidthFont, false)
	_TvMenuProp(bool, LoadImages, false)
	_TvMenuProp(bool, OverideDocCharset, false)

	// This UID is used to match data load events with their source document.
	// Sometimes data will arrive after the document that asked for it has
	// already been unloaded. So by assigned each document an UID we can check
	// the job UID against it and discard old data.
	_TvMenuProp(int, DocumentUid, 0)
	#undef _TvMenuProp

	virtual const char *GetCharset()
	{
		return Charset.Get() ? Charset.Get() : "utf-8";
	}
	
	virtual void SetCharset(const char *s)
	{
		Charset = s;
	}
	
	virtual const char *GetMimeType() = 0;

	///////////////////////////////////////////////////////////////////////
	// Object
	LDocView(LDocumentEnv *e = NULL)
	{
		SetEnv(e);
	}

	virtual ~LDocView()
	{
		SetEnv(NULL);
	}

	const char *GetClass() { return "LDocView"; }

	/// Open a file handler
	virtual bool Open(const char *Name, const char *Cs = 0) { return false; }
	/// Save a file handler
	virtual bool Save(const char *Name, const char *Cs = 0) { return false; }

	///////////////////////////////////////////////////////////////////////

	/// Find window handler
	virtual void DoFind(std::function<void(bool)> Callback) { if (Callback) Callback(false); }
	/// Replace window handler
	virtual void DoReplace(std::function<void(bool)> Callback) { if (Callback) Callback(false); }
	virtual LDocFindReplaceParams *CreateFindReplaceParams() { return 0; }
	virtual void SetFindReplaceParams(LDocFindReplaceParams *Params) { }

	///////////////////////////////////////////////////////////////////////

	/// Get the current environment
	virtual LDocumentEnv *GetEnv() { return Environment; }
	/// Set the current environment
	virtual void SetEnv(LDocumentEnv *e)
	{
		if (Environment) Environment->DetachView(this);
		Environment = e;
		if (Environment) Environment->AttachView(this);
	}
	/// When the env has loaded a resource it can pass it to the doc control via this method.
	/// It MUST be thread safe. Often an environment will call this function directly from
	/// it's worker thread.
	virtual void OnContent(LDocumentEnv::LoadJob *Res) {}
	
	///////////////////////////////////////////////////////////////////////
	// State / Selection
	
	/// Set the cursor position, to select an area, move the cursor with Select=false
	/// then set the other end of the region with Select=true.
	virtual void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) {}

	/// Cursor=false means the other end of the selection if any. The cursor is alwasy
	/// at one end of the selection.
	virtual ssize_t GetCaret(bool Cursor = true) { return 0; }

	/// True if there is a selection
	virtual bool HasSelection() { return false; }
	/// Unselect all the text
	virtual void UnSelectAll() {}
	/// Select the word from index 'From'
	virtual void SelectWord(size_t From) {}
	/// Select all the text in the control
	virtual void SelectAll() {}
	/// Get the selection as a dynamicially allocated utf-8 string
	virtual char *GetSelection() { return 0; }

	/// Returns the character index at the x,y location
	virtual ssize_t IndexAt(int x, int y) { return 0; }

	/// Index=-1 returns the x,y of the cursor, Index >=0 returns the specified x,y
	virtual bool GetLineColumnAtIndex(LPoint &Pt, ssize_t Index = -1) { return false; }

	/// True if the document has changed
	virtual bool IsDirty() { return false; }
	/// Gets the number of lines of text
	virtual size_t GetLines() { return 0; }
	/// Gets the pixels required to display all the text
	virtual void GetTextExtent(int &x, int &y) {}

	///////////////////////////////////////////////////////////////////////
	
	/// Cuts the selection from the document and puts it on the clipboard
	virtual bool Cut() { return false; }
	/// Copies the selection from the document to the clipboard
	virtual bool Copy() { return false; }
	/// Pastes the current contents of the clipboard into the document
	virtual bool Paste() { return false; }

	///////////////////////////////////////////////////////////////////////

	/// Called when the user hits the escape key
	virtual void OnEscape(LKey &K) {}
	/// Called when the user hits the enter key
	virtual void OnEnter(LKey &k) {}
	/// Called when the user clicks a URL
	virtual void OnUrl(char *Url) {}
	/// Called to add styling to the document
	virtual void OnAddStyle(const char *MimeType, const char *Styles) {}

	///////////////////////////////////////////////////////////////////////

	struct ContentMedia
	{
		LString Id;
		LString FileName;
		LString MimeType;
		LVariant Data;
		LAutoPtr<LStreamI> Stream;

		bool Valid()
		{
			return MimeType.Get() != NULL &&
				FileName.Get() != NULL &&
				(
					(Data.Type == GV_BINARY && Data.Value.Binary.Data != NULL)
					||
					(Stream.Get() != NULL)
				);
		}
	};

	/// Gets the document in format of a desired MIME type
	virtual bool GetFormattedContent
	(
		/// [In] The desired mime type of the content
		const char *MimeType,
		/// [Out] The content in the specified mime type
		LString &Out,
		/// [Out/Optional] Any attached media files that the content references
		LArray<ContentMedia> *Media = NULL
	)
	{ return false; }
};

/// Detects links in text, returning their location and type
template<typename T>
bool LDetectLinks(LArray<LLinkInfo> &Links, T *Text, ssize_t TextCharLen = -1)
{
	if (!Text)
		return false;

	if (TextCharLen < 0)
		TextCharLen = Strlen(Text);

	T *End = Text + TextCharLen;
	static T Http[] = {'h', 't', 't', 'p', ':', '/', '/', 0 };
	static T Https[] = {'h', 't', 't', 'p', 's', ':', '/', '/', 0};

	for (int64 i=0; i<TextCharLen; i++)
	{
		switch (Text[i])
		{
			case 'h':
			case 'H':
			{
				int64 Remaining = TextCharLen - i;
				if
				(
					Remaining >= 7
					&&
					(
						Strnicmp(Text+i, Http, 6) == 0 ||
						Strnicmp(Text+i, Https, 7) == 0
					)
				)
				{
					// find end
					T *s = Text + i;
					T *e = s + 6;
					for ( ; e < End && UrlChar(*e); e++)
						;
					
					while
					(
						e > s &&
						!
						(
							IsAlpha(e[-1]) ||
							IsDigit(e[-1]) ||
							e[-1] == '/'
						)
					)
						e--;

					Links.New().Set(s - Text, e - s, false);
					i = e - Text;
				}
				break;
			}
			case '@':
			{
				// find start
				T *s = Text + (MAX(i, 1) - 1);
				
				for ( ; s > Text && EmailChar(*s); s--)
					;

				if (s < Text + i)
				{
					if (!EmailChar(*s))
						s++;

					bool FoundDot = false;
					T *Start = Text + i + 1;
					T *e = Start;
					for	(	;
							e < End && EmailChar(*e);
							e++)
					{
						if (*e == '.')
							FoundDot = true;
					}

					while (e > Start && e[-1] == '.')
						e--;

					if (FoundDot)
					{
						Links.New().Set(s - Text, e - s, true);
						i = e - Text;
					}
				}
				break;
			}
		}
	}

	return true;
}

