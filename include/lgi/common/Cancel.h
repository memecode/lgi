//
//  LCancel.h
//  Lgi
//
//  Created by Matthew Allen on 19/10/2017.
//
//
#pragma once

class LgiClass LCancel
{
	bool Cancelled;
	
public:
	LCancel()
	{
		Cancelled = false;
	}
	
	virtual ~LCancel() {}
	
	virtual bool IsCancelled()
	{
		return Cancelled;
	}
	
	virtual bool Cancel(bool b = true)
	{
		Cancelled = b;
		return true;
	}
};

