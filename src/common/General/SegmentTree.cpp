#include "lgi/common/LgiInc.h"
#include "LgiOsDefs.h"
#include "lgi/common/SegmentTree.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
bool LSegment::Insert(LSegment *Seg, LSegment **Conflict)
{
	if (Overlap(Seg))
	{
		*Conflict = this;
		return false;
	}

	if (*Seg < this)
	{
		if (Left)
		{
			return Left->Insert(Seg, Conflict);
		}
		else
		{
			Left = Seg;
			Seg->Parent = this;
		}
	}
	else if (*Seg > this)
	{
		if (Right)
		{
			return Right->Insert(Seg, Conflict);
		}
		else
		{
			Right = Seg;
			Seg->Parent = this;
		}
	}

	return true;
}

LSegment *LSegment::Find(int64 Off)
{
	if (Off >= Start && Off < Start + Length)
	{
		return this;
	}

	if (Left)
	{
		LSegment *s = Left->Find(Off);
		if (s) return s;
	}

	if (Right)
	{
		return Right->Find(Off);
	}

	return 0;
}

void LSegment::Index(LSegment **&i)
{
	if (Left)
	{
		Left->Index(i);
	}

	*i++ = this;

	if (Right)
	{
		Right->Index(i);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class LSegmentTreePrivate
{
public:
	int Items;
	LSegment *Root;

	LSegmentTreePrivate()
	{
		Items = 0;
		Root = 0;
	}

	~LSegmentTreePrivate()
	{
		DeleteObj(Root);
	}
};

LSegmentTree::LSegmentTree()
{
	d = new LSegmentTreePrivate;
}

LSegmentTree::~LSegmentTree()
{
	DeleteObj(d);
}

int LSegmentTree::Items()
{
	return d->Items;
}

void LSegmentTree::Empty()
{
	DeleteObj(d->Root);
	d->Items = 0;
}

bool LSegmentTree::Insert(LSegment *Seg, LSegment **Conflict)
{
	bool Status = false;

	if (Seg)
	{
		if (d->Root)
		{
			LSegment *c = 0;			
			Status = d->Root->Insert(Seg, &c);
			if (!Status && Conflict)
			{
				*Conflict = c;
			}
		}
		else
		{
			d->Root = Seg;
			Status = true;
		}

		if (Status)
		{
			d->Items++;
		}
	}

	return Status;
}

bool LSegmentTree::Delete(LSegment *Seg)
{
	return false;
}

LSegment *LSegmentTree::ItemAt(int Offset)
{
	return d->Root ? d->Root->Find(Offset) : 0;
}

LSegment **LSegmentTree::CreateIndex()
{
	LSegment **Index = 0;

	if (d->Items > 0 && d->Root)
	{
		Index = new LSegment*[d->Items];
		if (Index)
		{
			LSegment **i = Index;
			d->Root->Index(i);
			LAssert(i == Index + d->Items);
		}
	}

	return Index;
}
