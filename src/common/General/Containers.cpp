/**
 * \file
 * \author Matthew Allen
 * \date 27/7/1995
 * \brief Container classes.\n
	Copyright (C) 1995-2003, <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifdef LINUX
#define _ISOC99_SOURCE		1
#include <wchar.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "lgi/common/Mem.h"
#include "lgi/common/Containers.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/LgiCommon.h"


/////////////////////////////////////////////////////////////////////////////////////////////////
int ACmp(LString *a, LString *b)
{
	return stricmp(*a, *b);
}

int LCmp(char *a, char *b, NativeInt d)
{
	return stricmp(a, b);
}

void UnitTest_CreateList(LArray<LString> &a, List<char> &l, int sz = 100)
{
	a.Empty();
	for (int i=0; i<sz; i++)
		a[i].Printf("obj.%i", i);

	l.Empty();
	for (int i=0; i<sz; i++)
		l.Add(a[i]);
}

bool UnitTest_Err(const char *Fmt, ...)
{
	printf("Error: %s\n", Fmt);
	LAssert(0);
	return false;
}

bool UnitTest_Check(LArray<LString> &a, List<char> &l)
{
	if (a.Length() != l.Length())
		return UnitTest_Err("Wrong size.");
	int n = 0;
	for (auto s : l)
	{
		LString t = s;
		if (t.Equals(a[n]))
			;//printf("%i: %s\n", n, s);
		else
			return UnitTest_Err("Wrong value.");
		n++;
	}
	return true;
}

#define CHECK(val) if (!(val)) return false

bool UnitTest_ListClass()
{
	LArray<LString> a;
	List<char> l;
	
	// Create test..
	UnitTest_CreateList(a, l, 100);
	CHECK(UnitTest_Check(a, l));
	
	// Delete tests..
	l.Delete(a[3].Get());
	a.DeleteAt(3, true);
	CHECK(UnitTest_Check(a, l));
	
	while (a.Length() > 60)
	{
		a.DeleteAt(60, true);
		l.DeleteAt(60);
	}
	CHECK(UnitTest_Check(a, l));

	// Sort test..
	a.Sort([](auto a, auto b)
	{
		return stricmp(*a, *b);
	});
	l.Sort(LCmp);
	CHECK(UnitTest_Check(a, l));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
LMemQueue::LMemQueue(size_t prealloc)
{
	PreAlloc = prealloc;
}

LMemQueue::~LMemQueue()
{
	Empty();
}

LMemQueue &LMemQueue::operator=(LMemQueue &p)
{
	Empty();

	PreAlloc = p.PreAlloc;
	for (auto b: p.Mem)
	{
		int Alloc = sizeof(Block) + b->Size;
		Alloc = LGI_ALLOC_ALIGN(Alloc);
		Block *n = (Block*) malloc(Alloc);
		if (n)
		{
			memcpy(n, b, sizeof(Block) + b->Size);
			Mem.Insert(n);
		}
		else break;		
	}

	return *this;
}

void LMemQueue::Empty()
{
	for (auto b: Mem)
		free(b);
	Mem.Empty();
}

int64 LMemQueue::GetSize()
{
	int Size = 0;
	for (auto b: Mem)
		Size += b->Used - b->Next;
	return Size;
}

int64 LMemQueue::Peek(uchar *Ptr, int Size)
{
	int64 Status = 0;

	if (Ptr && Size <= GetSize())
	{
		for (auto b: Mem)
		{
			if (Size <= 0)
				break;
			int Copy = MIN(Size, b->Size - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				Ptr += Copy;
				Size -= Copy;
				Status += Copy;
			}
		}
	}

	return Status;
}

void *LMemQueue::New(int AddBytes)
{
	int64 Len = GetSize();
	uchar *Data = Len > 0 ? new uchar[Len+AddBytes] : 0;
	if (Data)
	{
		ssize_t Rd = Read(Data, Len);
		if (Rd >= 0 && AddBytes)
		{
			memset(Data+Len, 0, AddBytes);
		}
	}

	return Data;
}

ssize_t LMemQueue::Read(void *Ptr, ssize_t Size, int Flags)
{
	int Status = 0;

	if (Ptr && Size > 0)
	{
		for (auto b: Mem)
		{
			if (Size <= 0)
				break;

			int Copy = (int) MIN(Size, b->Used - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				((uchar*&)Ptr) += Copy;
				Size -= Copy;
				b->Next += Copy;
				Status += Copy;
			}
		}

		while (Mem.Length() > 0)
		{
			auto it = Mem.begin();
			auto b = *it;
			if (b->Next < b->Used)
				break;
			Mem.Delete(b);
			free(b);
		}
	}

	return Status;
}

ssize_t LMemQueue::Write(const void *Ptr, ssize_t Size, int Flags)
{
	ssize_t Status = 0;

	if (Ptr && Size > 0)
	{
		if (PreAlloc > 0)
		{
			auto it = Mem.rbegin();
			Block *Last = *it;
			if (Last)
			{
				int Len = (int) MIN(Size, Last->Size - Last->Used);
				if (Len > 0)
				{
					memcpy(Last->Ptr() + Last->Used, Ptr, Len);
					Last->Used += Len;
					Size -= Len;
					((uchar*&)Ptr) += Len;
					Status += Len;
				}
			}
		}

		if (Size > 0)
		{
			ssize_t Bytes = MAX(PreAlloc, Size);
			ssize_t Alloc = sizeof(Block) + Bytes;
			Alloc = LGI_ALLOC_ALIGN(Alloc);
			Block *b = (Block*) malloc(Alloc);
			if (b)
			{
				void *p = b->Ptr();
				memcpy(p, Ptr, Size);
				b->Size = (int)Bytes;
				b->Used = (int)Size;
				b->Next = 0;
				Mem.Insert(b);
				Status += Size;
			}
		}
	}

	return Status;
}

ssize_t LMemQueue::Find(LString str, bool caseSensitive)
{
	if (!str.Get())
		return -1;

	if (!caseSensitive)
		str = str.Lower();

	ssize_t pos = 0;
	char *s = str.Get();
	ssize_t si = 0;
	for (auto b: Mem)
	{
		auto p = b->Ptr() + b->Next;
		auto e = b->Ptr() + b->Used;
		while (p < e)
		{
			if
			(
				(caseSensitive && (s[si] == *p))
				||
				(!caseSensitive && (s[si] == ToLower(*p)))
			)
			{
				si++;
				if (si == str.Length())
					return pos - si + 1;
			}
			else si = 0;

			p++;
			pos++;
		}
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
LString LStringPipe::NewGStr()
{
	LString s;
	
	int64 Sz = GetSize();
	if (Sz > 0)
	{
		if (s.Length(Sz))
		{
			ssize_t Rd = Read(s.Get(), s.Length());
			if (Rd > 0 && Rd <= Sz)
			{
				s.Get()[Rd] = 0;
			}
		}
		else s.Empty();
	}
	
	return s;
}

ssize_t LStringPipe::LineChars()
{
	ssize_t Len = 0;

	for (auto m: Mem)
	{
		uint8_t *p = m->Ptr();

		for (int i = m->Next; i < m->Used; i++)
		{
			if (p[i] == '\n')
				return Len;
			Len++;
		}
	}

	return -1;
}

ssize_t LStringPipe::SaveToBuffer(char *Start, ssize_t BufSize, ssize_t Chars)
{
	char *Str = Start;
	char *End = Str + BufSize; // Not including NULL

	auto it = Mem.begin();
	Block *m = *it;
	while (m)
	{
		for (	char *MPtr = (char*)m->Ptr();
				m->Next < m->Used;
				m->Next++)
		{
			if (Str < End)
				*Str++ = MPtr[m->Next];
			if (MPtr[m->Next] == '\n')
			{
				m->Next++;
				goto EndPop;
			}
		}

		if (m->Next >= m->Used)
		{
			Mem.Delete(it);
			free(m);
			it = Mem.begin();
			m = *it;
		}
	}

	EndPop:
	*Str = 0;

	return Str - Start;
}

ssize_t LStringPipe::Pop(LArray<char> &Buf)
{
	ssize_t Chars = LineChars();
	if (Chars > 0)
	{
		Chars++; // For the '\n'
		if ((ssize_t)Buf.Length() < Chars + 1)
			if (!Buf.Length(Chars + 1))
				return -1;
		SaveToBuffer(Buf.AddressOf(), Chars, Chars);
	}
	else return 0;

	return Chars;
}

LString LStringPipe::Pop()
{
	LString s;
	ssize_t Chars = LineChars();
	if (Chars > 0 &&
		s.Length(Chars))
	{
		SaveToBuffer(s.Get(), Chars, Chars);
	}

	return s;
}

ssize_t LStringPipe::Pop(char *Str, ssize_t BufSize)
{
	if (!Str)
		return 0;

	ssize_t Chars = LineChars();
	if (Chars < 0)
		return 0;

	return SaveToBuffer(Str, BufSize-1 /* for the NULL */, Chars);
}

