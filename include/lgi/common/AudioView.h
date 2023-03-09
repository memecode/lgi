/// Audio waveform display control
#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/FileSelect.h"

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
	
	T X() { x2 - x1 + 1; }
	T Y() { y2 - y1 + 1; }

	bool Valid()
	{
		return x2 >= x1 && y2 >= y1;
	}
	
	operator LRect()
	{
		LRect r(x1, y1, x2, y2);
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
};

typedef int32_t (*ConvertFn)(sample_t &ptr);

class LAudioView : public LLayout
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

	LArray<int8_t> Audio;
	LFileType Type = AudioUnknown;
	LSampleType SampleType = AudioS16LE;
	int SampleBits = 0;
	int SampleRate = 0;
	int Channels = 0;
	size_t DataStart = 0;
	size_t CursorSample = 0;
	Rect<int64_t> Data;
	LArray<LRect> ChData;
	double XZoom = 1.0;
	LString Msg, ErrorMsg;
	LDrawMode DrawMode = DrawAutoSelect;

	template<typename T>
	struct Grp
	{
		T Min = 0, Max = 0;
	};

	LArray<LArray<Grp<int32_t>>> IntGrps;
	LArray<LArray<Grp<float>>> FloatGrps;

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

		int Dy = Data.Y() - 1;
		for (int i=0; i<Channels; i++)
		{
			auto &r = ChData[i];
			r = Data;
			r.y1 = Data.y1 + (i * Dy / Channels);
			r.y2 = Data.y1 + ((i + 1) * Dy / Channels) - 1;
		}
	}

