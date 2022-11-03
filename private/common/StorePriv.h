#ifndef __STORE_PRIV_H
#define __STORE_PRIV_H

#include "StoreCommon.h"

class StorageItem;
class StorageObj;
class StorageKit;

enum NodeRelation
{
	NodeChild,
	NodeNext,
	NodePrev,
};

#define GSUBFILE_NOBUFFERING		0

class LSubFilePtr;
class LSubFile : public LFile
{
	GSemaphore *Lck;
	LArray<LSubFilePtr*> Ptrs;
	
	#if GSUBFILE_NOBUFFERING
	bool Buffer;
	uint8 *Buf;
	int Block;
	int Shift;
	int64 Cur;
	int64 Pos;
	bool Dirty;

	bool SetBlock(int64 b);
	#endif
	
public:
	LSubFile(GSemaphore *lock, bool Buffering = true);
	~LSubFile();

	LSubFilePtr *Create(char *file, int line);
	void Detach(LSubFilePtr *Ptr);
	bool Lock();
	bool LockWithTimeout(int Timeout /* in ms */);
	void Unlock();

	#if GSUBFILE_NOBUFFERING
	int Open(char *Str = 0, int Int = 0);
	int Read(void *Buffer, int Size, int Flags = 0);
	int Write(const void *Buffer, int Size, int Flags = 0);
	int64 GetPos();
	int64 SetPos(int64 pos);
	#endif
};

class LSubFilePtr : public LFile
{
	friend class LSubFile;
	LSubFile *File;

	// Sub file bound box
	bool Sub;
	int64 Start;
	int64 Len;
	
	// Allocation point
	char *SrcFile;
	int SrcLine;
	
	// Our state
	int64 OurPos;
	bool OurStatus;
	
	// Actual state
	int64 ActualPos;
	bool ActualStatus;
	bool ActualSwap;
	
	bool SaveState();
	bool RestoreState();
	
public:
	LSubFilePtr(LSubFile *Parent, char *file, int line);
	~LSubFilePtr();

	/// Sets the valid section of the file.
	void SetSub
	(
		/// The start of the valid region as an offset from the start of the file.
		int start,
		/// The length of the valid region.
		int len
	);

	/// Removes the sub region so you can access the entire file.
	void ClearSub();
	
	// LFile stuff
	int Open(char *Str = 0, int Int = 0);
	bool IsOpen();
	int Close();
	int64 GetSize();
	int64 SetSize(int64 Size);
	int64 GetPos();
	int64 SetPos(int64 pos);
	int64 Seek(int64 To, int Whence);
	LStreamI *Clone();
	bool Eof();
	bool GetStatus();
	void SetStatus(bool s);
	int Read(void *Buffer, int Size, int Flags = 0);
	int Write(const void *Buffer, int Size, int Flags = 0);
	int ReadStr(char *Buf, int Size);
	int WriteStr(char *Buf, int Size);
	int Print(char *Format, ...);
};

#endif
