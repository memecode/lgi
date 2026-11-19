#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <string>

#define LAssert assert

#include "lgi/common/LgiDefs.h"
#include "lgi/common/Array.h"
#include "lgi/common/StringClass.h"

#define _FL __FILE__, __LINE__

enum HandleType
{
	hRead,
	hWrite,
	hMax,
};

int in[hMax]; // app -> child pipe
int out[hMax]; // child -> app pipe
bool loop = true;

void LSleep(uint32_t i)
{
	struct timespec request = {}, remain = {};

	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	while (nanosleep(&request, &remain) == -1)
	    request = remain;
}

enum AppState
{
	sInit,
	sFrameSwitch,
	sLocals,
	sArgs,
	sList,
	sGetThreads,
	sThreadSwitch,
	sThreadBt,
}	state = sInit;

LString::Array *output = NULL;
int curThread = 1, threads = -1;
FILE *crashLog = NULL;

void Log(const char *fmt, ...)
{
	va_list args, copy;
	va_start(args, fmt);
	va_copy(copy, args);
	vprintf(fmt, args);
	va_end(args);
	if (crashLog)
		vfprintf(crashLog, fmt, copy);
	va_end(copy);
}

void OpenCrashLog()
{
	if (crashLog)
		return;
		
	time_t now;
	time(&now);

	auto cur = localtime(&now);
	char file[256] = "crash.log";

	if (cur)	
		snprintf(file, sizeof(file),
			"crash-%i%2.2i%2.2i-%2.2i%2.2i%2.2i.log",
			cur->tm_year + 1900,
			cur->tm_mon + 1,
			cur->tm_mday,
			cur->tm_hour,
			cur->tm_min,
			cur->tm_sec);
			
	crashLog = fopen(file, "w");
}

void DumpOutput(const char *Label)
{
	Log("%s:\n", Label);
	for (auto ln: *output)
	{
		if (ln.Find("(gdb)") < 0)
			Log("   %s\n", ln.Get());
	}
	Log("\n");
	output->Length(0);
}

/*

Basic state flow:

	sInit
	sGetThreads				# display all the threads
	for each thread:
		sThreadBt			# back trace the thread
		sThreadSwitch		# display back trace and check for crashed frame
		if has crash frame:
			sFrameSwitch	# get to the right frame...
			sLocals			# display the locals
			sArgs			# display any function args
			sList			# list the source code around the crash point
			
*/
void AtPrompt()
{
	auto GotoNextThread = [&]()
	{
		if (curThread <= threads)
		{
			// Check out the next thread...
			char cmd[256];
			auto cmdSz = snprintf(cmd, sizeof(cmd), "thread %i\n", curThread);
			int wr = write(in[hWrite], cmd, cmdSz);
			state = sThreadBt;
		}
		else loop = false;
	};


	switch (state)
	{
		case sInit:
		{
			/*			
			First thing is to look through all the threads and find the one that crashed...
			No point dumping locals and args from the thread that didn't crash.			
			*/
			auto cmd = "info threads\n";
			int wr = write(in[hWrite], cmd, strlen(cmd));
			
			state = sGetThreads;
			output = new LString::Array;
			
			OpenCrashLog();
			break;
		}
		case sFrameSwitch:
		{
			DumpOutput("Frame");

			// Then start dumping out the locals and args...
			auto cmd = "info locals\n";
			auto wr = write(in[hWrite], cmd, strlen(cmd));

			state = sLocals;
			break;
		}
		case sLocals:
		{
			DumpOutput("Locals");
		
			auto cmd = "info args\n";
			int wr = write(in[hWrite], cmd, strlen(cmd));
			
			state = sArgs;
			break;
		}
		case sArgs:
		{
			DumpOutput("Args");
		
			auto cmd = "list\n";
			int wr = write(in[hWrite], cmd, strlen(cmd));
			
			state = sList;
			break;
		}
		case sList:
		{
			DumpOutput("List");
		
			// If there are more threads... go back trace them
			GotoNextThread();
			break;
		}
		case sGetThreads:
		{
			LString last;
			for (int i=output->Length()-1; i>=0; i--)
			{
				last = (*output)[i].Strip();
				if (isdigit(last(0)))
				{
					threads = (int)last.Int();
					break;
				}
			}
			
			DumpOutput("Threads");
			output->Length(0);
			
			if (threads > 0)
			{
				GotoNextThread();
			}
			else
			{
				Log("%s:%i - Error: failed to get thread count.\n", _FL);
				loop = false;
			}
			break;
		}
		case sThreadSwitch:
		{
			bool hasCrash = false;
			int crashFrame = -1;
			if (output->Length() > 0)
			{
				char label[64];
				snprintf(label, sizeof(label), "Thread %i", curThread++);
				
				// Scan through the output and look for some crash indication
				int curFrame = -1;
				for (unsigned i=0; i<output->Length(); i++)
				{
					auto ln = (*output)[i];
					
					auto s = ln.Strip();
					if (s.Length() && s(0) == '#')
						curFrame = s.SplitDelimit()[0].Strip("#").Int();
					
					if (ln.Find("LgiCrashHandler") >= 0 ||
						ln.Find("signal handler called") >= 0)
					{
						hasCrash = true;
						crashFrame = curFrame + 1;
					}
				}
				
				DumpOutput(label);
			}

			char cmd[256];
			if (hasCrash && crashFrame >= 0)
			{
				// Switch to the crashing frame here...
				auto cmdSz = snprintf(cmd, sizeof(cmd), "frame %i\n", crashFrame);
				int wr = write(in[hWrite], cmd, cmdSz);
				state = sFrameSwitch;
			}
			else
			{
				// Check out the next thread...
				GotoNextThread();
			}
			break;
		}
		case sThreadBt:
		{
			auto cmd = "bt\n";
			int wr = write(in[hWrite], cmd, strlen(cmd));
			state = sThreadSwitch;
			output->Length(0);
			break;
		}			
	}
}