public:
	LColour cGrid, cMax, cMin, cCursorMin, cCursorMax;

	LAudioView(const char *file = NULL)
	{
		cGrid.Rgb(200, 200, 200);
		cMax = LColour::Blue;
		cMin = cMax.Mix(cGrid);
		cCursorMax = LColour::Red;
		cCursorMin = cCursorMax.Mix(cGrid);

		Empty();
		if (file)
			Load(file);
	}

	~LAudioView()
	{
		Empty();
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
		return Data.x1 + (int)((idx * Data.X()) / GetSamples());
	}

	size_t ViewToSample(int x /*px*/)
	{
		int64_t offset = (int64_t)x - (int64_t)Data.x1;
		int64_t samples = GetSamples();
		int64_t dx = (int64_t)Data.x2 - (int64_t)Data.x1 + 1;
		double pos = (double) offset / dx;
		int64_t idx = samples * pos;
		
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

	LPointer AddressOf(size_t Sample)
	{
		LPointer p;
		int SampleBytes = SampleBits >> 3;
		p.s8 = Audio.AddressOf(DataStart + (Sample * Channels * SampleBytes));
		return p;
	}

	void UpdateMsg()
	{
		ssize_t Samples = GetSamples();
		auto Addr = AddressOf(CursorSample);
		size_t AddrOffset = Addr.s8 - Audio.AddressOf();
		LString::Array Val;
		size_t GraphLen = 0;
		for (int ch=0; ch<Channels; ch++)
		{
			/*
			switch (SampleType)
			{
				case AudioS16LE:
				case AudioS16BE:
					Val.New().Printf("%i", Native(Addr.s16[ch]));
					GraphLen = Int16Grps.Length() ? Int16Grps[0].Length() : 0;
					break;
				case AudioS32LE:
				case AudioS32BE:
					Val.New().Printf("%i", Native(Addr.s32[ch]));
					GraphLen = Int32Grps.Length() ? Int32Grps[0].Length() : 0;
					break;
			}
			*/
		}

		Msg.Printf("Channels=%i SampleRate=%i BitDepth=%i XZoom=%g Samples=" LPrintfSSizeT " Cursor=" LPrintfSizeT " @ " LPrintfSizeT " (%s) Graph=" LPrintfSizeT "\n",
			Channels, SampleRate, SampleBits,
			XZoom, Samples, CursorSample, AddrOffset, LString(",").Join(Val).Get(), GraphLen);
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

			int OldX = Data.X();
			int x = (int)(DefPos.X() * XZoom);
			int CursorX = SampleToView(CursorSample);
			LRect d = Data;
			d.x1 = (int) (CursorX - (CursorSample * x / Samples));
			d.x2 = d.x1 + x - 1;
			SetData(d);

			Invalidate();
		}
		else
		{
			int change = (int)(Lines * -6);
			LRect d = Data;
			d.x1 += change;
			d.x2 += change;
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
		int cy = c.y1 + (c.Y() / 2);

		pDC->Colour(cGrid);
		pDC->Box(&c);

		int blk = 256;
		if (Grps->Length() == 0)
		{
			// Initialize the graph
			PROF("GraphGen");
			Grps->Length(Channels);
			for (int ch = 0; ch < Channels; ch++)
			{
				auto &GraphArr = (*Grps)[ch];
				GraphArr.SetFixedLength(false);
				GraphArr.Length(0);

				int BlkBytes = blk * Channels * SampleBytes;
				printf("BlkBytes=%i\n", BlkBytes);
				
				size_t BlkIndex = 0;
				int byteOffsetToSample = ch * SampleBytes;
				printf("byteOffsetToSample=%i\n", byteOffsetToSample);
				printf("SampleBytes=%i\n", SampleBytes);

				sample_t s;
				s.i8 = start+byteOffsetToSample;

				// For each block...
				for (; s.i8 < end; s.i8 += BlkBytes)
				{
					int remain = MIN( (int)(end - s.i8), BlkBytes );
					auto &b = GraphArr[BlkIndex++];

					// For all samples in the block...
					int8_t *blkStart = s.i8;
					int8_t *blkEnd = s.i8 + remain;
					auto smp = s;
					b.Min = b.Max = Convert(smp); // init min/max with first sample
					while (smp.i8 < blkEnd)
					{
						auto n = Convert(smp);
						if (n < b.Min)
							b.Min = n;
						if (n > b.Max)
							b.Max = n;
					}
					
					/*					
					if (BlkIndex % 1000 == 0)
						printf("bucket=%i %i-%i\n", (int)BlkIndex-1, b.Min, b.Max);
						*/
				}
				
				printf("end diff=%i\n", (int) (end - s.i8));
				printf("GraphArr.len=" LPrintfSizeT "\n", GraphArr.Length());

				GraphArr.SetFixedLength(true);
			}
		}

		PROF("PrePaint");

		int YScale = c.Y() / 2;
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
		pDC->HLine(c.x1, c.x2, cy);

		// LgiTrace("DrawRange: " LPrintfSizeT "->" LPrintfSizeT " (" LPrintfSizeT ") mode=%i\n", StartSample, EndSample, SampleRange, (int)EffectiveMode);
		if (EffectiveMode == DrawSamples)
		{
			sample_t pSample(start + (((StartSample * Channels) + ChannelIdx) * SampleBytes));
			bool DrawDots = SampleRange < (Client.X() >> 2);

			if (CursorX >= Client.x1 && CursorX <= Client.x2)
			{
				pDC->Colour(L_BLACK);
				pDC->VLine(CursorX, c.y1, c.y2);
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
					pDC->Rectangle(x-1, y-1, x+1, y+1);
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

				auto isCursor = CursorX == Vx;
				if (isCursor)
				{
					pDC->Colour(L_BLACK);
					pDC->VLine(Vx, c.y1, c.y2);
				}

				Grp<int32_t> Min, Max;
				StartSample = ViewToSample(Vx);
				EndSample = ViewToSample(Vx+1);				
				// printf("SampleIdx: " LPrintfSizeT "-" LPrintfSizeT "\n", StartSample, EndSample);
				
				if (EffectiveMode == ScanSamples)
				{
					// Scan individual samples
					sample_t pStart(start + (StartSample * Channels * SampleBytes) + ChannelIdx);
					auto pEnd = start + (EndSample * Channels * SampleBytes) + ChannelIdx;
					
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

		if (!Data.Valid())
		{
			auto r = DefaultPos();
			SetData(r);
		}

		auto Fnt = GetFont();
		LDisplayString ds(Fnt, ErrorMsg ? ErrorMsg : Msg);
		Fnt->Transparent(true);
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

