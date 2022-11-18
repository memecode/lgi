/// \file
/// \author Matthew Allen
#ifndef __PROGRESS_H
#define __PROGRESS_H

#include "lgi/common/Mutex.h"
#include "lgi/common/Cancel.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/Dom.h"
#include "lgi/common/DateTime.h"

/// Generic progress class, keeps track of how far through a task you are.
class LgiClass Progress :
	public LMutex,
	public LCancel,
	virtual public LDom
{
protected:
	LString Description, Type;
	uint64_t Start = 0;
	LDateTime StartDt;
	int64_t Val = 0;
	int64_t Low = 0, High = 0;
	double Scale = 1.0;

public:
	uint64 UserData = 0;
	
	Progress();
	Progress(char *desc, int64 l, int64 h, char *type = NULL, double scale = 1.0);
	virtual ~Progress();

	virtual bool SetRange(const LRange &r) { Low = r.Start; High = r.End(); return true; }
	virtual LRange GetRange() { return LRange((ssize_t)Low, (ssize_t)(High - Low)); }
	virtual LString GetDescription();
	virtual void SetDescription(const char *d = 0);
	virtual int64 Value() { return Val; }
	virtual void Value(int64 v) { Val = v; }
	virtual double GetScale() { return Scale; }
	virtual void SetScale(double s) { Scale = s; }
	virtual LString GetType();
	virtual void SetType(const char *t);
	
	Progress &operator =(Progress &p);
};

#endif
