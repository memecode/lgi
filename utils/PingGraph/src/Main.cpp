#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"
#include "lgi/common/Thread.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/TextLog.h"

#include "resdefs.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "PingGraph";

enum Ctrls 
{
	ID_STATIC = -1,
	ID_BOX = 100,
	ID_TABLE,
	ID_GRAPH,
	ID_HOSTNAME,
	ID_START,
	ID_LOG,
};

class Graph : public LView, public LMutex
{
	struct Sample {
		unsigned long time;
		uint32_t value;
	};
	LArray<Sample> samples; // lock before using

public:
	Graph(int id) : LMutex("Graph")
	{
		SetId(id);
	}

	void AddSample(int32_t ms)
	{
		{
			Auto lck(this, _FL);
			auto &s = samples.New();
			s.time = (unsigned long)time(NULL);
			s.value = ms;
		}
		Invalidate();
	}

	void OnPaint(LSurface *pDC) override
	{
		Auto lck(this, _FL);
		pDC->Colour(L_WORKSPACE);
		pDC->Rectangle();

		if (samples.Length() == 0)
			return;

		int scale = 5000; // ms
		auto client = GetClient();
		unsigned long first = samples[0].time;
		pDC->Colour(LColour::Blue);
		LPoint prev(0, client.y2-1);
		for (auto &s: samples)
		{
			int x = s.time - first;
			int y = s.value * client.Y() / scale;
			LPoint pt(client.x1 + x,
					  client.y2 - 1 - y);
			pDC->Line(prev.x, prev.y, pt.x, pt.y);
			prev = pt;
		}
	}
};

class Worker : public LThread, public LCancel
{
	LWindow *app;
	LString host;
	LTextLog *log;
	Graph *graph;

public:
	Worker(LWindow *appWnd, LString hostName, LTextLog *logger, Graph *grapher) :
		LThread("Worker"),
		app(appWnd),
		host(hostName),
		log(logger),
		graph(grapher)
	{
		Run();
	}

	~Worker()
	{
		Cancel();
		WaitForExit();
	}

	void OnLine(LString line)
	{
		auto p = line.SplitDelimit();
		if (p[0].Equals("reply"))
		{
			auto time = p[4].SplitDelimit("=");
			if (time[0].Equals("time"))
			{
				auto ms = time[1].Int();
				graph->AddSample((int32_t)ms);
			}
		}
		else if (line.Find("timed out") >= 0)
		{
			graph->AddSample(-1);
		}
		else log->Print("unparsed: %s\n", line.Get());
	}

	int Main()
	{
		LString args;
		#if WINDOWS
			args = LString::Fmt("/t %s", host.Get());
		#else
			args = LString::Fmt("%s", host.Get());
		#endif
		LSubProcess sub("ping", args);
		if (sub.Start())
		{
			char buf[512];
			size_t used = 0;

			while (!IsCancelled() && sub.IsRunning())
			{
				auto rd = sub.Read(buf + used, sizeof(buf) - used);
				if (rd > 0)
				{
					used += rd;

					while (auto nl = strnchr(buf, '\n', used))
					{
						size_t bytes = nl - buf;
						char *end = nl > buf && nl[-1] == '\r' ? nl - 1 : nl;
						*end = 0;

						if (buf[0])
							OnLine(buf);

						ssize_t remain = (buf + used) - ++nl;
						if (remain > 0)
							memmove(buf, nl, remain);
						used = remain;

						if (used < sizeof(buf))
							buf[used] = 0;
					}
				}
				else LSleep(10);
			}
		}
		return 0;
	}
};

class App : public LWindow
{
	LBox *vbox = nullptr;
	LTableLayout *tbl = nullptr;
	LEdit *hostname = nullptr;
	LTextLog *log = nullptr;
	Graph *graph = nullptr;
	LAutoPtr<Worker> worker;

public:
	App()
	{
		Name(AppName);
		LRect r(0, 0, 1000, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);

		if (Attach(0))
		{
			AddView(vbox = new LBox(ID_BOX, true));
			vbox->AddView(tbl = new LTableLayout(ID_TABLE));
			vbox->AddView(log = new LTextLog(ID_LOG));
			vbox->AddView(graph = new Graph(ID_GRAPH));
			tbl->GetCss(true)->Padding("0.5em");
			tbl->GetCss()->Height("2.8em");
			
			auto c = tbl->GetCell(0, 0);
				c->VerticalAlign(LCss::VerticalMiddle);
				c->Add(new LTextLabel(ID_STATIC, 0, 0, -1, -1, "hostname:"));
			c = tbl->GetCell(1, 0);
				c->Add(hostname = new LEdit(ID_HOSTNAME));
				hostname->Focus(true);
			c = tbl->GetCell(2, 0);
				c->Add(new LButton(ID_START, 0, 0, -1, -1, "Start"));

			AttachChildren();            
			Visible(true);

			worker.Reset(new Worker(this, "work", log, graph));
		}
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	LApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

