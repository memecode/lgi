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
	// This strips out '\r' characters in the input.
	// And also removes previous input when receiving '\b' backspace chars.
	bool ProcessInput = true;
	size_t Pos = 0;
	
	LMutex Sem;
	LArray<char16> Txt;
	
	size_t SizeLimit = 0;

	void ProcessTxt()
	{
		if (Txt.Length() == 0)
			return;

		LArray<char16> Local;
		if (Sem.LockWithTimeout(250, _FL))
		{
			// Minimize time locked by moving the text to a local var
			Local.Swap(Txt);
			Sem.Unlock();
		}
		else
		{
			TView::PostEvent(M_LOG_TEXT); // Try again later...
			return;
		}

		if (Local.Length())
		{
			LString msg;
			msg.Printf("LTextLog::ProcessTxt(%" PRIu64 ")", (uint64)Txt.Length());
			LProfile p(msg, 200);
			Add(Local.AddressOf(), Local.Length());
		}
	}

public:
	LThreadSafeTextView(int id) : TView(id, 0, 0, 2000, 1000), Sem("LThreadSafeTextView")
	{
		TView::Sunken(true);
		TView::SetPourLargest(true);
		TView::SetUndoOn(false);
		TView::SetWrapType(L_WRAP_NONE);
	}
	
	const char *GetClass() override { return "LThreadSafeTextView"; }
	
	void SetSizeLimit(size_t limit)
	{
		SizeLimit = limit;
	}
	
	void OnCreate() override
	{
		TView::OnCreate();
		TView::SetPulse(1000);
	}

	void OnPulse() override
	{
		ProcessTxt();
		
		if (SizeLimit > 0 &&
			TView::Length() >= SizeLimit)
			TView::Name("");
	}

	virtual void Add(char16 *w, ssize_t chars = -1)
	{
		ssize_t Len = chars >= 0 ? chars : StrlenW(w);
		bool AtEnd = TView::GetCaret() == TView::Size;
		
		if (ProcessInput)
		{
			auto *s = w, *prev = w, *end = w + Len;
			
			auto Ins = [this](char16 *s, ssize_t len)
			{
				if (len > 0)
				{
					auto del = MIN(TView::Size - (ssize_t)Pos, len);
					if (del > 0)
						TView::Delete(Pos, del);
					TView::Insert(Pos, s, len);
					Pos += len;
				}
			};			
			
			for (; s < end; s++)
			{
				if (*s == '\b')
				{
					// Remove previous character
					if (Pos > 0)
					{
						Ins(prev, s - prev);
						Pos--;
						prev = s + 1;
					}
				}
				else if (*s == '\r')
				{
					// Insert processed text
					Ins(prev, s - prev);
					prev = s + 1;
					while (Pos > 0 && TView::Text[Pos-1] != '\n')
						Pos--;
				}
				else if (*s == '\n')
				{
					Pos = TView::Size;
				}
			}
			
			Ins(prev, s - prev);
		}
		else
		{
			TView::Insert(TView::Size, w, Len);
		}
		
		TView::Invalidate();
		if (AtEnd)
			TView::SetCaret(TView::Size, false);
	}

	int64 SetSize(int64 s) override
	{
		TView::Name(0);
		return 0;
	}

	bool Write(const LString &s)
	{
		return Write(s.Get(), s.Length()) == s.Length();
	}
	
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override
	{
		LAutoWString w(Utf8ToWide((char*)Buffer, Size));
		if (!w)
			return 0;

		size_t OldLen = Txt.Length();
		if (Sem.Lock(_FL))
		{
			Txt.Add(w.Get(), Strlen(w.Get()));
			Sem.Unlock();
		}

		if (!OldLen
			#if LGI_VIEW_HANDLE
			&& TView::Handle()
			#endif
		)
		{
			TView::PostEvent(M_LOG_TEXT);
		}

		return Size;
	}

	LMessage::Result OnEvent(LMessage *m) override
	{
		if (m->Msg() == M_LOG_TEXT)
		{
			ProcessTxt();
		}

		return TView::OnEvent(m);
	}
};

typedef LThreadSafeTextView<LTextView3> LTextLog;
#ifdef _LTEXTVIEW4_H
typedef LThreadSafeTextView<LTextView4> LTextLog4;
#endif

#endif
