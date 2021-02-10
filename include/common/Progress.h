/// \file
/// \author Matthew Allen
#ifndef __PROGRESS_H
#define __PROGRESS_H

#include "LMutex.h"
#include "LCancel.h"
#include "GStringClass.h"
#include "GDom.h"

/// Generic progress class, keeps track of how far through a task you are.
class LgiClass Progress :
	public LMutex,
	public LCancel,
	virtual public GDom
{
protected:
	GString Description, Type;
	int64 Start;
	int64 Val;
	int64 Low, High;
	double Scale;

public:
	uint64 UserData;
	
	Progress();
	Progress(char *desc, int64 l, int64 h, char *type = NULL, double scale = 1.0);
	virtual ~Progress() {}

	virtual bool SetRange(const GRange &r) { Low = r.Start; High = r.End(); return true; }
	virtual GRange GetRange() { return GRange(Low, High - Low); }
	virtual GString GetDescription();
	virtual void SetDescription(const char *d = 0);
	virtual int64 Value() { return Val; }
	virtual void Value(int64 v) { Val = v; }
	virtual double GetScale() { return Scale; }
	virtual void SetScale(double s) { Scale = s; }
	virtual GString GetType();
	virtual void SetType(const char *t);
	virtual Progress &operator =(Progress &p);

	[[deprecated]] virtual void SetParameter(int Which, int What) {}
	[[deprecated]] virtual void GetLimits(int64 *l, int64 *h)
	{
		if (l) *l = Low;
		if (h) *h = High;
	}
	[[deprecated]] virtual void SetLimits(int64 l, int64 h)
	{
		Low = l;
		High = h;
	}
};


#endif
