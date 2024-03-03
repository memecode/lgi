// For reading timezone info files:
// https://www.rfc-editor.org/rfc/rfc8536.html

#pragma once

#ifdef HAIKU
#include <netinet/in.h>
#endif
#include "lgi/common/DateTime.h"

class LTimeZoneInfo
{
	static int TimeToSeconds(LString t)
	{
		auto parts = t.SplitDelimit(":");
		auto hrs = parts[0].Int();
		auto min = parts.IdxCheck(1) ? parts[1].Int() : 0;
		auto sec = parts.IdxCheck(2) ? parts[2].Int() : 0;
		return (hrs * 3600) + (min * 60) + sec;
	}

	struct Header
	{
		char magic[4];
		char version;
		char reserved[15];
		uint32_t UtCount;
		uint32_t StdCount;
		uint32_t LeapCount;
		uint32_t TimeCount;
		uint32_t TypeCount;
		uint32_t CharCount;
	};

	struct LocalTimeType
	{
		int32_t UtOffset;
		uint8_t IsDst;
		uint8_t Idx;
	};

	struct LeapSecond1
	{
		uint32_t occur;
		uint32_t corr;
	};

	struct LeapSecond2
	{
		uint64_t occur;
		uint32_t corr;
	};

	struct PosixTransition
	{
		int JulianIdx = -1; // 0 - 365
		int Month = 0;      // 1 - 12
		int Week = 0;       // 1 - 5
		int Day = 0;        // 0 - 6
		LString time;

		int OffsetSeconds(int StdOffsetSeconds)
		{
			int sec = StdOffsetSeconds;
			if (time)
			{
				auto TwoAm = 2 * 3600;
				auto TimeOff = TimeToSeconds(time);
				auto Offset = TimeOff - TwoAm;
				sec += Offset;
			}
			return sec;
		}

		LDateTime Get(int year)
		{
			LDateTime dt;
			if (JulianIdx >= 0)
			{
				dt.SetTimeZone(0, false);
				dt.Year(year);
				dt.Month(1);
				dt.Day(1);
				dt.AddDays(JulianIdx);
			}
			else if (Month)
			{
				dt.SetTimeZone(0, false);
				dt.Year(year);
				dt.Month(Month);
				dt.Day(1);

				if (Week == 5)
				{
					// Find last instance of 'Day'
					dt.Day(dt.DaysInMonth());
					while (dt.Day() > 1 && dt.DayOfWeek() != Day)
						dt.AddDays(-1);
				}
				else
				{
					// Find 'Week'th instance of 'Day'
					int CurWk = Week;
					int MonthsDays = dt.DaysInMonth();
					while (dt.Day() <= MonthsDays)
					{
						int CurDay = dt.DayOfWeek();
						if (CurDay == Day)
						{
							if (--CurWk == 0)
								break;
						}
						dt.AddDays(1);
					}
				}
			}
			else LAssert(0);

			if (time)
				dt.Hours(time.Int());
			else
				dt.Hours(2);

			return dt;
		}

		bool Set(LString spec)
		{
			auto parts = spec.SplitDelimit("/");
			auto s = parts[0];
			if (parts.Length() > 1)
				time = parts[1];

			if (s(0) == 'J')
			{
				JulianIdx = s(1, -1).Int() - 1;
				return JulianIdx >= 0 && JulianIdx < 365;
			}
			else if (s(0) == 'M')
			{
				auto parts = s(1, -1).SplitDelimit(".");
				if (parts.Length() != 3)
					return false;
				
				Month = parts[0].Int();
				Week  = parts[1].Int();
				Day   = parts[2].Int();

				return Month >= 1 && Month <= 12;
			}
			else
			{
				JulianIdx = s.Int();
				return JulianIdx >= 0 && JulianIdx < 365;
			}
		}
	};

	enum TVersion
	{
		Version1 = 0,
		Version2 = '2',
		Version3 = '3',
	};

	LString data;
	LPointer ptr;
	char *end = NULL;
	bool Eof() { return ptr.c >= end; }
	ssize_t Remaining() { return end - ptr.c; }

	Header hdr;
	LArray<uint32_t> TransitionTimes1;
	LArray<int64_t> TransitionTimes2;
	LArray<uint8_t>  TransitionTypes;
	LArray<LocalTimeType> LocalTimeTypes;
	LString TimeZoneDesignations;
	LArray<LeapSecond1> LeapSeconds1;
	LArray<LeapSecond2> LeapSeconds2;
	LArray<uint8_t> StandardWall;
	LArray<uint8_t> UtLocal;

	// Format:
	//		https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03
	// Examples:
	//		https://support.cyberdata.net/portal/en/kb/articles/010d63c0cfce3676151e1f2d5442e311
	LString footer;

