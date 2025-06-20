#include "lgi/common/Lgi.h"
#include "lgi/common/Box.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/Button.h"
#include "lgi/common/Thread.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/TextLog.h"
#include "lgi/common/DisplayString.h"

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
		time_t time;
		int32_t value;
	};
	LArray<Sample> samples; // lock before using

	LString logPath;
	LFile logFile;
	int groupX = 1;

public:
	Graph(int id) : LMutex("Graph")
	{
		SetId(id);

		LFile::Path p(LSP_APP_INSTALL);
		logPath = p / "log.dat";
		if (logFile.Open(logPath, O_READWRITE))
		{
			auto sz = sizeof(Sample);
			auto count = logFile.GetSize() / sz;
			if (samples.Length(count))
			{
				auto rd = logFile.Read(samples.AddressOf(), samples.Length() * sz);
			}
		}
	}

	bool OnMouseWheel(double Lines)
	{
		int add = (int)(Lines / 3.0);
		int newGrp = groupX + add;
		if (newGrp < 1)
			newGrp = 1;
		if (newGrp != groupX)
		{
			groupX = newGrp;
			Invalidate();
		}

		return true;
	}

	void AddSample(int32_t ms)
	{
		{
			Auto lck(this, _FL);
			auto &s = samples.New();
			s.time = time(NULL);
			s.value = ms;

			if (logFile)
				logFile.Write(&s, sizeof(s));
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

		auto fnt = GetFont();
		int markPx = 3;
		int scale = 5000; // ms
		auto client = GetClient();
		auto draw = client;
		draw.Inset(5, fnt->GetHeight());
		draw.y2 -= fnt->GetHeight() + (markPx * 2);
		LDisplayString ds(fnt, "5000ms");
		draw.x1 += (markPx * 2) + ds.X();
		
		LColour cGraph(222, 222, 222);
		pDC->Colour(cGraph);
		pDC->Line(draw.x1, draw.y1, draw.x1, draw.y2);
		pDC->Line(draw.x1, draw.y2, draw.x2, draw.y2);
		pDC->Line(draw.x2, draw.y1, draw.x2, draw.y2);
		
		LPoint prev;
		auto now = time(NULL); // time at draw.x2
		size_t i = 0;
		auto XtoTime = [&](int x)
			{
				int dx = draw.x2 - x;
				return now - (dx * groupX);
			};
		auto TimeToX = [&](time_t time)
			{
				int64_t sec = now - time;
				return (int) (draw.x2 - (sec / groupX));
			};

		auto startTime = XtoTime(draw.x1);
		for (ssize_t n=samples.Length()-1; n > 0; n--)
		{
			if (TimeToX(samples[n].time) < draw.x1)
			{
				i = n + 1;
				break;
			}
		}
		auto first = samples[i].time;
		auto prev_time = first;

		// Draw y axis
		pDC->Colour(cGraph);
		fnt->Fore(cGraph);
		fnt->Transparent(true);
		for (int k = 0; k <= scale; k += 1000)
		{
			auto str = LString::Fmt("%ims", k);
			LDisplayString ds(fnt, str);
			int y = draw.y2 - (k * draw.Y() / scale);
			ds.Draw(pDC, draw.x1 - (markPx * 2) - ds.X(), y - (ds.Y()/2));
			pDC->Line(draw.x1, y, draw.x1 - markPx, y);
		}

		// Draw x axis
		int nextX = 0;
		int prevMin = 0;
		for (int x = draw.x1; x <= draw.x2; x++)
		{
			time_t unix = XtoTime(x);
			struct tm *local = localtime(&unix);
			if (local && local->tm_min != prevMin)
			{
				auto str = LString::Fmt("%i:%2.2i", local->tm_hour, local->tm_min);
				LDisplayString ds(fnt, str);
				auto drawX = x - (ds.X()/2);
				if (!nextX || drawX >= nextX)
				{
					ds.Draw(pDC, x - (ds.X()/2), draw.y2 + (markPx*2));
					nextX = x + ds.X();
					pDC->Line(x, draw.y2, x, draw.y2+markPx);
				}
				prevMin = local->tm_min;
			}
		}

		{
			LDisplayString ds(fnt, LString::Fmt("groupX=%i", groupX));
			ds.Draw(pDC, draw.x2 - 5 - ds.X(), draw.y1);
		}

		// Draw samples
		if (groupX == 1)
		{
			pDC->Colour(LColour::Blue);		
			for (; i<samples.Length(); i++)
			{
				auto &s = samples[i];
				int x = TimeToX(s.time);
				int y = s.value >= 0 ? s.value * draw.Y() / scale : draw.Y();
				LPoint pt(x,
						  draw.y2 - y);
				if (prev.x && s.time - prev_time < 10)			
					pDC->Line(prev.x, prev.y, pt.x, pt.y);
				prev = pt;
				prev_time = s.time;
			}
		}
		else
		{
			pDC->Colour(LColour::Blue);		
			for (int x=draw.x1; x<=draw.x2; x++)
			{
				int min = scale;
				int max = 0;
				int count = 0;

				while (i < samples.Length())
				{
					auto &s = samples[i];
					auto sx = TimeToX(s.time);
					if (sx == x)
					{
						// add sample to grouping
						int sy = s.value >= 0 ? s.value * draw.Y() / scale : draw.Y();
						min = MIN(min, sy);
						max = MAX(max, sy);
						count++;
					}
					else if (sx > x)
						break;

					i++;
				}

				if (count)
					pDC->Line(x, draw.y2 - min, x, draw.y2 - max);
			}
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

