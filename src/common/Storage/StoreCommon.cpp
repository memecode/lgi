#include <stdio.h>

#include "Lgi.h"
#include "StoreCommon.h"

/// \brief This class can limit the reading/writing to a specific sub section of a file
/// Which should protect the application using the storage system from overwriting parts
/// of the file is shouldn't have access to. The default region is the whole file.
LSubFilePtr::LSubFilePtr(LSubFile *Parent, const char *file, int line)
{
	File = Parent;
	SrcFile = NewStr(file);
	SrcLine = line;
	OurStatus = true;
	Pos = 0;
	SetSwap(Parent->GetSwap());
	
	ClearSub();
}

LSubFilePtr::~LSubFilePtr()
{
	File->Detach(this);
	DeleteArray(SrcFile);
}

bool LSubFilePtr::SaveState()
{
	ActualPos = File->GetPos();
	ActualStatus = File->GetStatus();
	ActualSwap = File->GetSwap();
	
	File->SetSwap(GetSwap());
	File->SetStatus(OurStatus);
	
	int64 p = File->SetPos(Start + Pos);
	if (p == Start + Pos)
	{
		return true;
	}
	else
	{
		printf("%s:%i SaveState failed Pos=" LPrintfInt64 " OurPos=" LPrintfInt64 "\n", _FL, Pos, Pos);
	}
	return false;
}

bool LSubFilePtr::RestoreState()
{
	SetSwap(File->GetSwap());
	OurStatus = File->GetStatus();
	Pos = File->GetPos() - Start;

	File->SetStatus(ActualStatus);
	File->SetSwap(ActualSwap);
	return File->SetPos(ActualPos) == ActualPos;
}

bool LSubFilePtr::GetSub(int64 &start, int64 &len)
{
	start = Start;
	len = Len;
	return Sub;
}

bool LSubFilePtr::SetSub(int64 start, int64 len)
{
	Sub = true;
	Start = start;
	Len = len;
	Pos = 0;

	return true;
}

void LSubFilePtr::ClearSub()
{
	Sub = false;
	Start = Len = 0;
}

int LSubFilePtr::Open(const char *Str, int Int)
{
	LAssert(0);
	return 0;
}

bool LSubFilePtr::IsOpen()
{
	return true;
}

int LSubFilePtr::Close()
{
	LAssert(0);
	return 0;
}

int64 LSubFilePtr::GetSize()
{
	int64 s = -1;
	
	if (Sub)
	{
		s = Len;
	}
	else
	{
		LSubFile::SubLock Lock = File->Lock(_FL);
		s = File->GetSize();
	}
	
	return s;
}

int64 LSubFilePtr::SetSize(int64 Size)
{
	LAssert(0);
	return -1;
}

int64 LSubFilePtr::GetPos()
{
	return Pos;
}

int64 LSubFilePtr::SetPos(int64 pos)
{
	if (pos < 0)
	{
		printf("%s:%i - Pos < 0???\n", __FILE__, __LINE__);
	}
	
	return Pos = pos;
}

int64 LSubFilePtr::Seek(int64 To, int Whence)
{
	switch (Whence)
	{
		case SEEK_SET:
			SetPos(To);
			break;
		case SEEK_CUR:
			SetPos(GetPos() + To);
			break;
		case SEEK_END:
			SetPos(GetSize() + To);
			break;
	}

	return GetPos();
}

LStreamI *LSubFilePtr::Clone()
{
	LAssert(0);
	return 0;
}

bool LSubFilePtr::Eof()
{
	bool Status = false;
	
	if (Sub)
	{
		Status = Pos < 0 || Pos >= Len;
	}
	else
	{
		LSubFile::SubLock Lock = File->Lock(_FL);
		Status = File->Eof();
	}
	
	return Status;		
}

bool LSubFilePtr::GetStatus()
{
	return OurStatus;
}

void LSubFilePtr::SetStatus(bool s)
{
	OurStatus = s;
}

ssize_t LSubFilePtr::Read(void *Buffer, ssize_t Size, int Flags)
{
	int Status = 0;
	
	LSubFile::SubLock Lock = File->Lock(_FL);
	if (!Sub || (Pos >= 0 && Pos <= Len))
	{
		if (SaveState())
		{
			int64 Remaining = Len - Pos;
			uint32_t RdSize = (int) (Sub ? MIN(Remaining, Size) : Size);
			Status = File->Read(Buffer, RdSize, Flags);
			RestoreState();
		}
	}
	else
	{
		LAssert(0);
	}
	
	return Status;
}