void OnLine(const char *str)
{
	// printf("line='%s'\n", str);
	if (output)
		output->Add(str);
	
	if (!strcasecmp(str, "(gdb) "))
		AtPrompt();
}

int main(int argCount, char **arg)
{
	int pid = -1;
	for (int i=1; i<argCount; i++)
	{
		if (!strcasecmp(arg[i], "--pid") &&
			i < argCount - 1)
		{
			pid = atoi(arg[i+1]);
			break;
		}
	}
	
	if (pid < 0)
	{
		printf("Error: no pid given.\n");
		return -1;
	}
	
	if (pipe(in) == -1)
	{
		printf("parent: Failed to create stdin pipe");
		return false;
	}

	if (pipe(out) == -1)
	{
		printf("parent: Failed to create stdout pipe");
		return false;
	}

	auto child = fork();
	if (child < 0)
	{
		perror("crash-handler: fork");
		return 1;
	}
	if (child)
	{
		// GDB is the collector's parent and a direct child of the target.
		char pidArg[64];
		snprintf(pidArg, sizeof(pidArg), "--pid=%i", pid);

		// Child shouldn't write to its stdin.
		if (close(in[hWrite]))
			printf("%s:%i - close failed.\n", _FL);

		// Child shouldn't read from its stdout.
		if (close(out[hRead]))
			printf("%s:%i - close failed.\n", _FL);

		// Redirect stdin and stdout for the child process.
		if (dup2(in[hRead], fileno(stdin)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stdin for child\n", _FL);
			return -1;
		}
		if (close(in[hRead]))
			printf("%s:%i - close failed.\n", _FL);
		
		if (dup2(out[hWrite], fileno(stdout)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stdout for child\n", _FL);
			return -1;
		}

		if (dup2(out[hWrite], fileno(stderr)) == -1)
		{
			printf("%s:%i - child[pre-exec]: Failed to redirect stderr for child\n", _FL);
			return -1;
		}
		close(out[hWrite]);

		execlp("gdb", "gdb", "--nx", "-iex", "set pagination off", "-iex", "set confirm off",
			"-iex", "set debuginfod enabled off", pidArg, static_cast<char *>(nullptr));
		
		printf("%s:%i - execvp failed.\n", _FL);
		return -1;
	}
	
	close(in[hRead]);
	close(out[hWrite]);

	char buf[512];
	std::string pending;
	bool failed = false;
	while (loop)
	{
		auto rd = read(out[hRead], buf, sizeof(buf));
		if (rd < 0)
		{
			if (errno == EINTR)
				continue;
			perror("crash-handler: reading GDB output");
			failed = true;
			break;
		}
		if (rd == 0)
		{
			Log("GDB exited before crash collection finished.\n");
			failed = true;
			break;
		}

		pending.append(buf, rd);
		size_t start = 0;
		for (size_t end; loop && (end = pending.find('\n', start)) != std::string::npos;)
		{
			pending[end] = '\0';
			OnLine(pending.c_str() + start);
			start = end + 1;
		}
		pending.erase(0, start);
		// GDB's prompt has no newline and can arrive across multiple reads.
		if (loop && pending == "(gdb) ")
		{
			OnLine(pending.c_str());
			pending.clear();
		}
	}

	if (failed && output)
	{
		if (!pending.empty())
			output->Add(pending.c_str());
		DumpOutput("Incomplete GDB output");
	}
	close(in[hWrite]);
	close(out[hRead]);
	if (crashLog)
		fclose(crashLog);

	return failed ? 1 : 0;
}
