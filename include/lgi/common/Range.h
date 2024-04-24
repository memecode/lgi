#ifndef _GRANGE_H_
#define _GRANGE_H_

#include <assert.h>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? a : b)
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? a : b)
#endif

struct LRange
{
	ssize_t Start;
	ssize_t Len;

	// No arguments
	LRange()
	{
		Start = 0;
		Len = 0;
	}

	// One argument = length
	LRange(ssize_t len)
	{
		Start = 0;
		Len = len;
	}

	// Both arguments
	LRange(ssize_t start, ssize_t len)
	{
		Start = start;
		Len = len;
	}

	LRange &Set(ssize_t s, ssize_t l)
	{
		Start = s;
		Len = l;
		return *this;
	}

	LRange Overlap(const LRange &r)
	{
		LRange o;
		if (r.Start >= End())
			return o;
		if (r.End() <= Start)
			return o;

		ssize_t e = MIN(End(), r.End());
		o.Start = MAX(r.Start, Start);
		o.Len = e - o.Start;
		return o; 
	}

	bool Overlap(ssize_t Val)
	{
		return (Val == Start) || (Val >= Start && Val < End());
	}

	LRange Union(LRange &in)
	{
		LRange out;
		out.Start = MIN(Start, in.Start);
		out.Len = MAX(End(), in.End()) - Start;
		return out;
	}

	ssize_t End() const
	{
		return Start + Len;
	}

	bool Valid() const
	{
		return Start >= 0 && Len > 0;
	}

	operator bool() const
	{
		return Start >= 0 && Len > 0;
	}

	bool operator ==(const LRange &r) const { return Start == r.Start && Len == r.Len; }
	bool operator !=(const LRange &r) const { return Start != r.Start || Len != r.Len; }
	bool operator >(const LRange &r) const { return Start > r.End(); }
	bool operator >=(const LRange &r) const { return Start >= r.End(); }
	bool operator <(const LRange &r) const { return End() < r.Start; }
	bool operator <=(const LRange &r) const { return End() <= r.Start; }

	LRange &operator -=(const LRange &del)
	{
		LRange o = Overlap(del);
		if (o.Valid())
		{
			assert(o.Len <= Len);
			Len -= o.Len;
			if (del.Start < o.Start)
				Start = del.Start;
		}
		// else nothing happens

		return *this;
	}

	LRange &operator =(const LRange &r)
	{
		this->Start = r.Start;
		this->Len = r.Len;
		return *this;
	}

	#ifdef LPrintfSSizeT
	const char *GetStr()
	{
		static const int strs = 4;
		static char s[strs][32];
		static int idx = 0;

		char *buf = s[idx++];
		sprintf_s(buf, 32, LPrintfSSizeT "," LPrintfSSizeT, Start, Len);
		if (idx >= strs) idx = 0;
		return buf;
	}
	#endif
};

#endif