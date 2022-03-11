#ifndef _OPTION_FILE_H_
#define _OPTION_FILE_H_

#include "lgi/common/Variant.h"
#include "lgi/common/XmlTree.h"
#include "lgi/common/Mutex.h"

class LgiClass LOptionsFile : public LXmlTag, public LMutex
{
public:
	enum PortableType
	{
		UnknownMode,
		PortableMode,
		DesktopMode,
	};

private:
	LString File;
	LString Error;
	bool Dirty;
	PortableType Mode;
	
	char *LockFile;
	int LockLine;

	void _Init();
	bool _OnAccess(bool Start);

	// Don't allow non-threadsafe access to these
	char *GetAttr(const char *Name) { return LXmlTag::GetAttr(Name); }
	int GetAsInt(const char *Name) { return LXmlTag::GetAsInt(Name); }
	bool SetAttr(const char *Name, char *Value) { return LXmlTag::SetAttr(Name, Value); }
	bool SetAttr(const char *Name, int Value) { return LXmlTag::SetAttr(Name, Value); }
	bool SetAttr(const char *Name, int64 Value) { return LXmlTag::SetAttr(Name, Value); }
	bool DelAttr(const char *Name) { return LXmlTag::DelAttr(Name); }
	void InsertTag(LXmlTag *t) { LXmlTag::InsertTag(t); }
	bool RemoveTag() { return LXmlTag::RemoveTag(); }
	bool Serialize(bool Write) { return false; }
	LXmlTag *GetChildTag(const char *Name, bool Create = false) { return LXmlTag::GetChildTag(Name, Create); }

protected:
	virtual void _Defaults() {}

public:
	LOptionsFile(const char *FileName = NULL);
	LOptionsFile(PortableType mode, const char *BaseName = NULL);
	~LOptionsFile();

	void SetFile(const char *f);
	PortableType GuessMode();
	PortableType GetMode() { return Mode; }
	bool SetMode(PortableType mode, const char *BaseName = NULL);

	bool IsValid();
	char *GetFile() { return File; }
	char *GetError() { return Error; }
	bool SerializeFile(bool Write);
	bool DeleteValue(const char *Name);

	bool CreateTag(const char *Name);
	bool DeleteTag(const char *Name);
	
	/** This call locks the object and returns the tag if available.
	 If the return value is not NULL you have to Unlock the object
	 when you're done with the tag. e.g:
	 
		LXmlTag *t = Options.LockTag("MyTagName");
		if (t)
		{
			// Doing something with tag here...
			Options.Unlock();
		}
	*/
	LXmlTag *LockTag(const char *Name, const char *File, int Line);
};

#endif