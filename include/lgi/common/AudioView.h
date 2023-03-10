/// Audio waveform display control
#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/FileSelect.h"
#include "lgi/common/Thread.h"
#include "lgi/common/ThreadEvent.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/DropFiles.h"

#include "ao.h"

#define CC(code) LgiSwap32(code)

struct int24_t
{
	int32_t s : 24;
};

union sample_t
{
	int8_t *i8;
	int16_t *i16;
	int24_t *i24;
	int32_t *i32;
	float *f;

	sample_t(void *ptr = NULL)
	{
		*this = ptr;
	}

	sample_t &operator=(void *ptr)
	{
		i8 = (int8_t*)ptr;
		return *this;
	}
};

template<typename T>
struct Rect
{
	T x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	
	Rect<T> &operator =(const LRect &r)
	{
		x1 = r.x1;
		y1 = r.y1;
		x2 = r.x2;
		y2 = r.y2;
		return *this;
	}
	
	T X() { return x2 - x1 + 1; }
	T Y() { return y2 - y1 + 1; }

	bool Valid()
	{
		return x2 >= x1 && y2 >= y1;
	}
	
	operator LRect()
	{
		LRect r((int)x1, (int)y1, (int)x2, (int)y2);
		return r;
	}
	
	void ZOff(T sx, T sy)
	{
		x1 = y1 = 0;
		x2 = sx;
		y2 = sy;
	}
	
	const char *GetStr()
	{
		static char s[96];
		sprintf_s(s, sizeof(s),
			LPrintfInt64 "," LPrintfInt64 "," LPrintfInt64 "," LPrintfInt64,
			x1, y1, x2, y2);
		return s;
	}

	Rect &Offset(T x, T y)
	{
		x1 += x;
		x2 += x;
		y1 += y;
		y2 += y;
		return *this;
	}
};

typedef int32_t (*ConvertFn)(sample_t &ptr);

class LPopupNotification : public LWindow
{
	constexpr static int Border = 10; // px
	constexpr static int ShowMs = 3000; // milliseconds

	LColour Back = LColour(0xf7, 0xf0, 0xd5);
	LColour Fore = LColour(0xd4, 0xb8, 0x62);
	LWindow *RefWnd = NULL;
	LArray<LDisplayString*> Msgs;
	LPoint Decor = LPoint(2, 2);
	uint64_t HideTs = 0;

	LPoint CalcSize()
	{
		LPoint p;
		for (auto ds: Msgs)
		{
			p.x = MAX(p.x, ds->X());
			p.y += ds->Y();
		}

		p.x += Border * 2 + Decor.x;
		p.y += Border * 2 + Decor.y;

		return p;
	}

public:
	static LPopupNotification *Inst()
	{
		static LAutoPtr<LPopupNotification> inst;
		if (!inst)
			inst.Reset(new LPopupNotification(NULL, NULL));
		return inst;
	}

	LPopupNotification(LWindow *ref, LString msg)
	{
		SetTitleBar(false);
		Name("Notification");

		if (ref)
			RefWnd = ref;
		if (ref && msg)
			Add(ref, msg);
	}

	void Init()
	{
		if (!RefWnd)
		{
			LAssert(!"No reference window.");
			return;
		}

		auto r = RefWnd->GetPos();
		auto Sz = CalcSize();

		LRect pos;
		pos.ZOff(Sz.x - 1, Sz.y - 1);
		pos.Offset(r.x2 - pos.X(), r.y2 - pos.Y());
		SetPos(pos);

		SetPulse(500);		
		if (!IsAttached())
			Attach(0);
		Visible(true);
	}

	void Add(LWindow *ref, LString msg)
	{
		if (msg)
		{
			HideTs = LCurrentTime();
			Msgs.Add(new LDisplayString(GetFont(), msg));
		}
			
		if (ref && RefWnd != ref)
			RefWnd = ref;

		if (RefWnd && Msgs.Length() > 0)
			Init();
	}

	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(Fore);
		auto c = GetClient();
		pDC->Box(&c);
		c.Inset(1, 1);
		pDC->Colour(Back);
		pDC->Rectangle(&c);

		auto f = GetFont();
		f->Fore(Fore);
		f->Back(Back);
		f->Transparent(true);
		
		int x = c.x1 + Border;
		int y = c.y1 + Border;
		for (auto ds: Msgs)
		{
			ds->Draw(pDC, x, y);
			y += ds->Y();
		}
	}

	void OnPulse()
	{
		if (HideTs &&
			LCurrentTime() >= HideTs + ShowMs)
		{
			HideTs = 0;
			Visible(false);
			SetPulse();
			Msgs.DeleteObjects();
		}
	}
};

