#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
LSubProcess::IoThread::IoThread(const char *exe, const char *args) :
	LThread("LSubProcess.IoThread.Thread"),
	LMutex( "LSubProcess.IoThread.Lock")
{
	if (exe)
		Create(exe, args);
}

LSubProcess::IoThread::~IoThread()
{
	Cancel();
	WaitForExit();
}

bool LSubProcess::IoThread::Create(const char *exe, const char *args)
{
	Auto lck(this, _FL);
	if (!exe)
		return false;
	return process.Reset(new LSubProcess(exe, args));
}

bool LSubProcess::IoThread::Start(bool ReadAccess, bool WriteAccess, bool MapStderrToStdout)
{
	Auto lck(this, _FL);
	if (!process)
		return false;
	return process->Start(ReadAccess, WriteAccess, MapStderrToStdout);
}

void LSubProcess::IoThread::doRead(bool ended)
{
	if (!process || !process->Peek())
		return;

	char buf[1024];
	auto rd = process->Read(buf, sizeof(buf));
	if (!ended && rd <= 0)
		return;

	if (onBlock)
	{
		// Unparsed data mode:
		if (rd > 0)
			onBlock(LString(buf, rd));
	}
	else if (onLine)
	{
		// Parse into lines:
		if (rd > 0)
			out.Write(buf, rd);

		while (true)
		{
			ssize_t match = out.CharAt("\r\n");
			if (match < 0)
				break;

			LString line;
			if (line.Length(match))
			{
				out.Read(line.Get(), match);
				onLine(line);
			}
			char ch;
			out.Read(&ch, 1); // consume the first CR or LF
			if (ch == '\r')
			{
				auto next = out.Peek(1); // check if the next char is LF
				if (next == "\n")
					out.Read(&ch, 1); // consume that too...
			}
		}
		
		if (ended)
			// Process last line:
			if (auto ln = out.NewLStr())
				onLine(ln);
	}
	else if (onStdout)
	{
		// Whole process output in one go:
		if (rd > 0)
			out.Write(buf, rd);
		if (ended)
			onStdout(out.NewLStr());
	}
}

int LSubProcess::IoThread::Main()
{
	while (!IsCancelled())
	{
		{
			Auto lck(this, _FL);
			if (process)
			{
				bool running = process->IsRunning();
				doRead(!running);
				if (!running)
				{
					if (onComplete)
						onComplete(process->GetExitValue());
					break;
				}
			}
		}
		
		LSleep(50);
	}

	return 0;
}
