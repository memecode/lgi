#ifndef _GMIME_H_
#define _GMIME_H_

#include "LgiNetInc.h"
#include "lgi/common/Stream.h"
#include "lgi/common/NetTools.h"

extern void CreateMimeBoundary(char *Buf, int BufLen);

// MIME content types
enum LMimeEncodings
{
	CONTENT_NONE,
	CONTENT_BASE64,
	CONTENT_QUOTED_PRINTABLE,
	CONTENT_OCTET_STREAM
};

class LMime;

class LMimeAction
{
	friend class LMime;

protected:
	// Parent ptr
	LMime *Mime;

public:
	LMimeAction()
	{
		Mime = 0;
	}

	virtual void Empty() {} // reset to initial state
};

class LMimeBuf : public LStringPipe
{
	ssize_t Total;
	LStreamI *Src;
	LStreamEnd *End;

public:
	LMimeBuf(LStreamI *src, LStreamEnd *end);

	ssize_t Pop(LArray<char> &Buf) override;
	ssize_t Pop(char *Str, ssize_t BufSize) override;
};

class LMime
{
	// Header info
	char *Headers;

	// Data info
	ssize_t DataPos;
	ssize_t DataSize;
	LMutex *DataLock;
	LStreamI *DataStore;
	bool OwnDataStore;

	// Other info
	char *TmpPath;
	LMime *Parent;
	LArray<LMime*> Children;

	// Private methods
	bool Lock();
	void Unlock();
	bool CreateTempData();
	char *NewValue(char *&s, bool Alloc = true);
	char *StartOfField(char *s, const char *Feild);
	char *NextField(char *s);
	char *GetTmpPath();

public:
	static const char *DefaultCharset;

	LMime(const char *TmpFileRoot = 0);
	virtual ~LMime();

	// Methods
	bool Insert(LMime *m, int pos = -1);
	void Remove();
	ssize_t Length() { return Children.Length(); }
	LMime *operator[](uint32_t i);
	LMime *NewChild();
	void DeleteChildren() { Children.DeleteObjects(); }

	void Empty();
	bool SetHeaders(const char *h);
	char *GetHeaders() { return Headers; }
	ssize_t GetLength() { return DataSize; }
	LStreamI *GetData(bool Detach = false);
	bool SetData(bool OwnStream, LStreamI *Input, int RdPos = 0, int RdSize = -1, LMutex *Lock = 0);
	bool SetData(char *Str, int Len);

	// Simple Header Management
	char *Get(const char *Field, bool Short = true, const char *Default = 0); // 'Short'=true returns the value with out subfields
	bool Set(const char *Field, const char *Value); // 'Value' has to include any subfields.
	char *GetSub(const char *Field, const char *Sub);
	bool SetSub(const char *Field, const char *Sub, const char *Value, const char *DefaultValue = 0);

	// Header Shortcuts (uses Get[Sub]/Set[Sub])
	char *GetMimeType()				{ return Get("Content-Type", true, "text/plain"); }
	bool SetMimeType(const char *s)	{ return Set("Content-Type", s); }
	char *GetEncoding()				{ return Get("Content-Transfer-Encoding"); }
	bool SetEncoding(const char *s)	{ return Set("Content-Transfer-Encoding", s); }
	char *GetCharset()				{ return GetSub("Content-Type", "Charset"); }
	bool SetCharset(const char *s)	{ return SetSub("Content-Type", "Charset", s, DefaultCharset); }
	char *GetBoundary()				{ return GetSub("Content-Type", "Boundary"); }
	bool SetBoundary(const char *s)	{ return SetSub("Content-Type", "Boundary", s, DefaultCharset); }
	char *GetFileName();
	bool SetFileName(const char *s)	{ return SetSub("Content-Type", "Name", s, DefaultCharset); }

	// Streaming
	class LMimeText
	{
	public:
		class LMimeDecode : public LPullStreamer, public LMimeAction
		{
		public:
			ssize_t Pull(LStreamI *Source, LStreamEnd *End = 0);
			int Parse(LStringPipe *Source, class ParentState *State = 0);
			void Empty();
		} Decode;

		class LMimeEncode : public LPushStreamer, public LMimeAction
		{
		public:
			ssize_t Push(LStreamI *Dest, LStreamEnd *End = 0);
			void Empty();
		} Encode;

	} Text;

	friend class LMime::LMimeText::LMimeDecode;
	friend class LMime::LMimeText::LMimeEncode;

	class LMimeBinary
	{
	public:
		class LMimeRead : public LPullStreamer, public LMimeAction
		{
		public:
			ssize_t Pull(LStreamI *Source, LStreamEnd *End = 0);
			void Empty();
		} Read;

		class LMimeWrite : public LPushStreamer, public LMimeAction
		{
		public:
			int64 GetSize();
			ssize_t Push(LStreamI *Dest, LStreamEnd *End = 0);
			void Empty();
		} Write;

	} Binary;

	friend class LMime::LMimeBinary::LMimeRead;
	friend class LMime::LMimeBinary::LMimeWrite;
};

#endif
