/// \file
/// \author Matthew Allen
#ifndef _LTEXTLOG_H_
#define _LTEXTLOG_H_

#include "lgi/common/TextView3.h"
#include "lgi/common/Net.h"

template<typename TView>
class LThreadSafeTextView : public TView, public LStream
{
protected:
	bool ProcessReturns;
	size_t Pos;
	
	LMutex Sem;
	LArray<char16> Txt;

	void ProcessTxt()
	{
		if (Txt.Length() == 0)
			return;
		if (Sem.Lock(_FL))
		{
			LString msg;
			msg.Printf("LTextLog::ProcessTxt(%" PRIu64 ")", (uint64)Txt.Length());
			LProfile p(msg, 100);
			Add(Txt.AddressOf(), Txt.Length());
			Txt.Empty();
			Sem.Unlock();
		}
	}

public:
	LThreadSafeTextView(int id) : TView(id, 0, 0, 2000, 1000)
	{
		ProcessReturns = true;
		Pos = 0;
		Sunken(true);
		SetPourLargest(true);
		SetUndoOn(false);
		SetWrapType(TEXTED_WRAP_NONE);
	}
	
	void OnCreate()
	{
		TView::OnCreate();
		ProcessTxt();
	}

	virtual void Add(char16 *w, ssize_t chars = -1)
	{
		ssize_t Len = chars >= 0 ? chars : StrlenW(w);
		bool AtEnd = GetCaret() == Size;
		
		if (ProcessReturns)
		{
			auto *s = w, *prev = w;
			
			auto Ins = [this](char16 *s, ssize_t len)
			{
				if (len > 0)
				{
					auto del = MIN(Size - (ssize_t)Pos, len);
					if (del > 0)
						Delete(Pos, del);
					Insert(Pos, s, len);
					Pos += len;
				}
			};			
			
			for (s = w;
				chars >= 0 ? s < w + chars : *s;
				s++)
			{
				if (*s == '\r')
				{
					Ins(prev, s - prev);
					prev = s + 1;
					while (Pos > 0 && Text[Pos-1] != '\n')
						Pos--;
				}
				else if (*s == '\n')
				{
					Pos = Size;
				}
			}
			Ins(prev, s - prev);
		}
		else
		{
			Insert(Size, w, Len);
		}
		
		Invalidate();
		if (AtEnd)
			SetCaret(Size, false);
	}

	int64 SetSize(int64 s)
	{
		Name(0);
		return 0;
	}

	bool Write(const LString &s)
	{
		return Write(s.Get(), s.Length()) == s.Length();
	}
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0)
	{
		LAutoWString w(Utf8ToWide((char*)Buffer, Size));
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

	LMessage::Result OnEvent(LMessage *m)
	{
		if (m->Msg() == M_LOG_TEXT)
		{
			ProcessTxt();
		}

		return TView::OnEvent(m);
	}
};

typedef LThreadSafeTextView<LTextView3> LTextLog;

#endif