	bool Read(Header &h)
	{
		h.magic[0] = *ptr.c++;
		h.magic[1] = *ptr.c++;
		h.magic[2] = *ptr.c++;
		h.magic[3] = *ptr.c++;
		if (Strncmp(h.magic, "TZif", 4))
			return OnError("bad magic value");

		h.version  = *ptr.u8++;
		ptr.c += 15;
		h.UtCount = ntohl(*ptr.u32++);
		h.StdCount = ntohl(*ptr.u32++);
		h.LeapCount = ntohl(*ptr.u32++);
		h.TimeCount = ntohl(*ptr.u32++);
		h.TypeCount = ntohl(*ptr.u32++);
		h.CharCount = ntohl(*ptr.u32++);
		
		return !Eof();
	}

	bool Read(LocalTimeType &t)
	{
		if (Remaining() < 6)
			return false;
		t.UtOffset = ntohl(*ptr.s32++);
		t.IsDst = *ptr.u8++;
		t.Idx = *ptr.u8++;
		return true;
	}

	bool Read(LeapSecond1 &ls)
	{
		if (Remaining() < 8)
			return false;
		ls.occur = ntohl(*ptr.u32++);
		ls.corr = ntohl(*ptr.u32++);
		return true;
	}

	bool Read(LeapSecond2 &ls)
	{
		if (Remaining() < 12)
			return false;
		ls.occur = ntohll(*ptr.u64++);
		ls.corr = ntohl(*ptr.u32++);
		return true;
	}

	template<typename T>
	bool Read(LArray<T> &out, size_t elem)
	{
		if (elem == 0)
			return true;
		size_t bytes = sizeof(T) * elem;
		if (!out.Length(elem))
			return OnError("alloc failed");
		if (Remaining() < bytes)
			return OnError("not enough data left");
		memcpy(out.AddressOf(), ptr.vp, bytes);
		ptr.c += bytes;
		return true;
	}

	bool Read(LString &s)
	{
		char *start = ptr.c;
		while (!Eof() && *ptr.c)
			ptr.c++;
		if (ptr.c > start)
			if (!s.Set(start, ptr.c - start))
				return OnError("alloc failed");
		ptr.c++;
		return true;
	}

	bool Read(TVersion ver)
	{
		if (ver == Version1)
		{
			if (!Read(TransitionTimes1, hdr.TimeCount))
				return OnError("failed to read transitions");
		}
		else if (ver == Version2)
		{
			if (!Read(TransitionTimes2, hdr.TimeCount))
				return OnError("failed to read transitions");
			for (auto &i: TransitionTimes2)
				i = ntohll(i);
		}
		else return OnError("unsupported version");

		if (!Read(TransitionTypes, hdr.TimeCount))
			return OnError("failed to read transition types");

		if (!LocalTimeTypes.Length(hdr.TypeCount))
			return OnError("alloc failed");
		for (unsigned i=0; i<hdr.TypeCount; i++)
			if (!Read(LocalTimeTypes[i]))
				return false;

		if (!TimeZoneDesignations.Length(hdr.CharCount))
			return OnError("alloc error");
		memcpy(TimeZoneDesignations.Get(), ptr.c, hdr.CharCount);
		ptr.c += hdr.CharCount;		
		
		if (ver == Version1)
		{
			if (!LeapSeconds1.Length(hdr.LeapCount))
				return OnError("alloc failed");
			for (unsigned i=0; i<hdr.LeapCount; i++)
				Read(LeapSeconds1[i]);
		}
		else if (ver == Version2)
		{
			if (!LeapSeconds2.Length(hdr.LeapCount))
				return OnError("alloc failed");
			for (unsigned i=0; i<hdr.LeapCount; i++)
				Read(LeapSeconds2[i]);
		}
		
		Read(StandardWall, hdr.StdCount);
		
		Read(UtLocal, hdr.UtCount);

		return true;
	}

	bool GetTransition(size_t idx, LDateTime &dt, LString &tz, int &offsetSeconds, bool &isDst)
	{
		if (!TransitionTimes2.IdxCheck(idx))
			return false;

		auto time = TransitionTimes2[idx];
		auto typeIdx = TransitionTypes[idx];
		if (!LocalTimeTypes.IdxCheck(typeIdx))
		{
			LAssert(!"invalid index");
			return false;
		}

		auto &type = LocalTimeTypes[typeIdx];
		isDst = type.IsDst;
		tz = type.Idx < TimeZoneDesignations.Length() ? TimeZoneDesignations.Get() + type.Idx : NULL;

		dt.SetTimeZone(0, false);
		dt.SetUnix(time);
		offsetSeconds = type.UtOffset;

		return true;
	}

public:
	using LDstInfo = LDateTime::LDstInfo;
	constexpr static const char *DefaultFile = "/etc/localtime";

	LTimeZoneInfo(const char *fileName = NULL)
	{
		if (fileName)
			Read(fileName);
	}

	virtual ~LTimeZoneInfo()
	{
	}

