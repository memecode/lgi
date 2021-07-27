/// Audio waveform display control
#pragma once

#include "lgi/common/Layout.h"
#include "lgi/common/DisplayString.h"

#define CC(code) LgiSwap32(code)

class LAudioView : public LLayout
{
public:
	enum FileType
	{
		AudioUnknown,
		AudioRaw,
		AudioWav,
	};
	enum SampleType
	{
		AudioS16LE,
		AudioS32LE,
	};

protected:
	LArray<uint8_t> Audio;
	FileType Type = AudioUnknown;
	SampleType SampleType = AudioS16LE;
	int SampleBits = 0;
	int SampleRate = 0;
	size_t DataStart = 0;
	size_t CursorSample = 0;
	LRect DataRc;
	double XZoom = 1.0;
	LString Msg;

	struct Grp
	{
		int32_t Min = 0, Max = 0;
	};
	LArray<Grp> Graph;

	void MouseToCursor(LMouse &m)
	{
		auto idx = ViewToSample(m.x);
		if (idx != CursorSample)
		{
			CursorSample = idx;
			Invalidate();
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
		CursorSample = 0;
		DataRc.ZOff(-1, -1);

		return false;
	}

	bool Load(const char *FileName, int rate = 0, int bits = 0)
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
		SampleBits = bits;
		if (bits == 16)
		{
			SampleType = AudioS16LE;
		}
		else if (bits == 32)
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
					auto Channels = *p.u16++;
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
		}

		if (!SampleRate)
			SampleRate = 44100;

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
		return (Audio.Length() - DataStart) / (SampleBits >> 3);
	}

	int SampleToView(size_t idx)
	{
		return DataRc.x1 + (int)((idx * DataRc.X()) / GetSamples());
	}

	size_t ViewToSample(int x /*px*/)
	{
		int offset = x - DataRc.x1;
		ssize_t samples = GetSamples();
		ssize_t idx = offset * samples / DataRc.X();
		if (idx < 0)
			idx = 0;
		else if (idx >= samples)
			idx = samples - 1;
		return idx;
	}

	void OnMouseClick(LMouse &m)
	{
		if (m.IsContextMenu())
		{
		}
		else if (m.Left())
			MouseToCursor(m);
	}

	void OnMouseMove(LMouse &m)
	{
		if (m.Down())
			MouseToCursor(m);
	}

	bool OnMouseWheel(double Lines)
	{
		LMouse m;
		GetMouse(m);

		ssize_t samples = GetSamples();
		if (m.System() && m.Alt())
		{
			auto DefPos = DefaultPos();
			auto change = (double)Lines / 3;
			XZoom *= 1.0 + (-change / 10);

			int OldX = DataRc.X();
			int x = (int)(DefPos.X() * XZoom);
			int CursorX = SampleToView(CursorSample);
			DataRc.x1 = (int) (CursorX - (CursorSample * x / samples));
			DataRc.x2 = DataRc.x1 + x - 1;

			Invalidate();
		}
		else
		{
			int change = (int)(Lines * -6);
			DataRc.x1 += change;
			DataRc.x2 += change;

			Invalidate();
		}

		Msg.Printf("DataRc=%s XZoom=%g samples=" LPrintfSSizeT "\n", DataRc.GetStr(), XZoom, samples);

		return true;
	}

	template<typename T>
	void PaintSamples(LSurface *pDC)
	{
		T MaxT = 0;
		if (sizeof(T) == 1)
			MaxT = 0x7f;
		else if (sizeof(T) == 2)
			MaxT = 0x7fff;
		else
			MaxT = (T)(20 * 1000000);

		auto Client = GetClient();
		auto &c = DataRc;
		T *start = (T*)Audio.AddressOf(DataStart);
		size_t samples = GetSamples();
		auto *end = start + samples;
		int cy = c.y1 + (c.Y() / 2);

		int blk = 64;
		if (Graph.Length() == 0)
		{
			// Initialize the graph
			Graph.SetFixedLength(false);
			auto GenStart = LCurrentTime();
			for (auto p = start; p < end; p += blk)
			{
				int remain = MIN((int)(end - p), blk);
				auto &b = Graph.New();
				b.Min = b.Max = *p;
				for (int i=1; i<remain; i++)
				{
					if (p[i] < b.Min)
						b.Min = p[i];
					if (p[i] > b.Max)
						b.Max = p[i];
				}
			}
			auto GenTime = LCurrentTime() - GenStart;
			LgiTrace("GenTime=" LPrintfInt64 "\n", GenTime);
			Graph.SetFixedLength(true);
		}

		int YScale = c.Y() / 2;
		int CursorX = SampleToView(CursorSample);
		double blkScale = (double) Graph.Length() / c.X();
		bool DrawSamples = blkScale < 1.0;
				
		for (int x=0; x<c.X(); x++)
		{
			int ViewX = c.x1 + x;
			if (ViewX < Client.x1 || ViewX > Client.x2)
				continue;

			auto isCursor = CursorX == c.x1 + x;
			if (isCursor)
			{
				pDC->Colour(L_BLACK);
				pDC->VLine(c.x1 + x, c.y1, c.y2);
			}

			Grp Min, Max;
			if (DrawSamples)
			{
				// Scan individual samples
				auto Start = ViewToSample(ViewX);
				auto End = ViewToSample(ViewX+1);

				for (auto i = Start; i < End; i++)
				{
					Min.Min = MIN(Min.Min, start[i]);
					Max.Max = MAX(Max.Max, start[i]);														
				}						
			}
			else
			{
				// Scan the buckets
				double blkStart = (double)x * Graph.Length() / c.X();
				double blkEnd = (double)(x + 1) * Graph.Length() / c.X();
					
				for (int i=(int)floor(blkStart); i<(int)ceil(blkEnd); i++)
				{
					auto &b = Graph[i];
					Min.Min = MIN(Min.Min, b.Min);
					Min.Max = MAX(Min.Max, b.Min);
					Max.Min = MIN(Max.Min, b.Max);
					Max.Max = MAX(Max.Max, b.Max);
				}
			}
					
			int y1 = cy + (Min.Min * YScale / MaxT);
			int y2 = cy + (Max.Max * YScale / MaxT);
			pDC->Colour(isCursor ? cCursorMax : cMax);
			pDC->VLine(c.x1 + x, y1, y2);

			if (!DrawSamples)
			{
				y1 = cy + (Min.Max * YScale / MaxT);
				y2 = cy + (Max.Min * YScale / MaxT);
				pDC->Colour(isCursor ? cCursorMin : cMin);
				pDC->VLine(c.x1 + x, y1, y2);
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

		LColour cGrid(200, 200, 200);
		LColour cMax = LColour::Blue;
		LColour cMin = cMax.Mix(cGrid);

		if (!DataRc.Valid())
			DataRc = DefaultPos();
		auto &c = DataRc;
		pDC->Colour(cGrid);
		pDC->Box(&c);

		auto Fnt = GetFont();
		LDisplayString ds(Fnt, Msg);
		Fnt->Transparent(true);
		Fnt->Fore(cGrid);
		ds.Draw(pDC, 4, 4);

		switch (SampleType)
		{
			case AudioS16LE:
			{
				PaintSamples<int16_t>(pDC);
				break;
			}
			case AudioS32LE:
			{
				PaintSamples<int32_t>(pDC);
				break;
			}
		}
	}
};

