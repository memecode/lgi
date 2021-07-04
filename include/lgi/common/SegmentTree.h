#ifndef _GSEGMENT_TREE_H_
#define _GSEGMENT_TREE_H_

#include "LgiDefs.h"
#include "lgi/common/Mem.h"

class LSegment
{
	friend class LSegmentTree;
	LSegment *Left, *Right, *Parent;	

	bool Insert(LSegment *Seg, LSegment **Conflict);
	LSegment *Find(int64 Off);
	void Index(LSegment **&i);

public:
	int64 Start;
	int64 Length;
	void *UserData;

	LSegment(int64 start, int64 len, void *userData = 0)
	{
		Left = Right = Parent = 0;
		Start = start;
		Length = len;
		UserData = userData;
	}

	~LSegment()
	{
		DeleteObj(Left);
		DeleteObj(Right);
	}

	bool Overlap(LSegment *Seg)
	{
		if (Seg->Start + Seg->Length <= Start ||
			Seg->Start >= Start + Length)
		{
			return false;
		}

		return true;
	}

	bool operator < (LSegment *Seg)
	{
		return Start + Length <= Seg->Start;
	}

	bool operator > (LSegment *Seg)
	{
		return Start >= Seg->Start + Seg->Length;
	}
};

class LSegmentTree
{
	class LSegmentTreePrivate *d;

public:
	LSegmentTree();
	virtual ~LSegmentTree();

	// Status
	int Items();

	// Change
	bool Insert(LSegment *Seg, LSegment **Conflict = 0);
	bool Delete(LSegment *Seg);
	void Empty();

	// Query
	LSegment *ItemAt(int Offset);
	LSegment **CreateIndex();
};

#endif