ssize_t LStringPipe::Push(const char *Str, ssize_t Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = strlen(Str);

	return Write((void*)Str, Chars);
}

ssize_t LStringPipe::Push(const char16 *Str, ssize_t Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = StrlenW(Str);

	return Write((void*)Str, Chars * sizeof(char16));
}

#ifdef _DEBUG
bool LStringPipe::UnitTest()
{
	char Buf[16];
	memset(Buf, 0x1, sizeof(Buf));
	LStringPipe p(8);
	const char s[] = "1234567890abc\n"
					"abcdefghijklmn\n";
	p.Write(s, sizeof(s)-1);
	ssize_t rd = p.Pop(Buf, 10);
	int cmp = memcmp(Buf, "123456789\x00\x01\x01\x01\x01\x01\x01", 16);
	if (cmp)
		return false;
	rd = p.Pop(Buf, 10);
	cmp = memcmp(Buf, "abcdefghi\x00\x01\x01\x01\x01\x01\x01", 16);
	if (cmp)
		return false;

	p.Empty();
	p.Write(s, sizeof(s)-1);
	LString r;
	r = p.Pop();
	if (!r.Equals("1234567890abc"))
		return false;
	r = p.Pop();
	if (!r.Equals("abcdefghijklmn"))
		return false;
	return true;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
GMemFile::GMemFile(int BlkSize)
{
	CurPos = 0;
	Blocks = 0;
	BlockSize = BlkSize < 16 ? 16 : BlkSize;
	ZeroObj(Local);
}

GMemFile::~GMemFile()
{
	Empty();
}

GMemFile::Block *GMemFile::Get(int Index)
{
	if (Blocks == 0 || Index < 0)
		return NULL;

	if (Index < GMEMFILE_BLOCKS)
		return Local[Index];
	
	if (Index - GMEMFILE_BLOCKS >= Extra.Length())
	{
		LAssert(!"Index beyond last block");
		return NULL;
	}

	return Extra[Index - GMEMFILE_BLOCKS];
}

GMemFile::Block *GMemFile::Create()
{
	int Alloc = sizeof(Block) + BlockSize - 1;
	Alloc = LGI_ALLOC_ALIGN(Alloc);
	Block *b = (Block*) malloc(Alloc);
	if (!b)
		return NULL;
	
	b->Used = 0;
	b->Offset = Blocks * BlockSize;
	
	if (Blocks < GMEMFILE_BLOCKS)
		Local[Blocks] = b;
	else
		Extra.Add(b);

	Blocks++;
	return b;
}

void GMemFile::Empty()
{
	CurPos = 0;
	for (int i=0; i<GMEMFILE_BLOCKS; i++)
		free(Local[i]);
	for (int i=0; i<Extra.Length(); i++)
		free(Extra[i]);
}

int64 GMemFile::GetSize()
{
	#ifndef __llvm__
	if (this == NULL)
		return -1;
	#endif
	if (Blocks == 0)
		return 0;

	Block *b = GetLast();
	int64 Sz = (Blocks - 1) * BlockSize;
	return Sz + b->Used;	
}

bool GMemFile::FreeBlock(Block *b)
{
	if (!b)
		return false;
	
	ssize_t Idx = b->Offset / BlockSize;
	if (Idx < GMEMFILE_BLOCKS)
	{
		// Local block
		if (Local[Idx] != b ||
			Idx < Blocks-1)
		{
			LAssert(!"Block index error.");
			return false;
		}
		
		free(b);
		Local[Idx] = NULL;		
		Blocks--;
		return true;
	}

	// Extra block
	ssize_t Off = Idx - GMEMFILE_BLOCKS;
	if (Off != Extra.Length() - 1)
	{
		LAssert(!"Block index error.");
		return false;
	}
	
	free(b);
	Extra.DeleteAt(Off, true);
	Blocks--;
	return true;
}

int64 GMemFile::SetSize(int64 Size)
{
	if (Size <= 0)
	{
		Empty();
	}
	else
	{
		int64 CurSize = GetSize();
		if (Size > CurSize)
		{
			// Increase size...
			int64 Diff = Size - CurSize;
			Block *b = GetLast();
			if (b->Used < BlockSize)
			{
				// Add size to last incomplete block
				ssize_t Remaining = BlockSize - b->Used;
				ssize_t Add = MIN(Diff, Remaining);
				b->Used += Add;
				Diff -= Add;
			}
			while (Diff > 0)
			{
				// Add new blocks to cover the size...
				ssize_t Add = MIN(BlockSize, Diff);
				b = Create();
				b->Used = Add;
				Diff -= Add;
			}
		}
		else
		{
			// Decrease size...
			uint64 Diff = CurSize - Size;
			while (Diff > 0 && Blocks > 0)
			{
				Block *b = GetLast();
				if (!b)
					break;

				ssize_t Sub = MIN(b->Used, Diff);
				b->Used -= Sub;
				Diff -= Sub;
				if (b->Used == 0)
					FreeBlock(b);
			}
		}
	}
	
	return GetSize();
}

int64 GMemFile::GetPos()
{
	return CurPos;
}

int64 GMemFile::SetPos(int64 Pos)
{
	if (Pos <= 0)
		return CurPos = 0; // Off the start of the structure
	
	ssize_t BlockIndex = Pos / BlockSize;
	if (BlockIndex >= Blocks)
		return CurPos = GetSize(); // Off the end of the structure
	
	if (BlockIndex >= 0 && BlockIndex < Blocks - 1)
		return CurPos = Pos; // Inside a full block
	
	Block *Last = GetLast();
	uint64 Offset = Pos - Last->Offset;
	if (Offset >= Last->Used)
		return CurPos = Last->Offset + Last->Used; // End of last block
	
	return CurPos = Pos; // Inside the last block
}

ssize_t GMemFile::Read(void *Ptr, ssize_t Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;
	
	uint8_t *p = (uint8_t*) Ptr;
	uint8_t *end = p + Size;
	while (p < end)
	{
		int Cur = CurBlock();
		if (Cur >= Blocks)
			break;
		Block *b = Get(Cur);
		
		// Where are we in the current block?
		ssize_t BlkOffset = CurPos - b->Offset;
		LAssert(b && BlkOffset >= 0 && BlkOffset <= (ssize_t)b->Used);
		ssize_t Remaining = b->Used - BlkOffset;
		if (Remaining > 0)
		{
			ssize_t Common = MIN(Remaining, end - p);
			memcpy(p, b->Data + BlkOffset, Common);
			CurPos += Common;
			p += Common;
		}
		else break;
		
		LAssert(p <= end);
		if (p >= end)	// End of read buffer reached?
			break;		// Exit loop
	}	
	
	return p - (uint8_t*) Ptr;
}

ssize_t GMemFile::Write(const void *Ptr, ssize_t Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;

	uint8_t *p = (uint8_t*) Ptr;
	ssize_t len = Size;
	
	Block *b = GetLast();
	if (b && b->Used < BlockSize)
	{
		// Any more space in the last block?
		ssize_t Remaining = BlockSize - b->Used;
		ssize_t Common = MIN(Remaining, Size);
		if (Common > 0)
		{
			memcpy(b->Data + b->Used, p, Common);
			p += Common;
			len -= Common;
			b->Used += Common;
		}
	}

	// Store remaining data into new blocks		
	while (len > 0)
	{
		b = Create();
		if (!b)
			break;
		
		ssize_t Common = MIN(BlockSize, len);
		memcpy(b->Data, p, Common);
		b->Used = Common;
		p += Common;
		len -= Common;
	}
	
	return Size - len;
}
