#define _CRT_SECURE_NO_WARNINGS 1

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
#include "lgi/common/OptionsFile.h"

#include "resdefs.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "PingGraph";
#define HISTORY		0

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

#define OptHostName "hostName"

class Graph : public LView, public LMutex
{
	struct Sample {
		time_t time;
		int32_t value;
	};
	LArray<Sample> samples; // lock before using

	LString logPath;
	LFile logFile;
	#if HISTORY
	int groupX = 20;
	#else
	int groupX = 1;
	#endif
	LColour cGraph = LColour(200, 200, 200);
	LColour cSamples = LColour::Blue;
	LColour cBack = LColour(L_WORKSPACE);
	LColour cHalf = cBack.Mix(cSamples);
	LArray<LString> historyDays;

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

	LString::Array DaysWithSamples()
	{
		LHashTbl<IntKey<time_t>, bool> map;
		auto s = samples.AddressOf();
		auto e = s + samples.Length();
		while (s < e)
		{
			auto t = localtime(&s->time);
			t->tm_hour = 0;
			t->tm_min = 0;
			t->tm_sec = 0;
			map.Add(mktime(t), true);
			s++;
		}

		LString::Array days;
		for (auto p: map)
		{
			auto t = localtime(&p.key);
			if (t->tm_wday > 0 && t->tm_wday < 6)
				days.Add(LString::Fmt("%i/%2.2i/%2.2i", t->tm_year+1900, t->tm_mon+1, t->tm_mday));
		}

		days.Sort([](auto a, auto b)
			{
				return Stricmp(a->Get(), b->Get());
			});
		return days;
	}

	void SetHistoryDays(LArray<LString> days)
	{
		historyDays = days;
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

	void DrawGraph(LSurface *pDC, LRect &client, time_t endTime, LString dayLabel)
	{
		if (samples.Length() == 0)
			return;

		auto fnt = GetFont();
		int markPx = 3;
		int scale = 5000; // ms
		auto draw = client;
		draw.Inset(5, fnt->GetHeight());
		draw.y2 -= fnt->GetHeight() + (markPx * 2);

		LDisplayString label(fnt, dayLabel);
		fnt->Colour(LColour(L_TEXT), cBack);
		fnt->Transparent(true);
		label.Draw(pDC, draw.x1, client.y1);
		draw.x1 += 70;

		LDisplayString ds(fnt, "5000ms");
		draw.x1 += (markPx * 2) + ds.X();
		
		pDC->Colour(cGraph);
		pDC->Line(draw.x1, draw.y1, draw.x1, draw.y2);
		pDC->Line(draw.x1, draw.y2, draw.x2, draw.y2);
		pDC->Line(draw.x2, draw.y1, draw.x2, draw.y2);
		
		LPoint prev;
		size_t i = 0;
		auto XtoTime = [&](int x)
			{
				int dx = draw.x2 - x;
				return endTime - (dx * groupX);
			};
		auto TimeToX = [&](time_t time)
			{
				int64_t sec = endTime - time;
				return (int) (draw.x2 - (sec / groupX));
			};

		auto startTime = XtoTime(draw.x1);

		// Find start of the sample range:
		for (ssize_t n=samples.Length()-1; n > 0; n--)
		{
			if (TimeToX(samples[n].time) < draw.x1)
			{
				i = n + 1;
				break;
			}
		}

		// Find the end of the sample range:
		size_t endRange = samples.Length() - 1;
		for (ssize_t n=endRange; n > 0; n--)
		{
			if (samples[n].time <= endTime)
			{
				endRange = n;
				break;
			}
		}
		#if HISTORY
		// Trim off any -1 samples...
		while (endRange > i && samples[endRange].value < 0)
			endRange--;
		#endif

		auto first = samples[i].time;
		auto prev_time = first;

		// Draw y axis
		pDC->Colour(cGraph);
		fnt->Fore(cGraph);
		fnt->Transparent(true);
		#if HISTORY
		int kInc = scale;
		#else
		int kInc = 1000;
		#endif
		for (int k = 0; k <= scale; k += kInc)
		{
			auto str = LString::Fmt("%ims", k);
			LDisplayString ds(fnt, str);
			int y = draw.y2 - (k * draw.Y() / scale);
			ds.Draw(pDC, draw.x1 - (markPx * 2) - ds.X(), y - (ds.Y()/2));
			pDC->Line(draw.x1, y, draw.x1 - markPx, y);
		}

		// Draw x axis
		int nextX = 0;
		int gridSizeMins[] = {1, 5, 15, 30, 60};
		int gridSeconds = 60 * 60;
		auto gridTxtPx = LDisplayString(fnt, "99:99").X();
		time_t now = 0;
		time(&now);
		for (int i=0; i<CountOf(gridSizeMins); i++)
		{
			auto x1 = TimeToX(now);
			auto x2 = TimeToX(now - (gridSizeMins[i] * 60));
			if (ABS(x2 - x1) > gridTxtPx)
			{
				gridSeconds = gridSizeMins[i] * 60;
				break;
			}
		}

		auto prevLabel = XtoTime(draw.x1) / gridSeconds;
		for (int x = draw.x1; x <= draw.x2; x++)
		{
			time_t unix = XtoTime(x);
			auto current = unix / gridSeconds;
			if (current != prevLabel)
			{
				prevLabel = current;
				#if WINDOWS
				struct tm localMem, *local = &localMem;
				auto localTimeErr = localtime_s(&localMem, &unix);
				if (!localTimeErr)
				#else
				struct tm *local = localtime(&unix);
				if (local)
				#endif
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
				}
			}
		}

		#if !HISTORY
		{
			LDisplayString ds(fnt, LString::Fmt("groupX=%i", groupX));
			ds.Draw(pDC, draw.x2 - 5 - ds.X(), draw.y1);
		}
		#endif

		// Draw samples
		if (groupX == 1)
		{
			pDC->Colour(cSamples);
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
			// LDateTime dt("2025/08/20 17:12:00");
			// auto debugTime = dt.GetUnix();

			for (int x=draw.x1; x<=draw.x2; x++)
			{
				int total = 0;
				int max = 0;
				int min = draw.Y();
				int count = 0;

				/*
				auto unixTime = XtoTime(x);
				if (ABS((int64_t)unixTime - debugTime) < 100)
				{
					int asd=0;
				}
				*/

				while (i < samples.Length())
				{
					auto &s = samples[i];
					if (i > endRange)
						break;

					auto sx = TimeToX(s.time);
					if (sx == x)
					{
						// add sample to grouping
						int sy = s.value >= 0 ? s.value * draw.Y() / scale : draw.Y();
						total += sy;
						min = MIN(min, sy);
						max = MAX(max, sy);
						count++;
					}
					else if (sx > x)
					{
						break;
					}

					i++;
				}

				if (count)
				{
					int avg = total / count;
					pDC->Colour(cSamples);
					pDC->Line(x, draw.y2, x, draw.y2 - min);
					pDC->Colour(cHalf);
					pDC->Line(x, draw.y2 - min, x, draw.y2 - max);
				}
			}
		}
	}

