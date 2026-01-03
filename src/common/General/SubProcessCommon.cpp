#include "lgi/common/Lgi.h"
#include "lgi/common/SubProcess.h"

#if 0
#define LOG(...)		printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
LSubProcess::IoThread::IoThread(const char *exe, const char *args) :
	LThread("SubProcIoThread"),
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
	{
		LOG("%s:%i - start: no process?\n", _FL);
		return false;
	}
	
	if (!process->Start(ReadAccess, WriteAccess, MapStderrToStdout))
	{
		LOG("%s:%i - start: failed to start process\n", _FL);
		return false;
	}
	
	// Start the thread to watch the output...
	LOG("%s:%i - start: run thread\n", _FL);
	Run();
	return true;
}

/// \returns true if something was read
bool LSubProcess::IoThread::doRead(bool ended)
{
	if (!process || !process->Peek())
	{
		LOG("%s:%i - doRead: no process or peek=0\n", _FL);
		return false;
	}

	char buf[1024];
	auto rd = process->Read(buf, sizeof(buf));
	if (!ended && rd <= 0)
	{
		LOG("%s:%i - doRead: read=0\n", _FL);
		return false;
	}

	if (onBlock)
	{
		// Unparsed data mode:
		if (rd > 0)
		{
			LOG("%s:%i - doRead: onBlock=%i\n", _FL, (int)rd);
			onBlock(LString(buf, rd));
		}
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
		{
			auto stdOut = out.NewLStr();
			LOG("%s:%i - doRead: onStdout ended=%i\n", _FL, (int)stdOut.Length());
			onStdout(stdOut);
		}
		else
		{
			LOG("%s:%i - doRead: onStdout running=%i\n", _FL, (int)rd);
		}
	}

	return true;
}

int LSubProcess::IoThread::Main()
{
	LOG("%s:%i - main: starting...\n", _FL);
	while (!IsCancelled())
	{
		bool readData = false;
		
		{
			Auto lck(this, _FL);
			if (process)
			{
				bool running = process->IsRunning();
				readData = doRead(!running);
				LOG("%s:%i - main: doRead=%i running=%i\n", _FL, readData, running);
				if (!running)
				{
					if (onComplete)
						onComplete(process->GetExitValue());
					break;
				}
			}
			else
			{
				LOG("%s:%i - main: no process\n", _FL);
			}
		}
		
		if (!readData) // only sleep when there is no data
			LSleep(50);
	}

	LOG("%s:%i - main: exiting...\n", _FL);
	return 0;
}