ssize_t LSubFilePtr::Write(const void *Buffer, ssize_t Size, int Flags)
{
	int Status = 0;
	
	LSubFile::SubLock Lock = File->Lock(_FL);

	int64 End = Pos + Size;
	if (!Sub ||
		(Pos >= 0 && End <= Len))
	{
		if (SaveState())
		{
			Status = File->Write(Buffer, Size, Flags);
			RestoreState();
		}
	}
	else
	{
		LgiTrace("LSubFilePtr error, Pos=" LPrintfInt64 " Start=" LPrintfInt64 " End=" LPrintfInt64 " Len=" LPrintfInt64 "\n",
			Pos, Start, End, Len);
	}
	
	return Status;
}

ssize_t LSubFilePtr::ReadStr(char *Buf, ssize_t Size)
{
	LAssert(0);
	return 0;
}

ssize_t LSubFilePtr::WriteStr(char *Buf, ssize_t Size)
{
	LAssert(0);
	return 0;
}

///////////////////////////////////////////////////////////////////////
LSubFile::LSubFile(LMutex *lock, bool Buffering)
{
	Lck = lock;

	#if GSUBFILE_NOBUFFERING
	Buffer = Buffering;
	Block = 0;
	Buf = 0;
	Shift = 0;
	Cur = -1;
	Pos = -1;
	Dirty = false;
	#endif
}

LSubFile::~LSubFile()
{
	while (Ptrs.Length())
	{
		LSubFilePtr *p = Ptrs[0];
		if (p)
		{
			printf("%s:%i - LSubFilePtr not released.\n", p->SrcFile, p->SrcLine);
			DeleteObj(p);
		}
		else break;
	}

	#if GSUBFILE_NOBUFFERING
	DeleteArray(Buf);
	#endif
}

LSubFilePtr *LSubFile::Create(const char *file, int line)
{
	LSubFilePtr *p = 0;
	
	p = new LSubFilePtr(this, file, line);
	if (p)
	{
		SubLock Lck = Lock(_FL);
		Ptrs[Ptrs.Length()] = p;
	}
	
	return p;
}

void LSubFile::Detach(LSubFilePtr *Ptr)
{
	SubLock Lck = Lock(_FL);
	LAssert(Ptrs.HasItem(Ptr));
	Ptrs.Delete(Ptr);
}

LSubFile::SubLock LSubFile::Lock(const char *file, int line)
{
	return SubLock(new LMutex::Auto(Lck, file, line));
}

#if GSUBFILE_NOBUFFERING
int LSubFile::Open(char *Str, int Int)
{
	int s = LFile::Open(Str, Int | (!Buffer ? O_NO_CACHE : 0));
	if (s && !Buffer)
	{
		Block = GetBlockSize();
		Shift = 1;
		while ((1 << Shift) < Block)
			Shift++;
		Buf = new uint8[Block];
		Pos = 0;
	}
	return s;
}

int LSubFile::Read(void *OutBuf, int Size, int Flags)
{
	int Status = 0;

	if (!Buffer && Buf)
	{
		uint8 *Out = (uint8*)OutBuf;
		int64 Blk = Pos >> Shift;
		int Mask = Block - 1;

		while (Size)
		{
			if (Blk != Cur)
			{
				SetBlock(Blk);
			}

			int Off = Pos & Mask;
			int Len = Block - Off;
			if (Len > Size)
				Len = Size;

			memcpy(Out, Buf + Off, Len);

			Pos += Len;
			Out += Len;
			Size -= Len;
			Status += Len;
			Blk++;
		}
	}
	else
	{
		Status = LFile::Read(OutBuf, Size, Flags);
		if (Status > 0)
		{
			Pos += Status;
		}
	}

	return Status;
}

int LSubFile::Write(const void *InBuf, int Size, int Flags)
{
	int Status = 0;

	if (!Buffer && Buf)
	{
		uint8 *In = (uint8*)InBuf;
		int64 Blk = Pos >> Shift;
		int Mask = Block - 1;

		while (Size)
		{
			if (Blk != Cur)
			{
				SetBlock(Blk);
			}

			int Off = Pos & Mask;
			int Len = Block - Off;
			if (Len > Size)
				Len = Size;

			memcpy(Buf + Off, In, Len);
			Dirty = true;

			Pos += Len;
			In += Len;
			Size -= Len;
			Status += Len;
			Blk++;
		}
	}
	else
	{
		Status = LFile::Write(InBuf, Size, Flags);
		if (Status > 0)
		{
			Pos += Status;
		}
	}

	return Status;
}

int64 LSubFile::GetPos()
{
	return Pos;
}

int64 LSubFile::SetPos(int64 pos)
{
	return Pos = pos;
}

bool LSubFile::SetBlock(int64 Blk)
{
	if (Blk != Cur)
	{
		int64 p, n;

		if (Dirty)
		{
			p = LFile::SetPos(Cur << Shift);
			n = LFile::Write(Buf, Block);
			if (n != Block)
				return false;
			
			Dirty = false;
		}

		Cur = Blk;

		p = LFile::SetPos(Cur << Shift);
		n = LFile::Read(Buf, Block);

		return n > 0;
	}

	return true;
}

#endif
