#pragma once

/// This class keeps an array of ranges in sorted order, merging 
/// adjacent ranges when new ranges are added.
class LRanges : public LArray<LRange>
{
public:
	LRanges(const char *init = NULL)
	{
		if (init)
			FromString(init);
	}

	LRanges &operator +=(const LRange &r)
	{
		// Insert at the right position
		if (Length() == 0)
			New() = r;
		else if (r > Last())
			New() = r;
		else
		{
			size_t s = 0, e = Length() - 1;
			while (s != e)
			{
				size_t mid = s + ((e - s + 1) >> 1);
				auto &mr = (*this)[mid];
				if (r < mr)
					e = mid - 1;
				else if (r >= mr)
					s = mid + 1;
				else
				{
					s = e = mid;
					break;
				}
			}

			auto &cur = (*this)[s];
			if (r < cur)
				AddAt(s, r);
			else if (r >= cur)
				AddAt(s + 1, r);
			else
			{
				// Overlapping?
				auto us = MIN(r.Start, cur.Start);
				LRange u(us, MAX(r.End(), cur.End()) - us);
				cur = u;
			}
		}

		return *this;
	}

	/// Returns the complete range, start to end, including any gaps.
	LRange Union()
	{
		LRange u;

		if (Length() > 0)
		{
			u.Start = First().Start;
			u.Len = Last().End() - u.Start;
		}

		return u;
	}

	// If you edit values or add ranges outside of the += operator this
	// will fix the data to be consistent.
	bool Merge()
	{
		bool SortChk = true;
		RestartMerge:
		for (unsigned i=0; i<Length()-1; )
		{
			auto &a = (*this)[i];
			auto &b = (*this)[i+1];

			if (SortChk && b.Start < a.Start)
			{
				// Sort error..
				SortChk = false;
				Sort
				(
					[](LRange *a, LRange *b) -> int
					{
						auto Diff = a->Start - b->Start;
						if (Diff < 0) return -1;
						return Diff > 1;
					}
				);
				goto RestartMerge;
			}

			if (a.End() >= b.Start)
			{
				// Merge segments
				a.Len = b.End() - a.Start;
				if (!DeleteAt(i + 1))
					return false;
			}
			else i++;
		}
		return true;
	}

	// Check the ranges are sorted and non-overlapping
	bool IsValid() const
	{
		for (size_t i=0; i<Length()-1; i++)
		{
			auto &a = ItemAt(i);
			auto &b = ItemAt(i+1);
			if (a.End() >= b.Start)
				return false;
		}
		return true;
	}

	LString ToString()
	{
		LStringPipe p;
		for (auto r : *this)
			p.Print(LPrintfSSizeT "-" LPrintfSSizeT ",", r.Start, r.End());
		return p.NewLStr();
	}

	bool FromString(LString s)
	{
		Length(0);
		auto Rngs = s.SplitDelimit(",");
		for (auto r : Rngs)
		{
			auto a = r.Split("-");
			if (a.Length() != 2) return false;
			auto &n = New();
			n.Start = a[0].Int();
			n.Len = a[1].Int() - n.Start;
		}
		return true;
	}

	static bool UnitTests()
	{
		/*
		// Out of order merge test
		FromString("30-50,20-35,45-60");
		Merge();
		if (Length() != 1) goto Error;
		auto &r = (*this)[0];
		if (r.Start != 20 || r.Len != 40) goto Error;
		Length(0);
		*/

		// Overlapping insert test
		LRanges r("10-20,30-40,50-60,70-80,90-100");
		LRange *rng = NULL;
		r += LRange(55,10);
		if (r.Length() != 5)
			goto Error;
		rng = r.AddressOf(2);
		if (rng->Start != 50 || rng->Len != 15)
			goto Error;

		return true;

	Error:
		LAssert(!"Failed.");
		return false;
	}
};