class LAudioView :
	public LLayout,
	public LThread,
	public LCancel,
	public LDragDropTarget
{
public:
	enum LFileType
	{
		AudioUnknown,
		AudioRaw,
		AudioWav,
	};

	enum LSampleType
	{
		AudioS8,
		AudioS16LE,
		AudioS16BE,
		AudioS24LE,
		AudioS24BE,
		AudioS32LE,
		AudioS32BE,
		AudioFloat32,
	};

	enum Messages
	{
		M_WAVE_FORMS_FINISHED = M_USER + 100,
	};
	
	struct LBookmark
	{
		int x = 0;
		bool error = false;
		int64_t sample;
		LColour colour;
	};

protected:
	enum LCmds
	{
		IDC_COPY_CURSOR,
		
		IDC_LOAD_WAV,
		IDC_LOAD_RAW,
		IDC_SAVE_WAV,
		IDC_SAVE_RAW,
		
		IDC_DRAW_AUTO,
		IDC_DRAW_LINE,
		IDC_SCAN_SAMPLES,
		IDC_SCAN_GROUPS,
		
		IDC_44K,
		IDC_48K,
		IDC_88K,
		IDC_96K,
		
		IDC_MONO,
		IDC_STEREO,
		IDC_2pt1_CHANNELS,
		IDC_5pt1_CHANNELS,
	};

	enum LDrawMode
	{
		DrawAutoSelect,
		DrawSamples,
		ScanSamples,
		ScanGroups,
	};

	enum CtrlState
	{
		CtrlNormal,
		CtrlLoading,
		CtrlBuildingWaveforms,
		CtrlSaving,
	}	State = CtrlNormal;

	LAutoPtr<LThread> Worker;

	LArray<int8_t> Audio;
	LFileType Type = AudioUnknown;
	LSampleType SampleType = AudioS16LE;
	int SampleBits = 0;
	int SampleRate = 0;
	int Channels = 0;
	size_t DataStart = 0;
	int64_t CursorSample = 0;
	Rect<int64_t> Data;
	LArray<Rect<int64_t>> ChData;
	double XZoom = 1.0;
	LString Msg, ErrorMsg;
	LDrawMode DrawMode = DrawAutoSelect;
	LArray<LBookmark> Bookmarks;
	LString FilePath;

	template<typename T>
	struct Grp
	{
		T Min = 0, Max = 0;
	};

	LArray<LArray<Grp<int32_t>>> IntGrps;
	LArray<LArray<Grp<float>>> FloatGrps;

	// Libao stuff
	int AoDriver = -1;
	bool AoPlaying = false;
	ao_device *AoDev = NULL;
	LThreadEvent AoEvent;

	void MouseToCursor(LMouse &m)
	{
		auto idx = ViewToSample(m.x);
		if (idx != CursorSample)
		{
			CursorSample = idx;
			UpdateMsg();
			Invalidate();
		}
	}

	template<typename T>
	void SetData(T &r)
	{
		Data = r;
		ChData.Length(Channels);

		auto Dy = (int)Data.Y() - 1;
		for (int i=0; i<Channels; i++)
		{
			auto &r = ChData[i];
			r = Data;
			r.y1 = (int)Data.y1 + (i * Dy / Channels);
			r.y2 = (int)Data.y1 + ((i + 1) * Dy / Channels) - 1;
		}
	}

public:
	LColour cGrid, cMax, cMin, cCursorMin, cCursorMax;

	LAudioView(const char *file = NULL) :
		LThread("LAudioView.Thread")
	{
		ao_initialize();
		cGrid.Rgb(200, 200, 200);
		cMax = LColour::Blue;
		cMin = cMax.Mix(cGrid);
		cCursorMax = LColour::Red;
		cCursorMin = cCursorMax.Mix(cGrid);

		Empty();
		Msg = "No file loaded.";

		if (file)
			Load(file);

		SetPourLargest(true);
		Focus(true);
		Run();
		SetPulse(100);
	}

	~LAudioView()
	{
		Cancel();
		Empty();

		WaitForExit();

		ao_shutdown();
	}

	const char *GetClass() override { return "LAudioView"; }

	void OnCreate()
	{
		SetWindow(this);
	}

	void OnPulse()
	{
		if (AoPlaying)
			Invalidate();
	}

	LMessage::Result OnEvent(LMessage *m)
	{
		switch (m->Msg())
		{
			case M_WAVE_FORMS_FINISHED:
				OnWaveFormsFinished();
				break;
		}

		return LView::OnEvent(m);
	}

	int WillAccept(LDragFormats &Formats, LPoint Pt, int KeyState)
	{
		Formats.SupportsFileDrops();
		return DROPEFFECT_COPY;
	}

	int OnDrop(LArray<LDragData> &Data, LPoint Pt, int KeyState)
	{
		for (auto &d : Data)
		{
			if (d.IsFileDrop())
			{
				LDropFiles df(d);
				auto w = GetWindow();
				if (w)
				{
					w->OnReceiveFiles(df);
					return DROPEFFECT_COPY;
				}
			}
		}
		
		return DROPEFFECT_NONE;
	}

	bool Empty()
	{
		Audio.Free();
		
		Type = AudioUnknown;
		SampleType = AudioS16LE;
		SampleRate = 0;
		SampleBits = 0;
		DataStart = 0;
		Channels = 0;
		CursorSample = 0;
		Data.ZOff(-1, -1);
		ChData.Empty();
		Msg.Empty();
		FilePath.Empty();

		Invalidate();

		return false;
	}

	bool Load(const char *FileName, int rate = 0, int bitDepth = 0, int channels = 0)
	{
		LFile f(FileName, O_READ);
		if (!f.IsOpen())
		{
			ErrorMsg.Printf("Can't open '%s' for reading.", FileName);
			return false;
		}

		Msg.Printf("Loading '%s'...", FileName);
		LPopupNotification::Inst()->Add(GetWindow(), Msg);


		FilePath = FileName;
		if (!Audio.Length(f.GetSize()))
		{
			ErrorMsg.Printf("Can't allocate %s.", LFormatSize(f.GetSize()).Get());
			return Empty();
		}

		auto rd = f.Read(Audio.AddressOf(), f.GetSize());
		if (rd > 0 && rd != f.GetSize())
			Audio.Length(rd);

		auto Ext = LGetExtension(FileName);
		if (!Stricmp(Ext, "wav"))
			Type = AudioWav;
		else if (!Stricmp(Ext, "raw"))
			Type = AudioRaw;
		else
		{
			ErrorMsg.Printf("Unknown format: %s", Ext);
			LPopupNotification::Inst()->Add(GetWindow(), ErrorMsg);
			return Empty();
		}
			
		if (rate)
			SampleRate = rate;

		DataStart = 0;
		SampleBits = bitDepth;
		Channels = channels;
		if (bitDepth == 16)
		{
			SampleType = AudioS16LE;
		}
		else if (bitDepth == 32)
		{
			SampleType = AudioS32LE;
		}
		else if (Type == AudioWav)
		{
			// Parse the wave file...
			LPointer p;
			p.s8 = Audio.AddressOf();
			auto end = p.s8 + Audio.Length();

			if (*p.u32++ != CC('RIFF'))
				return Empty();
			auto ChunkSz = *p.u32++;
			auto Fmt = *p.u32++;
			
			while (p.s8 < end)
			{
				auto SubChunkId = *p.u32++;
				auto SubChunkSz = *p.u32++;
				auto NextChunk = p.u8 + SubChunkSz;

				if (SubChunkId == CC('fmt '))
				{
					auto AudioFmt = *p.u16++;
					Channels = *p.u16++;
					SampleRate = *p.u32++;
					auto ByteRate = *p.u32++;
					auto BlockAlign = *p.u16++;
					SampleBits = *p.u16++;

					if (SampleBits == 16)
						SampleType = AudioS16LE;
					else if (SampleBits == 24)
						SampleType = AudioS24LE;
					else if (SampleBits == 32)
						SampleType = AudioS32LE;

					printf("Channels=%i\n", Channels);
					printf("SampleRate=%i\n", SampleRate);
					printf("SampleBits=%i\n", SampleBits);
				}
				else if (SubChunkId == CC('data'))
				{
					DataStart = p.s8 - Audio.AddressOf();
					
					printf("DataStart=" LPrintfSizeT "\n", DataStart);
					break;
				}
				
				p.u8 = NextChunk;
			}

			if (!DataStart)
			{
				ErrorMsg = "No 'data' element found.";
				LPopupNotification::Inst()->Add(GetWindow(), ErrorMsg);
				return Empty();
			}
		}
		else if (Type == AudioRaw)
		{
			// Assume 32bit
			SampleType = AudioS32LE;
			SampleBits = 32;
		}

		if (!SampleRate)
			SampleRate = 44100;
		if (!Channels)
			Channels = 2;

		Invalidate();
		LPopupNotification::Inst()->Add(GetWindow(), "Loaded ok...");
		return true;
	}

	bool Save(const char *FileName = NULL)
	{
		if (!FileName)
			FileName = FilePath;
		if (!FileName)
			return false;

		LFile f;
		if (!f.Open(FileName, O_WRITE))
		{
			ErrorMsg.Printf("Can't open '%s' for writing.", FileName);
			LPopupNotification::Inst()->Add(GetWindow(), ErrorMsg);
			return false;
		}

		LString Msg;
		Msg.Printf("Saving '%s'...", FileName);
		LPopupNotification::Inst()->Add(GetWindow(), Msg);
		f.SetSize(0);
		auto wr = f.Write(Audio.AddressOf(0), Audio.Length());
		if (wr != Audio.Length())
		{
			ErrorMsg.Printf("Write error: only wrote " LPrintfSizeT " of " LPrintfSizeT " bytes", wr, Audio.Length());
			LPopupNotification::Inst()->Add(GetWindow(), ErrorMsg);
			return false;
		}

		LPopupNotification::Inst()->Add(GetWindow(), "Saved ok.");
		return true;
	}

	LRect DefaultPos()
	{
		auto c = GetClient();
		c.y1 += LSysFont->GetHeight();
		c.Inset(10, 10);
		return c;
	}

	size_t GetSamples()
	{
		return (Audio.Length() - DataStart) / (SampleBits >> 3) / Channels;
	}

	int SampleToView(size_t idx)
	{
		return (int) (Data.x1 + ((idx * Data.X()) / GetSamples()));
	}

	size_t ViewToSample(int x /*px*/)
	{
		int64_t offset = (int64_t)x - (int64_t)Data.x1;
		int64_t samples = GetSamples();
		int64_t dx = (int64_t)Data.x2 - (int64_t)Data.x1 + 1;
		double pos = (double) offset / dx;
		int64_t idx = (int64_t)(samples * pos);
		
		printf("ViewToSample(%i) data=%s offset=" LPrintfInt64 " samples=" LPrintfInt64 " idx=" LPrintfInt64 " pos=%f\n",
			x, Data.GetStr(), offset, samples, idx, pos);
		
		if (idx < 0)
			idx = 0;
		else if (idx >= samples)
			idx = samples - 1;
		return idx;
	}

	void OnPosChange()
	{
		auto def = DefaultPos();
		LRect d = Data;
		d.y1 = def.y1;
		d.y2 = def.y2;
		SetData(d);
	}

	#define CheckItem(Sub, Name, Id, Chk) \
	{ \
		auto item = Sub->AppendItem(Name, Id); \
		if (item && Chk) item->Checked(true); \
	}

	void OnMouseClick(LMouse &m)
	{
		if (m.IsContextMenu())
		{
			LSubMenu s;
			s.AppendItem("Copy Cursor Address", IDC_COPY_CURSOR);
			s.AppendSeparator();

			auto File = s.AppendSub("File");
			File->AppendItem("Load Wav", IDC_LOAD_WAV);
			File->AppendItem("Load Raw", IDC_LOAD_RAW);
			File->AppendItem("Save Wav", IDC_SAVE_WAV);
			File->AppendItem("Save Raw", IDC_SAVE_RAW);

			auto Draw = s.AppendSub("Draw Mode");
			CheckItem(Draw, "Auto", IDC_DRAW_AUTO, DrawMode == DrawAutoSelect);
			CheckItem(Draw, "Line", IDC_DRAW_LINE, DrawMode == DrawSamples);
			CheckItem(Draw, "Scan Samples", IDC_SCAN_SAMPLES, DrawMode == ScanSamples);
			CheckItem(Draw, "Scan Groups", IDC_SCAN_GROUPS, DrawMode == ScanGroups);

			auto Rate = s.AppendSub("Sample Rate");
			CheckItem(Rate, "44.1k", IDC_44K, SampleRate == 44100);
			CheckItem(Rate, "48k", IDC_48K, SampleRate == 4800);
			CheckItem(Rate, "88.2k", IDC_88K, SampleRate == 44100*2);
			CheckItem(Rate, "96k", IDC_96K, SampleRate == 48000*2);

			auto Ch = s.AppendSub("Channels");
			CheckItem(Ch, "Mono", IDC_MONO, Channels == 1);
			CheckItem(Ch, "Stereo", IDC_STEREO, Channels == 2);
			CheckItem(Ch, "2.1", IDC_2pt1_CHANNELS, Channels == 3);
			CheckItem(Ch, "5.1", IDC_5pt1_CHANNELS, Channels == 6);

			bool Update = false;
			switch (s.Float(this, m))
			{
				case IDC_COPY_CURSOR:
				{
					LClipBoard clip(this);
					size_t addrVal = DataStart + (CursorSample * Channels * SampleBits / 8);
					LString addr;
					addr.Printf(LPrintfSizeT, addrVal);
					clip.Text(addr);
					break;
				}
				case IDC_SAVE_RAW:
				{
					auto s = new LFileSelect;
					s->Parent(this);
					s->Save([this](auto s, auto ok)
					{
						if (ok)
						{
							LFile out(s->Name(), O_WRITE);
							if (out)
							{
								out.SetSize(0);

								auto ptr = Audio.AddressOf(DataStart);
								out.Write(ptr, Audio.Length() - DataStart);
							}
							else
								LgiMsg(this, "Can't open '%s' for writing.", "Error", MB_OK, s->Name());
						}
						delete s;
					});
					break;
				}
				case IDC_DRAW_AUTO:
					DrawMode = DrawAutoSelect;
					Update = true;
					break;
				case IDC_DRAW_LINE:
					DrawMode = DrawSamples;
					Update = true;
					break;
				case IDC_SCAN_SAMPLES:
					DrawMode = ScanSamples;
					Update = true;
					break;
				case IDC_SCAN_GROUPS:
					DrawMode = ScanGroups;
					Update = true;
					break;
				case IDC_44K:
					SampleRate = 44100;
					Update = true;
					break;
				case IDC_48K:
					SampleRate = 48000;
					Update = true;
					break;
				case IDC_88K:
					SampleRate = 44100*2;
					Update = true;
					break;
				case IDC_96K:
					SampleRate = 48000*2;
					Update = true;
					break;
				case IDC_MONO:
					Channels = 1;
					Update = true;
					break;
				case IDC_STEREO:
					Channels = 2;
					Update = true;
					break;
				case IDC_2pt1_CHANNELS:
					Channels = 3;
					Update = true;
					break;
				case IDC_5pt1_CHANNELS:
					Channels = 6;
					Update = true;
					break;
				default:
					break;
			}

			if (Update)
			{
				IntGrps.Length(0);
				FloatGrps.Length(0);
				SetData(Data);
				Invalidate();
			}
		}
		else if (m.Left())
		{
			Focus(true);
			MouseToCursor(m);
		}
	}

	void OnMouseMove(LMouse &m)
	{
		if (m.Down())
			MouseToCursor(m);
	}

	bool OnKey(LKey &k)
	{
		k.Trace("key");

		if (k.IsChar)
		{
			switch (k.c16)
			{
				case 'f':
				case 'F':
				{
					if (k.Down())
						FindErrors();
					return true;
				}
				case 'r':
				case 'R':
				{
					if (k.Down())
						RepairNext();
					return true;
				}
				case 'a':
				case 'A':
				{
					if (k.Down())
						RepairAll();
					return true;
				}
				case ' ':
				{
					if (k.Down())
						TogglePlay();
					return true;
				}
			}
		}
		else if (k.Ctrl())
		{
			if (k.c16 == 'S')
			{
				if (k.Down())
					Save();
				return true;
			}
			else if (k.c16 == 'W')
			{
				if (k.Down())
					Empty();
				return true;
			}
		}

		return false;
	}

	sample_t AddressOf(size_t Sample)
	{
		int SampleBytes = SampleBits >> 3;
		return sample_t(Audio.AddressOf(DataStart + (Sample * Channels * SampleBytes)));
	}

	void RepairBookmark(LBookmark &bm)
	{
		auto Convert = GetConvert();
		auto end = Audio.AddressOf(Audio.Length()-1) + 1;

		// Look at previous sample...
		auto repairSample = bm.sample;
		auto p = AddressOf(repairSample - 1);
		auto lastGood = Convert(p);

		// Seek over and find the end...
		auto nextGood = Convert(p);
		while (nextGood)
		{
			nextGood = Convert(p);
			repairSample++;
		}

		auto endRepair = repairSample;
		while (nextGood == 0)
		{
			nextGood = Convert(p);
			endRepair++;
		}

		int64_t sampleDiff = nextGood - lastGood;
		int64_t repairSamples = endRepair - repairSample + 1;

		// Write the new interpolated samples
		for (int64_t i = repairSample; i < endRepair; i++)
		{
			p = AddressOf(i);
			if (p.i8 >= end)
				break;

			switch (SampleType)
			{
				case AudioS24LE:
				{
					auto off = i - repairSample + 1;
					p.i24->s = lastGood + (off * sampleDiff / repairSamples);
					break;
				}
				default:
				{
					LAssert(!"Impl me.");
					break;
				}
			}
		}

		bm.colour = LColour::Green;
		bm.error = false;
	}

	void RepairNext()
	{
		int64_t prev = 0;
		LBookmark *Bookmark = NULL;
		for (auto &bm: Bookmarks)
		{
			if (bm.sample >= CursorSample &&
				prev < CursorSample &&
				bm.error)
			{
				Bookmark = &bm;
				break;
			}
		}

		if (!Bookmark)
			return;

		RepairBookmark(*Bookmark);
		Invalidate();
	}

	void RepairAll()
	{
		for (auto &bm: Bookmarks)
			RepairBookmark(bm);
		Invalidate();
	}

	int64_t PtrToSample(sample_t &ptr)
	{
		int sampleBytes = SampleBits >> 3;
		auto start = Audio.AddressOf(DataStart);
		return (ptr.i8 - start) / (sampleBytes * Channels);
	}

	void FindErrors()
	{
		auto Convert = GetConvert();
		auto ptr = AddressOf(0);
		auto start = ptr.i8;
		auto end = Audio.AddressOf(Audio.Length()-1) + 1;
		int32_t prev = 0;
		int sampleBytes = SampleBits / 8;
		int byteInc = (Channels - 1) * sampleBytes;
		int threshold = 180 * 1000;
		int maxDiff = 0;
		int64_t zeroRunStart = -1;

		while (ptr.i8 < end)
		{
			auto s = Convert(ptr);
			auto diff = s - prev;
			if (diff < 0)
				diff = -diff;

			if (s == 0)
			{
				if (zeroRunStart < 0)
					zeroRunStart = PtrToSample(ptr) - 1;
			}
			else if (zeroRunStart >= 0)
			{
				auto cur = PtrToSample(ptr) - 1;
				auto len = cur - zeroRunStart;
				if (len >= 12)
				{
					// emit book mark
					auto &bm = Bookmarks.New();
					bm.sample = zeroRunStart;
					bm.colour = LColour::Red;
					bm.error = true;

					if (Bookmarks.Length() >= 10000)
						break;
				}

				zeroRunStart = -1;
			}			

			/*
			if (diff > threshold && s == 0)
			{
				// emit book mark
				auto &bm = Bookmarks.New();
				bm.sample = PtrToSample(ptr) - 1;
				bm.colour = LColour::Red;
				bm.error = true;

				if (Bookmarks.Length() >= 5000)
					break;
			}
			*/

			maxDiff = MAX(diff, maxDiff);
			prev = s;
			ptr.i8 += byteInc;
		}

		Invalidate();
	}

	void UpdateMsg()
	{
		ssize_t Samples = GetSamples();
		auto Addr = AddressOf(CursorSample);
		LString::Array Val;
		size_t GraphLen = IntGrps.Length() ? IntGrps[0].Length() : 0;
		auto Convert = GetConvert();

		// Work out the time stamp of the cursor:
		auto Seconds = CursorSample / SampleRate;
		auto Hours = Seconds / 3600;
		Seconds -= Hours * 3600;
		auto Minutes = Seconds / 60;
		Seconds -= Minutes * 60;
		auto Ms = (CursorSample % SampleRate) * 1000 / SampleRate;
		LString Time;
		Time.Printf("%i:%02.2i:%02.2i.%i", (int)Hours, (int)Minutes, (int)Seconds, (int)Ms);

		for (int ch=0; ch<Channels; ch++)
			Val.New().Printf("%i", Convert(Addr));

		Msg.Printf("Channels=%i SampleRate=%i BitDepth=%i XZoom=%g Samples=" LPrintfSSizeT " Cursor=" LPrintfSizeT " @ %s (%s) Graph=" LPrintfSizeT "\n",
			Channels, SampleRate, SampleBits,
			XZoom, Samples, CursorSample, Time.Get(), LString(",").Join(Val).Get(), GraphLen);
	}

	bool OnMouseWheel(double Lines)
	{
		LMouse m;
		GetMouse(m);

		auto Samples = GetSamples();
		if (m.Ctrl())
		{
			auto DefPos = DefaultPos();
			auto change = (double)Lines / 2;
			XZoom *= 1.0 + (-change / 4);
			if (XZoom < 1.0)
				XZoom = 1.0;

			auto OldX = Data.X();
			int64_t x = (int64_t)(DefPos.X() * XZoom);
			auto CursorX = SampleToView(CursorSample);
			auto d = Data;
			d.x1 = CursorX - (CursorSample * x / Samples);
			d.x2 = d.x1 + x - 1;
			SetData(d);

			Invalidate();
		}
		else
		{
			auto Client = GetClient();
			int change = (int)(Lines * -6);
			auto d = Data;
			d.x1 += change;
			d.x2 += change;
			if (d.x2 < Client.x1)
				d.Offset(-d.x2, 0);
			else if (d.x1 >= Client.x2)
				d.Offset(-(d.x2 - Client.x2), 0);
			SetData(d);

			Invalidate();
		}

		UpdateMsg();

		return true;
	}

	#define PROFILE_PAINT_SAMPLES 0
	#if PROFILE_PAINT_SAMPLES
		#define PROF(name) Prof.Add(name)
	#else
		#define PROF(name)
	#endif

	ConvertFn GetConvert()
	{
		switch (SampleType)
		{
			case AudioS8:
				return [](sample_t &s) -> int32_t
				{
					return *s.i8++;
				};
			case AudioS16LE:
				return [](sample_t &s) -> int32_t
				{					
					return *s.i16++;
				};
			case AudioS16BE:
				return [](sample_t &s) -> int32_t
				{
					int16_t i = LgiSwap16(*s.i16);
					s.i16++;
					return i;
				};
			case AudioS24LE:
				return [](sample_t &s) -> int32_t
				{
					int32_t i = s.i24->s;
					s.i8 += 3;
					return i;
				};
			case AudioS24BE:
				return [](sample_t &s) -> int32_t
				{
					int24_t i;
					int8_t *p = (int8_t*)&i;
					p[0] = s.i8[2];
					p[1] = s.i8[1];
					p[2] = s.i8[0];
					s.i8 += 3;
					return i.s;
				};
			case AudioS32LE:
				return [](sample_t &s) -> int32_t
				{
					return *s.i32++;
				};
			case AudioS32BE:
				return [](sample_t &s) -> int32_t
				{
					int32_t i = *s.i32++;
					return LgiSwap32(i);
				};
		}

		return NULL;
	}

private:
	constexpr static int WaveformBlockSize = 256;

	struct WaveFormWork
	{
		ConvertFn convert;
		sample_t start;
		int8_t *end = NULL;
		int stepBytes = 0;
		int blkBytes = 0;
		size_t blocks = 0;
		Grp<int32_t> *out = NULL;
	};
	LArray<WaveFormWork*> WaveFormTodo;
	LArray<LThread*> WaveFormThreads;

	struct WaveFormThread : public LThread, public LCancel
	{
		LAudioView *view;

		WaveFormThread(LAudioView *v) : LThread("WaveFormThread", v->AddDispatch())
		{
			view = v;
			Run();
		}

		~WaveFormThread()
		{
			Cancel();
		}

		void OnComplete()
		{
			// Clean up thread array...
			LThread *t = this;
			LAssert(view->WaveFormThreads.HasItem(t));
			view->WaveFormThreads.Delete(t);
			DeleteOnExit = true;

			// Last one finished?
			if (view->WaveFormThreads.Length() == 0)
				view->PostEvent(M_WAVE_FORMS_FINISHED);
		}

		int Main()
		{
			while (!IsCancelled())
			{
				WaveFormWork *w = NULL;

				if (view->Lock(_FL, 500))
				{
					// Is there work?
					if (view->WaveFormTodo.Length() > 0)
					{
						w = view->WaveFormTodo[0];
						view->WaveFormTodo.DeleteAt(0);
					}

					view->Unlock();

					if (!w)
						return 0;
				}
				
				auto s = w->start;
				auto block = w->out;
				auto convert = w->convert;

				// For all blocks we're assigned...
				for (int b = 0;
					b < w->blocks && !IsCancelled();
					b++, s.i8 += w->blkBytes, block++)
				{
					auto ptr = s;
					auto blkEnd = s.i8 + w->blkBytes;
					auto stepBytes = w->stepBytes;

					// For all samples in the block...
					block->Min = block->Max = convert(ptr); // init min/max with first sample
					while (ptr.i8 < blkEnd)
					{
						auto n = convert(ptr);
						if (n < block->Min)
							block->Min = n;
						if (n > block->Max)
							block->Max = n;
						
						ptr.i8 += stepBytes;
					}
				}
			}

			return 0;
		}
	};

public:
	void BuildWaveForms(LArray<LArray<Grp<int32_t>>> *Grps)
	{
		LPopupNotification::Inst()->Add(GetWindow(), "Building waveform data...");
		State = CtrlBuildingWaveforms;

		auto SampleBytes = SampleBits / 8;
		auto start = Audio.AddressOf(DataStart);
		size_t samples = GetSamples();
		auto end = start + (SampleBytes * samples * Channels);
		auto BlkBytes = WaveformBlockSize * Channels * SampleBytes;
		auto Blocks = ((Audio.Length() - DataStart) + BlkBytes - 1) / BlkBytes;
		int Threads = LAppInst->GetCpuCount();

		// Initialize the graph
		Grps->Length(Channels);
		for (int ch = 0; ch < Channels; ch++)
		{
			auto &GraphArr = (*Grps)[ch];
			GraphArr.Length(Blocks);

			for (int Thread = 0; Thread < Threads; Thread++)
			{
				auto from = Thread       * Blocks / Threads;
				auto to   = (Thread + 1) * Blocks / Threads;

				WaveFormWork *work = new WaveFormWork;
				work->convert   = GetConvert();
				work->start.i8  = start + (from * BlkBytes) + (ch * SampleBytes);
				work->end       = start + (to   * BlkBytes) + (ch * SampleBytes);
				work->stepBytes = (Channels - 1) * SampleBytes;
				work->blkBytes  = BlkBytes;
				work->blocks    = to - from;
				work->out       = GraphArr.AddressOf(from);
				
				WaveFormTodo.Add(work);
			}
		}

		for (int Thread = 0; Thread < Threads; Thread++)
			WaveFormThreads.Add(new WaveFormThread(this));
	}

	void OnWaveFormsFinished()
	{
		State = CtrlNormal;
		LPopupNotification::Inst()->Add(GetWindow(), "Done.");
		Invalidate();
	}

	void PaintSamples(LSurface *pDC, int ChannelIdx, LArray<LArray<Grp<int32_t>>> *Grps)
	{
		#if PROFILE_PAINT_SAMPLES
		LProfile Prof("PaintSamples", 10);
		#endif

		int32_t MaxT = 0;
		ConvertFn Convert = GetConvert();

		if (SampleBits == 8)
			MaxT = 0x7f;
		else if (SampleBits == 16)
			MaxT = 0x7fff;
		else if (SampleBits == 24)
			MaxT = 0x7fffff;
		else
			MaxT = 0x7fffffff;

		auto SampleBytes = SampleBits / 8;
		auto Client = GetClient();
		auto &c = ChData[ChannelIdx];
		auto start = Audio.AddressOf(DataStart);
		size_t samples = GetSamples();
		// printf("samples=" LPrintfSizeT "\n", samples);
		auto end = start + (SampleBytes * samples * Channels);
		auto cy = c.y1 + (c.Y() / 2);

		pDC->Colour(cGrid);
		pDC->Box(&(LRect)c);

		if (Grps->Length() == 0)
		{
			BuildWaveForms(Grps);
			return;
		}
		else if (State != CtrlNormal)
		{
			return;
		}

		PROF("PrePaint");

		auto YScale = c.Y() / 2;
		int CursorX = SampleToView(CursorSample);
		double blkScale = Grps->Length() > 0 ? (double) (*Grps)[0].Length() / c.X() : 1.0;
		// printf("blkScale=%f blks/px\n", blkScale);
		LDrawMode EffectiveMode = DrawMode;
		auto StartSample = ViewToSample(Client.x1);
		auto EndSample = ViewToSample(Client.x2);
		auto SampleRange = EndSample - StartSample;

		if (EffectiveMode == DrawAutoSelect)
		{
			if (SampleRange < Client.X() * 32)
				EffectiveMode = DrawSamples;
			else if (blkScale < 1.0)
				EffectiveMode = ScanSamples;
			else
				EffectiveMode = ScanGroups;
		}
	
		// This is the bytes in channels we're not looking at
		int byteInc = (Channels - 1) * SampleBytes;

		pDC->Colour(cGrid);
		pDC->HLine((int)MAX(0,c.x1), (int)MIN(pDC->X(),c.x2), (int)cy);

		// Setup for drawing bookmarks..
		for (auto &bm: Bookmarks)
			bm.x = SampleToView(bm.sample);
		auto nextBookmark = Bookmarks.Length() ? Bookmarks.AddressOf(0) : NULL;
		auto endBookmark = Bookmarks.Length() ? nextBookmark + Bookmarks.Length() : NULL;

		if (EffectiveMode == DrawSamples)
		{
			sample_t pSample(start + (((StartSample * Channels) + ChannelIdx) * SampleBytes));
			bool DrawDots = SampleRange < (Client.X() >> 2);

			if (CursorX >= Client.x1 && CursorX <= Client.x2)
			{
				pDC->Colour(L_BLACK);
				pDC->VLine(CursorX, (int)c.y1, (int)c.y2);
			}

			while (	nextBookmark && nextBookmark < endBookmark)
			{
				pDC->Colour(nextBookmark->colour);
				pDC->VLine(nextBookmark->x, (int)c.y1, (int)c.y2);
				nextBookmark++;
			}

			// For all samples in the view space...
			pDC->Colour(cMax);
			LPoint Prev(-1, -1);
			for (auto idx = StartSample; idx <= EndSample + 1; idx++, pSample.i8 += byteInc)
			{
				auto n = Convert(pSample);
				auto x = SampleToView(idx);
				auto y = (cy - ((int64_t)n * YScale / MaxT));
				if (idx != StartSample)
					pDC->Line(Prev.x, Prev.y, (int)x, (int)y);
				Prev.Set((int)x, (int)y);
				if (DrawDots)
					pDC->Rectangle((int)x-1, (int)y-1, (int)x+1, (int)y+1);
			}
		}
		else
		{
			// For all x coordinates in the view space..
			for (int Vx=Client.x1; Vx<=Client.x2; Vx++)
			{
				if (Vx % 100 == 0)
				{
					PROF("Pixels");
				}

				if (Vx < c.x1 || Vx > c.x2)
					continue;

				if (nextBookmark && nextBookmark->x <= Vx)
				{
					while (	nextBookmark < endBookmark &&
							nextBookmark->x <= Vx)
					{
						pDC->Colour(nextBookmark->colour);
						pDC->VLine(nextBookmark->x, (int)c.y1, (int)c.y2);
						nextBookmark++;
					}
				}

				auto isCursor = CursorX == Vx;
				if (isCursor)
				{
					pDC->Colour(L_BLACK);
					pDC->VLine(Vx, (int)c.y1, (int)c.y2);
				}
				
				Grp<int32_t> Min, Max;
				StartSample = ViewToSample(Vx);
				EndSample = ViewToSample(Vx+1);				
				// printf("SampleIdx: " LPrintfSizeT "-" LPrintfSizeT "\n", StartSample, EndSample);
				
				if (EffectiveMode == ScanSamples)
				{
					// Scan individual samples
					sample_t pStart(start + ((StartSample * Channels + ChannelIdx) * SampleBytes));
					auto pEnd = start + ((EndSample * Channels + ChannelIdx) * SampleBytes);
					
					#if 0
					if (Vx==Client.x1)
					{
						auto PosX = SampleToView((pStart - ChannelIdx - start) / Channels);
						LgiTrace("	ptr=%i " LPrintfSizeT "-" LPrintfSizeT " x=%i pos=%i\n", (int)((char*)pStart-(char*)start), StartSample, EndSample, Vx, PosX);
					}
					#endif
					
					for (auto i = pStart; i.i8 < pEnd; i.i8 += byteInc)
					{
						auto n = Convert(i);
						Max.Min = MIN(Max.Min, n);
						Max.Max = MAX(Max.Max, n);
					}
				}
				else
				{
					// Scan the buckets
					auto &Graph = (*Grps)[ChannelIdx];
					double blkStart = (double)StartSample * Graph.Length() / samples;
					double blkEnd = (double)EndSample * Graph.Length() / samples;
					/*
					if (Vx % 10 == 0)
						printf("BlkRange[%i]: %g-%g of:%i\n", Vx, blkStart, blkEnd, (int)Graph.Length());
					*/
				
					#if 0
					if (isCursor)
						LgiTrace("Blk %g-%g of " LPrintfSizeT "\n", blkStart, blkEnd, Graph.Length());
					#endif

					auto iEnd = (int)ceil(blkEnd);
					int count = 0;
					for (int i=(int)floor(blkStart); i<iEnd; i++, count++)
					{
						auto &b = Graph[i];
						Min.Min += b.Min;
						Min.Max += b.Max;
						Max.Min = MIN(Max.Min, b.Min);
						Max.Max = MAX(Max.Max, b.Max);
					}
					if (count)
					{
						Min.Min /= count;
						Min.Max /= count;
					}
				}
			
				auto y1 = (cy - (Max.Min * YScale / MaxT));
				auto y2 = (cy - (Max.Max * YScale / MaxT));
				/*
				printf("y=%i,%i r=%i,%i scale=%i max=%i\n",
					(int)y1, (int)y2,
					Min.Min, Min.Max,
					YScale, MaxT);
				*/
				pDC->Colour(isCursor ? cCursorMax : cMax);
				pDC->VLine(Vx, (int)y1, (int)y2);

				if (EffectiveMode == ScanGroups)
				{
					y1 = (cy - (Min.Max * YScale / MaxT));
					y2 = (cy - (Min.Min * YScale / MaxT));
					pDC->Colour(isCursor ? cCursorMin : cMin);
					pDC->VLine(Vx, (int)y1, (int)y2);
				}
			}
		}
	}

	void OnPaint(LSurface *pDC)
	{
		#ifdef WINDOWS
		LDoubleBuffer DblBuf(pDC);
		#endif
		pDC->Colour(L_WORKSPACE);
		pDC->Rectangle();

		auto Fnt = GetFont();
		Fnt->Transparent(true);
		if (!FilePath)
		{
			LDisplayString ds(Fnt, Msg);
			Fnt->Fore(L_LOW);
			auto c = GetClient().Center();
			ds.Draw(pDC, c.x - (ds.X()/2), c.y - (ds.Y()/2));
			return;
		}

		if (!Data.Valid())
		{
			auto r = DefaultPos();
			SetData(r);
		}

		LDisplayString ds(Fnt, ErrorMsg ? ErrorMsg : Msg);
		Fnt->Fore(ErrorMsg ? LColour::Red : LColour(L_LOW));
		ds.Draw(pDC, 4, 4);
		if (ErrorMsg)
			return;

		switch (SampleType)
		{
			case AudioS16LE:
			case AudioS16BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples(pDC, ch, &IntGrps);
				break;
			}
			case AudioS24LE:
			case AudioS24BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples(pDC, ch, &IntGrps);
				break;
			}
			case AudioS32LE:
			case AudioS32BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples(pDC, ch, &IntGrps);
				break;
			}
			case AudioFloat32:
			{
				/*
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples(pDC, ch, &FloatGrps);
				*/
				break;
			}
		}
	}

	void TogglePlay()
	{
		if (AoPlaying)
		{
			AoPlaying = false;
		}
		else // Start playback
		{
			if (AoDriver < 0)
			{
				AoDriver = ao_default_driver_id();
			}

			if (!AoDev)
			{
				ao_sample_format fmt;

				fmt.bits = SampleBits;
				fmt.rate = SampleRate;
				fmt.channels = Channels;
				fmt.byte_format = AO_FMT_LITTLE;
				fmt.matrix = NULL;

				AoDev = ao_open_live(AoDriver, &fmt, NULL);
			}

			if (AoDev)
			{
				AoPlaying = true;
				AoEvent.Signal();
			}
		}
	}

	int Main()
	{
		int TimeSlice = 50; // ms
		int sampleBytes = SampleBits / 8;
		int sliceSamples = SampleRate * TimeSlice / 1000;
		int sliceBytes = sliceSamples * Channels * sampleBytes;

		while (!IsCancelled())
		{
			if (AoPlaying)
			{
				// Play one timeslice of audio
				auto addr = AddressOf(CursorSample);
				ao_play(AoDev, (char*)addr.i8, sliceBytes);
				CursorSample += sliceSamples;
			}
			else
			{
				AoEvent.Wait(100);
			}
		}				

		return 0;
	}

	bool UnitTests()
	{		
		SampleType = AudioS16LE;
		auto c = GetConvert();
		int16_t le16 = (2 << 8) | (1);
		sample_t ptr(&le16);
		auto out = c(ptr);
		if (out != 0x201)
		{
			LAssert(0);
			return false;
		}

		SampleType = AudioS16BE;
		c = GetConvert();
		int16_t be16 = (1 << 8) | (2);
		ptr = &be16;
		out = c(ptr);
		if (out != 0x201)
		{
			LAssert(0);
			return false;
		}

		SampleType = AudioS24LE;
		c = GetConvert();
		uint8_t le24[] = {1, 2, 3};
		ptr = &le24;
		out = c(ptr);
		if (out != 0x30201)
		{
			LAssert(0);
			return false;
		}

		SampleType = AudioS24BE;
		c = GetConvert();
		uint8_t be24[] = {3, 2, 1};
		ptr = &be24;
		out = c(ptr);
		if (out != 0x30201)
		{
			LAssert(0);
			return false;
		}

		SampleType = AudioS32LE;
		c = GetConvert();
		uint8_t le32[] = {1, 2, 3, 4};
		ptr = &le32;
		out = c(ptr);
		if (out != 0x4030201)
		{
			LAssert(0);
			return false;
		}

		SampleType = AudioS32BE;
		c = GetConvert();
		uint8_t be32[] = {4, 3, 2, 1};
		ptr = &be32;
		out = c(ptr);
		if (out != 0x4030201)
		{
			LAssert(0);
			return false;
		}

		return true;
	}
};

