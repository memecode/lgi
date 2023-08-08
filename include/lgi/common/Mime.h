#pragma once

#include "lgi/common/Stream.h"

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
	LMime *Mime = NULL;

public:
	virtual void Empty() {} // reset to initial state
};

class LMimeBuf : public LStringPipe
{
	ssize_t Total = 0;
	LStreamI *Src = NULL;
	LStreamEnd *End = NULL;

public:
	constexpr static int BlockSz = 4 << 10;

	LMimeBuf(LStreamI *src, LStreamEnd *end);

	// Read some data from 'src'
	// \returns true if some data was received.
	bool ReadSrc();

	/// Reads 'Len' bytes into a LString
	LString ReadStr(ssize_t Len = -1)
	{
		if (Len < 0)
			Len = GetSize();
		LString s;
		if (!s.Length(Len))
			return s;
		auto rd = Read(s.Get(), s.Length());
		if (rd < 0)
			s.Empty();
		else
			s.Length(rd);
		return s;
	}

	ssize_t Pop(LArray<char> &Buf) override;
	ssize_t Pop(char *Str, ssize_t BufSize) override;
};

class LMime
{
	// Header info
	LString Headers;

	// Data info
	ssize_t DataPos = 0;
	ssize_t DataSize = 0;
	LMutex *DataLock = NULL;
	LStreamI *DataStore = NULL;
	bool OwnDataStore = false;

	// Other info
	char *TmpPath = NULL;
	LMime *Parent = NULL;
	LArray<LMime*> Children;

	// Private methods
	bool Lock();
	void Unlock();
	bool CreateTempData();
	[[deprecated]] char *NewValue(char *&s, bool Alloc = true);
	LString LNewValue(char *&s, bool Alloc = true);
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
	const char *GetHeaders() { return Headers; }
	ssize_t GetLength() { return DataSize; }
	LStreamI *GetData(bool Detach = false);
	bool SetData(bool OwnStream, LStreamI *Input, int RdPos = 0, int RdSize = -1, LMutex *Lock = 0);
	bool SetData(char *Str, int Len);

	// Simple Header Management
	LString LGet(const char *Field, bool Short = true, const char *Default = NULL);
	bool Set(const char *Field, const char *Value); // 'Value' has to include any subfields.
	
	LString LGetSub(const char *Field, const char *Sub);
	bool SetSub(const char *Field, const char *Sub, const char *Value, const char *DefaultValue = 0);
	
	[[deprecated]] char *Get(const char *Field, bool Short = true, const char *Default = NULL); // 'Short'=true returns the value with out subfields
	[[deprecated]] char *GetSub(const char *Field, const char *Sub);

	// Header Shortcuts (uses Get[Sub]/Set[Sub])
	LString LGetMimeType()				{ return LGet("Content-Type", true, "text/plain"); }
	bool SetMimeType(const char *s)		{ return Set("Content-Type", s); }
	
	LString LGetEncoding()				{ return LGet("Content-Transfer-Encoding"); }
	bool SetEncoding(const char *s)		{ return Set("Content-Transfer-Encoding", s); }
	
	LString LGetCharset()				{ return LGetSub("Content-Type", "Charset"); }
	bool SetCharset(const char *s)		{ return SetSub("Content-Type", "Charset", s, DefaultCharset); }
	
	LString LGetBoundary()				{ return LGetSub("Content-Type", "Boundary"); }
	bool SetBoundary(const char *s)		{ return SetSub("Content-Type", "Boundary", s, DefaultCharset); }
	
	LString LGetFileName();
	bool SetFileName(const char *s)		{ return SetSub("Content-Type", "Name", s, DefaultCharset); }

	[[deprecated]] char *GetMimeType();
	[[deprecated]] char *GetEncoding();
	[[deprecated]] char *GetCharset();
	[[deprecated]] char *GetBoundary();
	[[deprecated]] char *GetFileName();

	// Streaming
	class LMimeText
	{
	public:
		class LMimeDecode : public LPullStreamer, public LMimeAction
		{
		public:
			ssize_t Pull(LStreamI *Source, LStreamEnd *End = NULL);
			ssize_t Parse(LMimeBuf *Source, class ParentState *State = NULL);
			void Empty();
		}	Decode;

		class LMimeEncode : public LPushStreamer, public LMimeAction
		{
		public:
			ssize_t Push(LStreamI *Dest, LStreamEnd *End = NULL);
			void Empty();
		}	Encode;

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

