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
			auto *in = w, *out = w, *end = w + Len;
			
			while (in < end)
			{
				if (*in == '\b')
				{
					// Backspace: Remove previous character
					if (out > w)
						out--;
					else
						LAssert(!"can we delete a character from the document?");
				}
				else if (*in == '\r')
					// Strip returns
					;
				else
					*out++ = *in;

				in++;
			}

			Len = out - w;
		}

		TView::Insert(TView::Size, w, Len);
		TView::Invalidate();

		if (AtEnd)
			TView::SetCaret(TView::Size, false);
	}

	int64 SetSize(int64 s) override
	{
		TView::Name(0);
		return 0;
	}

	void Empty()
	{
		TView::PostEvent(M_LOG_EMPTY);
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
		switch (m->Msg())
		{
			case M_LOG_TEXT:
			{
				ProcessTxt();
				break;
			}
			case M_LOG_EMPTY:
			{
				Name(nullptr);
				break;
			}
		}

		return TView::OnEvent(m);
	}
};

typedef LThreadSafeTextView<LTextView3> LTextLog;
#ifdef _LTEXTVIEW4_H
typedef LThreadSafeTextView<LTextView4> LTextLog4;
#endif

#endif
