#include "Lgi.h"
#include "LAnonPipe.h"

#define WT_EXECUTEDEFAULT 0x00000000
#define WT_EXECUTEINIOTHREAD 0x00000001
#define WT_EXECUTEINUITHREAD 0x00000002
#define WT_EXECUTEINWAITTHREAD 0x00000004
#define WT_EXECUTEONLYONCE 0x00000008
#define WT_EXECUTEINTIMERTHREAD 0x00000020
#define WT_EXECUTELONGFUNCTION 0x00000010
#define WT_EXECUTEINPERSISTENTIOTHREAD 0x00000040
#define WT_EXECUTEINPERSISTENTTHREAD 0x00000080
#define WT_TRANSFER_IMPERSONATION 0x00000100
#define WT_EXECUTEINLONGTHREAD 0x00000010
#define WT_EXECUTEDELETEWAIT 0x00000008 

typedef VOID (CALLBACK *WAITORTIMERCALLBACK)(PVOID lpParameter,BOOLEAN TimerOrWaitFired);
typedef BOOL (WINAPI *pCreateTimerQueueTimer)(PHANDLE phNewTimer, HANDLE TimerQueue, WAITORTIMERCALLBACK callback, PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags);
typedef BOOL (WINAPI *pDeleteTimerQueueTimer)(HANDLE, HANDLE, HANDLE);

VOID CALLBACK PipeTimer_Callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	LAnonPipe *Ap = (LAnonPipe*)lpParameter;
	Ap->PostEvent(WM_TIMER);
}

struct Msg
{
	int Cmd;
	int a;
	int b;
};

struct GAnonPipePriv : public LLibrary, public GSemaphore
{
	HANDLE r, w;
	HANDLE Timer;

	GAnonPipePriv() : LLibrary("Kernel32")
	{
		Timer = r = w = INVALID_HANDLE;
		CreatePipe(&r, &w, 0, 0);
	}

	~GAnonPipePriv()
	{
		Close();
	}

	void Close()
	{
		if (r != INVALID_HANDLE)
		{
			CloseHandle(r);
			r = INVALID_HANDLE;
		}

		if (w != INVALID_HANDLE)
		{
			CloseHandle(w);
			w = INVALID_HANDLE;
		}
	}
};

LAnonPipe::LAnonPipe()
{
	d = new GAnonPipePriv;
}

LAnonPipe::~LAnonPipe()
{
	DeleteObj(d);
}

void LAnonPipe::Close()
{
	d->Close();
}

bool LAnonPipe::IsOk()
{
	return  d &&
			d->r != INVALID_HANDLE &&
			d->w != INVALID_HANDLE;
}

bool LAnonPipe::SetPulse(int i)
{
	if (d->Timer != INVALID_HANDLE)
	{
		pDeleteTimerQueueTimer DeleteTimerQueueTimer = (pDeleteTimerQueueTimer) d->GetAddress("DeleteTimerQueueTimer");
		LAssert(DeleteTimerQueueTimer);
		if (DeleteTimerQueueTimer)
		{
			DeleteTimerQueueTimer(0, d->Timer, INVALID_HANDLE_VALUE);
		}
	}

	if (i > 0)
	{
		pCreateTimerQueueTimer CreateTimerQueueTimer = (pCreateTimerQueueTimer) d->GetAddress("CreateTimerQueueTimer");
		LAssert(CreateTimerQueueTimer);
		if (CreateTimerQueueTimer)
		{
			return CreateTimerQueueTimer(&d->Timer, 0, PipeTimer_Callback, this, i, i, WT_EXECUTEINPERSISTENTTHREAD);
		}
	}

	return true;
}

int LAnonPipe::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg)
	{
		case WM_TIMER:
		{
			OnPulse();
			break;
		}
	}

	return 0;
}

void LAnonPipe::PostEvent(int cmd, int a, int b)
{
	if (!IsOk())
	{
		LAssert(!"Not a valid pipe");
		return;
	}

	Msg m = {cmd, a, b};
	DWORD written = 0;
	if (d->Lock(_FL))
	{
		WriteFile(d->w, &m, sizeof(m), &written, 0);
		d->Unlock();
	}
}

LMessage *LAnonPipe::GetMessage()
{
	if (!IsOk())
	{
		LAssert(!"Not a valid pipe");
		return 0;
	}

	Msg m;
	DWORD read;
	if (ReadFile(d->r, &m, sizeof(m), &read, 0))
	{
		if (read == sizeof(m))
		{
			LMessage *n = new LMessage;
			if (n)
			{
				n->Msg = m.Cmd;
				n->a = m.a;
				n->b = m.b;
				return n;
			}
		}
	}

	return 0;
}
