/// Audio waveform display control
#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Menu.h"
#include "lgi/common/ClipBoard.h"

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
	enum LCmds
	{
		IDC_COPY_CURSOR,
	};

protected:
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
	LString Msg;

	template<typename T>
	struct Grp
	{
		T Min = 0, Max = 0;
	};

	LArray<LArray<Grp<int16_t>>> Int16Grps;
	LArray<LArray<Grp<int32_t>>> Int32Grps;
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
			return false;
		
		if (!Audio.Length(f.GetSize()))
			return Empty();

		auto rd = f.Read(Audio.AddressOf(), f.GetSize());
		if (rd > 0 && rd != f.GetSize())
			Audio.Length(rd);

		auto Ext = LGetExtension(FileName);
		if (!Stricmp(Ext, "wav"))
			Type = AudioWav;
		else if (!Stricmp(Ext, "raw"))
			Type = AudioRaw;
		else
			return Empty();
			
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
				else p.u8 += SubChunkSz;
			}
		}
		else if (Type == AudioRaw)
		{
			// Assume 32bit
			SampleType = AudioS32LE;
			SampleBits = 32;
			// DataStart = 2;
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
		c.Size(20, 20);
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

	void OnMouseClick(LMouse &m)
	{
		if (m.IsContextMenu())
		{
			LSubMenu s;
			s.AppendItem("Copy Cursor Address", IDC_COPY_CURSOR);
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
				default:
					break;
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
		Msg.Printf("XZoom=%g Samples=" LPrintfSSizeT " Cursor=" LPrintfSizeT " @ " LPrintfSizeT " (%s) Graph=" LPrintfSizeT "\n",
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

	template<typename T>
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
			MaxT = (T)(20 * 1000000);

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
		bool DrawSamples = blkScale < 1.0;
				
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
			auto StartSample = ViewToSample(Vx);
			auto EndSample = ViewToSample(Vx+1);
			if (DrawSamples)
			{
				// Scan individual samples
				auto pStart = start + (StartSample * Channels) + ChannelIdx;
				auto pEnd = start + (EndSample * Channels) + ChannelIdx;
				for (auto i = pStart; i < pEnd; i += Channels)
				{
					auto n = Native(*i);
					Min.Min = MIN(Min.Min, n);
					Max.Max = MAX(Max.Max, n);
				}

				int asd=0;
			}
			else
			{
				// Scan the buckets
				auto &Graph = (*Grps)[ChannelIdx];
				double blkStart = (double)StartSample * Graph.Length() / samples;
				double blkEnd = (double)EndSample * Graph.Length() / samples;
				
				if (isCursor)
					LgiTrace("Blk %g-%g of " LPrintfSizeT "\n", blkStart, blkEnd, Graph.Length());

				for (int i=(int)floor(blkStart); i<(int)ceil(blkEnd); i++)
				{
					auto &b = Graph[i];
					Min.Min = MIN(Min.Min, b.Min);
					Min.Max = MAX(Min.Max, b.Min);
					Max.Min = MIN(Max.Min, b.Max);
					Max.Max = MAX(Max.Max, b.Max);
				}
			}
			
			auto y1 = cy + (Min.Min * YScale / MaxT);
			auto y2 = cy + (Max.Max * YScale / MaxT);
			pDC->Colour(isCursor ? cCursorMax : cMax);
			pDC->VLine(Vx, (int)y1, (int)y2);

			if (!DrawSamples)
			{
				y1 = cy + (Min.Max * YScale / MaxT);
				y2 = cy + (Max.Min * YScale / MaxT);
				pDC->Colour(isCursor ? cCursorMin : cMin);
				pDC->VLine(Vx, (int)y1, (int)y2);
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
		LDisplayString ds(Fnt, Msg);
		Fnt->Transparent(true);
		Fnt->Fore(L_LOW);
		ds.Draw(pDC, 4, 4);

		switch (SampleType)
		{
			case AudioS16LE:
			case AudioS16BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples<int16_t>(pDC, ch, &Int16Grps);
				break;
			}
			case AudioS32LE:
			case AudioS32BE:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples<int32_t>(pDC, ch, &Int32Grps);
				break;
			}
			case AudioFloat32:
			{
				for (int ch = 0; ch < Channels; ch++)
					PaintSamples<float>(pDC, ch, &FloatGrps);
				break;
			}
		}
	}
};

