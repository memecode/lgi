/// \file
/// \author Matthew Allen
#ifndef _GTEXTLOG_H_
#define _GTEXTLOG_H_

#include "GTextView3.h"
#include "INet.h"

class GTextLog : public GTextView3, public GStream
{
protected:
	bool RemoveReturns;
	LMutex Sem;
	GArray<char16> Txt;

	void ProcessTxt()
	{
		if (Txt.Length() == 0)
			return;
		if (Sem.Lock(_FL))
		{
			GProfile p("GTextLog::ProcessTxt()", 100);
			Add(Txt.AddressOf(), Txt.Length());
			Txt.Empty();
			Sem.Unlock();
		}
	}

public:
	GTextLog(int id) : GTextView3(id, 0, 0, 2000, 1000)
	{
		RemoveReturns = true;
		Sunken(true);
		SetPourLargest(true);
		SetWrapType(TEXTED_WRAP_NONE);
	}
	
	void OnCreate()
	{
		GTextView3::OnCreate();
		ProcessTxt();
	}

	virtual void Add(char16 *w, ssize_t chars = -1)
	{
		ssize_t Len = chars >= 0 ? chars : StrlenW(w);
		
		if (RemoveReturns)
		{
			char16 *end = w + Len;
			for (char16 *s = w; *s; )
			{
				char16 *e = s;
				while (e < end && *e != '\r')
					e++;
				if (e > s)
					Insert(Size, s, e - s);
				if (e >= end)
					break;
				s = e + 1;
			}
		}
		else
		{
			Insert(Size, w, Len);
		}
		
		Invalidate();
		SetCaret(Size, false);
	}

	int64 SetSize(int64 s)
	{
		Name(0);
		return 0;
	}
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		GAutoWString w(Utf8ToWide((char*)Buffer, Size));
		if (!w)
			return 0;

		if (Sem.LockWithTimeout(200, _FL))
		{
			Txt.Add(w.Get(), Strlen(w.Get()));
			Sem.Unlock();
		}
		#if LGI_VIEW_HANDLE
		if (Handle())
		#endif
		{
			PostEvent(M_LOG_TEXT);
		}

		return Size;
	}

	GMessage::Result OnEvent(GMessage *m)
	{
		if (m->Msg() == M_LOG_TEXT)
		{
			ProcessTxt();
		}

		return GTextView3::OnEvent(m);
	}
};

#endif