	// Override this to handle errors in a custom way
	virtual bool OnError(LString msg)
	{
		LgiTrace("LTimeZoneInfo error: %s\n", msg.Get());
		return false;
	}

	bool Read(const char *fileName = DefaultFile)
	{
		// Read the file into memeory...
		LFile f(fileName, O_READ);
		if (!f)
			return OnError(LString::Fmt("failed to open '%s' for reading", fileName));
		if (!data.Length(f.GetSize()))
			return OnError("alloc failed");
		if (f.Read(data.Get(), data.Length()) != data.Length())
			return OnError("failed to read all the data");
		ptr.c = data.Get();
		end = data.Get() + data.Length();

		// Read version 1 header:
		if (!Read(hdr))
			return OnError("hdr read failed");

		// Version 1 data blocks:
		if (!Read(Version1))
			return false;

		if (hdr.version == Version2)
		{
			if (!Read(hdr))
				return OnError("hdr ver2 read failed");
			if (!Read(Version2))
				return false;

			// footer
			if (*ptr.c++ != '\n')
				return OnError("expecting start nl");
			auto endFooter = strnchr(ptr.c, '\n', Remaining());
			if (!endFooter)
				return OnError("expecting end nl");
			footer.Set(ptr.c, endFooter - ptr.c);
		}

		return true;
	}
	
	bool GetDaylightSavingsInfo(LArray<LDstInfo> &Info, LDateTime &Start, LDateTime *End)
	{
		auto InRange = [&](LDateTime &dt)
		{
			if (End)
				return dt >= Start && dt < *End;
			
			return	dt.Year() == (Start.Year()-1) ||
					dt.Year() == Start.Year();
		};

		for (unsigned i=0; i<TransitionTimes2.Length(); i++)
		{
			LDateTime dt;
			LString tz;
			int offsetSeconds;
			bool isDst;
			if (GetTransition(i, dt, tz, offsetSeconds, isDst))
			{
				bool inrange = InRange(dt);
				#if 0
				LgiTrace("%s: UtOffset=%g, IsDst=%i, Name=%s, inrange=%i\n",
					dt.Get().Get(),
					(double)offsetSeconds/3600,
					isDst,
					tz.Get(),
					inrange);
				#endif

				if (inrange)
				{
					auto &inf = Info.New();
					inf.Utc = dt.Ts();
					inf.Offset = offsetSeconds / 60;
				}
			}
		}

		if (footer)
		{
			// Get the last custom transition and make the start after that...
			if (TransitionTimes2.Length())
			{
				LDateTime lastDt;
				LString tz;
				int offsetSeconds;
				bool isDst;
				if (GetTransition(TransitionTimes2.Length()-1, lastDt, tz, offsetSeconds, isDst))
				{
					if (Start < lastDt)
					{
						Start = lastDt;
						Start.AddDays(1);
					}
				}
				else return false;
			}

			// stdoffset[dst[offset][,start[/time],end[/time]]]
			auto parts = footer.SplitDelimit(",");
			const char *s = parts[0];

			auto ReadAlpha = [&]()
			{
				auto start = s;
				while (*s && IsAlpha(*s)) s++;
				return LString(start, s - start);
			};
			auto ReadDigit = [&]()
			{
				auto start = s;
				while (*s && (*s == '-' || *s == ':' || IsDigit(*s))) s++;
				return LString(start, s - start);
			};

			auto StdName = ReadAlpha();
			auto StdOffsetSeconds = -TimeToSeconds(ReadDigit());
			auto DstName = ReadAlpha();
			PosixTransition startTrans, endTrans;
			if (parts.IdxCheck(1))
				startTrans.Set(parts[1]);
			if (parts.IdxCheck(2))
				endTrans.Set(parts[2]);

			#if 0
			printf("start=%s end=%s\n", Start.Get().Get(), End ? End->Get().Get() : NULL);
			#endif

			for (int year = Start.Year()-1; year <= (End ? End->Year() : Start.Year()); year++)
			{
				// Figure out the start and end DST for 'year'
				int DstOffset = endTrans.OffsetSeconds(StdOffsetSeconds) / 60;

				auto startDt = startTrans.Get(year);
				startDt.SetTimeZone(StdOffsetSeconds / 60, false);
				if (InRange(startDt))
				{
					auto &s = Info.New();
					s.Utc = startDt.Ts();
					s.Offset = DstOffset;
				}

				auto endDt = endTrans.Get(year);
				endDt.SetTimeZone(StdOffsetSeconds / 60, false);
				#if 0
				printf("\tstart=%s %i, end=%s %i\n",
					startDt.Get().Get(), InRange(startDt),
					endDt.Get().Get(), InRange(endDt));
				#endif
				if (InRange(endDt))
				{
					auto &e = Info.New();
					e.Utc = endDt.Ts();
					e.Offset = StdOffsetSeconds / 60;
				}
			}
		}
		
		return Info.Length() > 0;
	}
};