	void OnPaint(LSurface *pDC) override
	{
		#if WINDOWS
		LDoubleBuffer dblBuf(pDC);
		#endif

		Auto lck(this, _FL);
		auto client = GetClient();

		pDC->Colour(cBack);
		pDC->Rectangle();

		if (historyDays.Length())
		{
			for (unsigned i=0; i<historyDays.Length(); i++)
			{
				auto parts = historyDays[i].SplitDelimit("/");
				struct tm t = {};
				t.tm_year = (int)parts[0].Int() - 1900;
				t.tm_mon = (int)parts[1].Int() - 1;
				t.tm_mday = (int)parts[2].Int();
				t.tm_hour = 18;
				auto end = mktime(&t);

				int y1 = client.y1 + (i * client.Y() / (int)historyDays.Length());
				int y2 = client.y1 + ((i+1) * client.Y() / (int)historyDays.Length());
				LRect r(client.x1, y1, client.x2, y2-1);
				
				DrawGraph(pDC, r, end, historyDays[i]);
			}
		}
		else
		{
			auto endTime = time(NULL); // time at draw.x2
			DrawGraph(pDC, client, endTime, "live");
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
	LOptionsFile options;

public:
	App() : options(LOptionsFile::PortableMode, AppName)
	{
		Name(AppName);
		LRect r(0, 0, 1000, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);

		options.SerializeFile(false);

		if (Attach(0))
		{
			AddView(vbox = new LBox(ID_BOX, true));
			vbox->AddView(tbl = new LTableLayout(ID_TABLE));
			vbox->AddView(log = new LTextLog(ID_LOG));
			log->GetCss(true)->Height("4em");
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

			LVariant v;
			if (options.GetValue(OptHostName, v))
			{
				if (auto s = v.Str())
				{
					hostname->Name(s);

					#if HISTORY
						// scan the sample database for days with data
						LArray<LString> days = graph->DaysWithSamples();
						days.PopLast();
						days = days.Slice(-7, -1);
						for (auto d: days)
						{
							log->Print("%s\n", d.Get());
						}
						graph->SetHistoryDays(days);
					#else
						// setup the thread to live capture more samples
						worker.Reset(new Worker(this, s, log, graph));
					#endif
				}
			}
		}
	}

	~App()
	{
	}

	void Start(const char *host)
	{
		if (host)
			worker.Reset(new Worker(this, host, log, graph));
		else
			worker.Reset();
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		switch (Ctrl->GetId())
		{
			case ID_HOSTNAME:
			{
				if (n.Type == LNotifyReturnKey)
				{
					Start(Ctrl->Name());
				}
				else if (n.Type == LNotifyValueChanged)
				{
					LVariant v;
					options.SetValue(OptHostName, v = Ctrl->Name());
					options.SerializeFile(true);
				}
				break;
			}
			case ID_START:
			{
				Start(GetCtrlName(ID_HOSTNAME));
				break;
			}
		}
		
		return LWindow::OnNotify(Ctrl, n);
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

