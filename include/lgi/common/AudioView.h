/// Audio waveform display control
#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/FileSelect.h"

#define CC(code) LgiSwap32(code)

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
		AudioS16LE,
		AudioS16BE,
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

	LArray<uint8_t> Audio;
	LFileType Type = AudioUnknown;
	LSampleType SampleType = AudioS16LE;
	int SampleBits = 0;
	int SampleRate = 0;
	int Channels = 0;
	size_t DataStart = 0;
	size_t CursorSample = 0;
	LRect Data;
	LArray<LRect> ChData;
	double XZoom = 1.0;
	LString Msg, ErrorMsg;
	LDrawMode DrawMode = DrawAutoSelect;

	template<typename T>
	struct Grp
	{
		T Min = 0, Max = 0;
	};

	LArray<LArray<Grp<int16_t>>> Int16Grps;
	LArray<LArray<Grp<int32_t>>> Int32Grps;
	LArray<LArray<Grp<float>>> FloatGrps;

	LArray<LArray<void*>> PixelAddr;

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

	void SetData(LRect &r)
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
	}

	bool Empty()
	{
		Audio.Length(0);
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
			p.u8 = Audio.AddressOf();
			auto end = p.u8 + Audio.Length();

			if (*p.u32++ != CC('RIFF'))
				return Empty();
			auto ChunkSz = *p.u32++;
			auto Fmt = *p.u32++;
			
			while (p.u8 < end)
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
					else if (SampleBits == 32)
						SampleType = AudioS32LE;						
				}
				else if (SubChunkId == CC('data'))
				{
					DataStart = p.u8 - Audio.AddressOf();
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
		c.Inset(20, 20);
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
		int offset = x - Data.x1;
		ssize_t samples = GetSamples();
		ssize_t idx = offset * samples / Data.X();
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
					LFileSelect s;
					s.Parent(this);
					if (!s.Save())
						break;

					LFile out(s.Name(), O_WRITE);
					if (out)
					{
						out.SetSize(0);

						auto ptr = Audio.AddressOf(DataStart);
						out.Write(ptr, Audio.Length() - DataStart);
					}
					else
						LgiMsg(this, "Can't open '%s' for writing.", "Error", MB_OK, s.Name());
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
				Int16Grps.Length(0);
				Int32Grps.Length(0);
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
		p.u8 = Audio.AddressOf(DataStart + (Sample * Channels * SampleBytes));
		return p;
	}

	int16_t Native(int16_t &i)
	{
		if (SampleType == AudioS16BE)
			return LgiSwap16(i);
		return i;
	}

	int32_t Native(int32_t &i)
	{
		if (SampleType == AudioS32BE)
			return LgiSwap32(i);
		return i;
	}

	float Native(float &i)
	{
		return i;
	}

	void UpdateMsg()
	{
		ssize_t Samples = GetSamples();
		auto Addr = AddressOf(CursorSample);
		size_t AddrOffset = Addr.u8 - Audio.AddressOf();
		LString::Array Val;
		size_t GraphLen = 0;
		for (int ch=0; ch<Channels; ch++)
		{
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
		}

		Msg.Printf("Channels=%i SampleRate=%i BitDepth=%i XZoom=%g Samples=" LPrintfSSizeT " Cursor=" LPrintfSizeT " @ " LPrintfSizeT " (%s) Graph=" LPrintfSizeT "\n",
			Channels, SampleRate, SampleBits,
			XZoom, Samples, CursorSample, AddrOffset, LString(",").Join(Val).Get(), GraphLen);
	}

	bool OnMouseWheel(double Lines)
	{
		LMouse m;
		GetMouse(m);

		ssize_t Samples = GetSamples();
		if (m.System() && m.Alt())
		{
			auto DefPos = DefaultPos();
			auto change = (double)Lines / 3;
			XZoom *= 1.0 + (-change / 10);

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

	template<typename T, typename M /* Intermediate type for doing sample math, needs to be larger than 'T' */>
	void PaintSamples(LSurface *pDC, int ChannelIdx, LArray<LArray<Grp<T>>> *Grps)
	{
		#if PROFILE_PAINT_SAMPLES
		LProfile Prof("PaintSamples", 10);
		#endif

		T MaxT = 0;
		if (sizeof(T) == 1)
			MaxT = 0x7f;
		else if (sizeof(T) == 2)
			MaxT = 0x7fff;
		else
			MaxT = (T)0x7fffffff;

		auto Client = GetClient();
		auto &c = ChData[ChannelIdx];
		auto start = (T*)Audio.AddressOf(DataStart);
		size_t samples = GetSamples();
		auto end = start + (samples * Channels);
		int cy = c.y1 + (c.Y() / 2);

		pDC->Colour(cGrid);
		pDC->Box(&c);

		int blk = 64;
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

				int BlkChannels = blk * Channels;
				size_t BlkIndex = 0;
				for (auto p = start; p < end; p += BlkChannels)
				{
					int remain = MIN( (int)(end - p), BlkChannels );
					auto &b = GraphArr[BlkIndex++];
					b.Min = b.Max = Native(p[ch]);
					for (int i = Channels + ch; i < remain; i += Channels)
					{
						auto n = Native(p[i]);
						if (n < b.Min)
							b.Min = n;
						if (n > b.Max)
							b.Max = n;
					}
				}

				GraphArr.SetFixedLength(true);
			}
		}

		PROF("PrePaint");

		int YScale = c.Y() / 2;
		int CursorX = SampleToView(CursorSample);
		double blkScale = Grps->Length() > 0 ? (double) (*Grps)[0].Length() / c.X() : 1.0;
		LDrawMode EffectiveMode = DrawMode;
		auto StartSample = ViewToSample(Client.x1);
		auto EndSample = ViewToSample(Client.x2);
		auto SampleRange = EndSample - StartSample;

		if (EffectiveMode == DrawAutoSelect)
		{
			if (SampleRange < (Client.X() << 2))
				EffectiveMode = DrawSamples;
			else if (blkScale < 1.0)
				EffectiveMode = ScanSamples;
			else
				EffectiveMode = ScanGroups;
		}
	
		// LgiTrace("DrawRange: " LPrintfSizeT "->" LPrintfSizeT " (" LPrintfSizeT ") mode=%i\n", StartSample, EndSample, SampleRange, (int)EffectiveMode);
		if (EffectiveMode == DrawSamples)
		{
			auto pSample = start + (StartSample * Channels) + ChannelIdx;

			if (CursorX >= Client.x1 && CursorX <= Client.x2)
			{
				pDC->Colour(L_BLACK);
				pDC->VLine(CursorX, c.y1, c.y2);
			}
			pDC->Colour(cGrid);
			pDC->HLine(c.x1, c.x2, cy);

			// LgiTrace("	ptr=%i\n", (int)((char*)pSample-(char*)start));

			// For all samples in the view space...
			pDC->Colour(cMax);
			LPoint Prev(-1, -1);
			for (auto idx = StartSample; idx <= EndSample + 1; idx++, pSample += Channels)
			{
				auto n = Native(*pSample);
				auto x = SampleToView(idx);
				auto y = (T) (cy - ((M)n * YScale / MaxT));
				if (idx != StartSample)
					pDC->Line(Prev.x, Prev.y, (int)x, (int)y);
				Prev.Set((int)x, (int)y);
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

				Grp<T> Min, Max;
				StartSample = ViewToSample(Vx);
				EndSample = ViewToSample(Vx+1);
				if (EffectiveMode == ScanSamples)
				{
					// Scan individual samples
					auto pStart = start + (StartSample * Channels) + ChannelIdx;
					auto pEnd = start + (EndSample * Channels) + ChannelIdx;

					#if 0
					if (Vx==Client.x1)
					{
						auto PosX = SampleToView((pStart - ChannelIdx - start) / Channels);
						LgiTrace("	ptr=%i " LPrintfSizeT "-" LPrintfSizeT " x=%i pos=%i\n", (int)((char*)pStart-(char*)start), StartSample, EndSample, Vx, PosX);
					}
					#endif
					
					for (auto i = pStart; i < pEnd; i += Channels)
					{
						auto n = Native(*i);
						Min.Min = MIN(Min.Min, n);
						Max.Max = MAX(Max.Max, n);
					}
				}
				else
				{
					// Scan the buckets
					auto &Graph = (*Grps)[ChannelIdx];
					double blkStart = (double)StartSample * Graph.Length() / samples;
					double blkEnd = (double)EndSample * Graph.Length() / samples;
				
					#if 0
					if (isCursor)
						LgiTrace("Blk %g-%g of " LPrintfSizeT "\n", blkStart, blkEnd, Graph.Length());
					#endif

					for (int i=(int)floor(blkStart); i<(int)ceil(blkEnd); i++)
					{
						auto &b = Graph[i];
						Min.Min = MIN(Min.Min, b.Min);
						Min.Max = MAX(Min.Max, b.Min);
						Max.Min = MIN(Max.Min, b.Max);
						Max.Max = MAX(Max.Max, b.Max);
					}
				}
			
				auto y1 = (T) (cy - ((M)Min.Min * YScale / MaxT));
				auto y2 = (T) (cy - ((M)Max.Max * YScale / MaxT));
				pDC->Colour(isCursor ? cCursorMax : cMax);
				pDC->VLine(Vx, (int)y1, (int)y2);

				if (EffectiveMode == ScanGroups)
				{
					y1 = (T) (cy - ((M)Min.Max * YScale / MaxT));
					y2 = (T) (cy - ((M)Max.Min * YScale / MaxT));
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
			SetData(DefaultPos());

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
					PaintSamples<int16_t,int32_t>(pDC, ch, &Int16Grps);
				break;
			}
			case AudioS32LE:
			case AudioS32BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples<int32_t,int64_t>(pDC, ch, &Int32Grps);
				break;
			}
			case AudioFloat32:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples<float,double>(pDC, ch, &FloatGrps);
				break;
			}
		}
	}
};

