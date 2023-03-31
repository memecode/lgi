/*
**	FILE:			LDateTime.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			11/11/98
**	DESCRIPTION:	Scribe Date Time Object
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#define _DEFAULT_SOURCE
#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/timeb.h>
#if defined(MAC)
#include <sys/time.h>
#endif
#ifdef WINDOWS
#include <tchar.h>
#endif

#include "lgi/common/Lgi.h"
#include "lgi/common/DateTime.h"
#include "lgi/common/DocView.h"

constexpr const char *LDateTime::WeekdaysShort[7];
constexpr const char *LDateTime::WeekdaysLong[7];
constexpr const char *LDateTime::MonthsShort[12];
constexpr const char *LDateTime::MonthsLong[12];

#if !defined(WINDOWS)
#define MIN_YEAR		1800
#endif

//////////////////////////////////////////////////////////////////////////////
uint16 LDateTime::DefaultFormat = GDTF_DEFAULT;
char LDateTime::DefaultSeparator = '/';

uint16 LDateTime::GetDefaultFormat()
{
	if (DefaultFormat == GDTF_DEFAULT)
	{
		#ifdef WIN32

		TCHAR s[80] = _T("1");
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDATE, s, CountOf(s));
		switch (_tstoi(s))
		{
			case 0:
				DefaultFormat = GDTF_MONTH_DAY_YEAR;
				break;
			default:
			case 1:
				DefaultFormat = GDTF_DAY_MONTH_YEAR;
				break;
			case 2:
				DefaultFormat = GDTF_YEAR_MONTH_DAY;
				break;
		}

		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ITIME, s, sizeof(s));
		if (_tstoi(s) == 1)
		{
			DefaultFormat |= GDTF_24HOUR;
		}
		else
		{
			DefaultFormat |= GDTF_12HOUR;
		}

		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDATE, s, sizeof(s)))
			DefaultSeparator = (char)s[0];

		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, s, sizeof(s)))
		{
			char Sep[] = { DefaultSeparator, '/', '\\', '-', '.', 0 };
			LString Str = s;
			auto t = Str.SplitDelimit(Sep);
			for (int i=0; i<t.Length(); i++)
			{
				if (t[i].Equals("mm"))
					DefaultFormat |= GDTF_MONTH_LEADINGZ;
				else if (t[i].Equals("dd"))
					DefaultFormat |= GDTF_DAY_LEADINGZ;
			}
		}

		#else

		// get some user setting from the system??
		DefaultFormat = GDTF_DAY_MONTH_YEAR | GDTF_12HOUR;

		#endif
	}

	return DefaultFormat;
}

//////////////////////////////////////////////////////////////////////////////
#define NO_ZONE				-1000
static int CurTz			= NO_ZONE;
static int CurTzOff			= NO_ZONE;

LDateTime::LDateTime(const char *Init)
{
	Empty();
	if (Init)
		Set(Init);
}

LDateTime::LDateTime(uint64 Ts)
{
	Empty();
	if (Ts)
		Set(Ts);
}

LDateTime::~LDateTime()
{
}

void LDateTime::Empty()
{
	_Year = 0;
	_Month = 0;
	_Day = 0;
	_Minutes = 0;
	_Hours = 0;
	_Seconds = 0;
	_Thousands = 0;

	if (CurTz == NO_ZONE)
		_Tz = SystemTimeZone();
	else
		_Tz = CurTz + CurTzOff;

	_Format = GetDefaultFormat();
}

#define InRange(v, low, high) ((v) >= low && (v) <= high)
bool LDateTime::IsValid() const
{
	return	InRange(_Day, 1, 31) &&
			InRange(_Year, 1600, 2100) &&
			InRange(_Thousands, 0, 999) &&
			InRange(_Month, 1, 12) &&
			InRange(_Seconds, 0, 59) &&
			InRange(_Minutes, 0, 59) &&
			InRange(_Hours, 0, 23) &&
			InRange(_Tz, -780, 780);
}

void LDateTime::SetTimeZone(int NewTz, bool ConvertTime)
{
	if (ConvertTime && NewTz != _Tz)
	{
		// printf("SetTimeZone: %i\n", NewTz - _Tz);
		AddMinutes(NewTz - _Tz);
	}

	_Tz = NewTz;
}

int LDateTime::SystemTimeZone(bool ForceUpdate)
{
	if (ForceUpdate || CurTz == NO_ZONE)
	{
		CurTz = 0;
		CurTzOff = 0;

		#ifdef MAC

			#ifdef LGI_COCOA

				NSTimeZone *timeZone = [NSTimeZone localTimeZone];
				if (timeZone)
				{
					NSDate *Now = [NSDate date];
					CurTz = (int) [timeZone secondsFromGMTForDate:Now] / 60;
					CurTzOff = [timeZone daylightSavingTimeOffsetForDate:Now] / 60;
					CurTz -= CurTzOff;
				}

			#elif defined LGI_CARBON
			
				CFTimeZoneRef tz = CFTimeZoneCopySystem();
				CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
				Boolean dst = CFTimeZoneIsDaylightSavingTime(tz, now);
				if (dst)
				{
					CFAbsoluteTime next = CFTimeZoneGetNextDaylightSavingTimeTransition(tz, now);
					CurTz = CFTimeZoneGetSecondsFromGMT(tz, next + 100) / 60;
				}
				else
				{
					CurTz = CFTimeZoneGetSecondsFromGMT(tz, now) / 60;
				}

				CurTzOff = CFTimeZoneGetDaylightSavingTimeOffset(tz, now) / 60;
				CFRelease(tz);
			
			#endif
		
		#elif defined(WIN32)
		
			timeb tbTime;
			ftime(&tbTime);
			CurTz = -tbTime.timezone;

			TIME_ZONE_INFORMATION Tzi;
			if (GetTimeZoneInformation(&Tzi) == TIME_ZONE_ID_DAYLIGHT)
				CurTzOff = -Tzi.DaylightBias;
		
		#elif defined(LINUX) || defined(HAIKU)

			int six_months = (365 * 24 * 60 * 60) / 2;
			time_t now = 0, then = 0;
			time (&now);
			then = now - six_months;
			
			tm now_tz, then_tz;
			tm *t = localtime_r(&now, &now_tz);
			if (t)
			{
				localtime_r(&then, &then_tz);
				
				CurTz = now_tz.tm_gmtoff / 60;
				if (now_tz.tm_isdst)
				{
					CurTzOff = (now_tz.tm_gmtoff - then_tz.tm_gmtoff) / 60;
					CurTz = then_tz.tm_gmtoff / 60;
				}
				else
					// This is not DST so there is no offset right?
					CurTzOff = 0; // (then_tz.tm_gmtoff - now_tz.tm_gmtoff) / 60;
			}
			else return NO_ZONE;
		
		#else
		
			#error "Impl me."
		
		#endif
	}
	
	return CurTz + CurTzOff;
}

int LDateTime::SystemTimeZoneOffset()
{
	if (CurTz == NO_ZONE)
		SystemTimeZone();

	return CurTzOff;
}

#if defined WIN32
LDateTime ConvertSysTime(SYSTEMTIME &st, int year)
{
	LDateTime n;
	if (st.wYear)
	{
		n.Year(st.wYear);
		n.Month(st.wMonth);
		n.Day(st.wDay);
	}
	else
	{
		n.Year(year);
		n.Month(st.wMonth);
		
		// Find the 'nth' matching weekday, starting from the first day in the month
		n.Day(1);
		LDateTime c = n;
		for (int i=0; i<st.wDay && c.Month() == n.Month(); i++)
		{
			while (c.DayOfWeek() != st.wDayOfWeek)
				c.AddDays(1);
			if (c.Month() != n.Month())
				break;
			n = c;
		}
	}

	n.Hours(st.wHour);
	n.Minutes(st.wMinute);
	n.Seconds(st.wSecond);
	n.Thousands(st.wMilliseconds);
	return n;
}

static int LDateCmp(LDateTime *a, LDateTime *b)
{
	return a->Compare(b);
}

#elif defined(LINUX)

static bool ParseValue(char *s, LAutoString &var, LAutoString &val)
{
	if (!s)
		return false;
	char *e = strchr(s, '=');
	if (!e)
		return false;
	
	*e++ = 0;
	var.Reset(NewStr(s));
	val.Reset(NewStr(e));
	*e = '=';
	return var != 0 && val != 0;
}

#endif

/* Testing code...
	LDateTime Start, End;
	LArray<LDateTime::GDstInfo> Info;

	Start.Set("1/1/2010");
	End.Set("31/12/2014");
	LDateTime::GetDaylightSavingsInfo(Info, Start, &End);

	LStringPipe p;
	for (int i=0; i<Info.Length(); i++)
	{
		LDateTime dt;
		dt = Info[i].UtcTimeStamp;
		char s[64];
		dt.Get(s);
		p.Print("%s, %i\n", s, Info[i].Offset);
	}
	LAutoString s(p.NewStr());
	LgiMsg(0, s, "Test");
*/

LDateTime LDateTime::GDstInfo::GetLocal()
{
	LDateTime d;
	d = UtcTimeStamp;
	d.SetTimeZone(0, false);
	d.SetTimeZone(Offset, true);
	return d;
}

struct MonthHash : public LHashTbl<ConstStrKey<char,false>,int>
{
	MonthHash()
	{
		for (int i=0; i<CountOf(LDateTime::MonthsShort); i++)
			Add(LDateTime::MonthsShort[i], i + 1);

		for (int i=0; i<CountOf(LDateTime::MonthsLong); i++)
			Add(LDateTime::MonthsLong[i], i + 1);
	}
};

LString::Array Zdump;

#define DEBUG_DST_INFO		0

bool LDateTime::GetDaylightSavingsInfo(LArray<GDstInfo> &Info, LDateTime &Start, LDateTime *End)
{
	bool Status = false;
	
	#if defined(WIN32)

		TIME_ZONE_INFORMATION Tzi;
		auto r = GetTimeZoneInformation(&Tzi);
		if (r > TIME_ZONE_ID_UNKNOWN)
		{
			Info.Length(0);

			// Find the dates for the previous year from Start. This allows
			// us to cover the start of the current year.
			LDateTime s = ConvertSysTime(Tzi.StandardDate, Start.Year() - 1);
			LDateTime d = ConvertSysTime(Tzi.DaylightDate, Start.Year() - 1);

			// Create initial Info entry, as the last change in the previous year
			auto *i = &Info.New();
			if (s < d)
			{
				// Year is: Daylight->Standard->Daylight
				LDateTime tmp = d;
				i->Offset = -(Tzi.Bias + Tzi.DaylightBias);
				tmp.AddMinutes(-i->Offset);
				i->UtcTimeStamp = tmp.Ts();
			}
			else
			{
				// Year is: Standard->Daylight->Standard
				LDateTime tmp = s;
				i->Offset = -(Tzi.Bias + Tzi.StandardBias);
				tmp.AddMinutes(-i->Offset);
				i->UtcTimeStamp = tmp.Ts();;
			}
				
			for (auto y=Start.Year(); y<=(End?End->Year():Start.Year()); y++)
			{
				if (s < d)
				{
					// Cur year, first event: end of DST
					i = &Info.New();
					auto tmp = ConvertSysTime(Tzi.StandardDate, y);
					i->Offset = -(Tzi.Bias + Tzi.StandardBias);
					tmp.AddMinutes(-i->Offset);
					i->UtcTimeStamp = tmp.Ts();

					// Cur year, second event: start of DST
					i = &Info.New();
					tmp = ConvertSysTime(Tzi.DaylightDate, y);
					i->Offset = -(Tzi.Bias + Tzi.DaylightBias);
					tmp.AddMinutes(-i->Offset);
					i->UtcTimeStamp = tmp.Ts();
				}
				else
				{
					// Cur year, first event: start of DST
					i = &Info.New();
					auto tmp = ConvertSysTime(Tzi.DaylightDate, Start.Year());
					i->Offset = -(Tzi.Bias + Tzi.DaylightBias);
					tmp.AddMinutes(-i->Offset);
					i->UtcTimeStamp = tmp.Ts();

					// Cur year, second event: end of DST
					i = &Info.New();
					tmp = ConvertSysTime(Tzi.StandardDate, Start.Year());
					i->Offset = -(Tzi.Bias + Tzi.StandardBias);
					tmp.AddMinutes(-i->Offset);
					i->UtcTimeStamp = tmp.Ts();
				}
			}

			Status = true;
		}
	
	#elif defined(MAC)
	
		LDateTime Before = Start;
		Before.AddMonths(-6);

		NSTimeZone *tz = [NSTimeZone systemTimeZone];
		NSDate *startDate = [[NSDate alloc] initWithTimeIntervalSince1970:(Before.Ts() / Second64Bit) - Offset1800];
		for (int n=0; n<6; n++)
		{
			NSDate *next = [tz nextDaylightSavingTimeTransitionAfterDate:startDate];
			auto &i = Info.New();
			
			i.UtcTimeStamp = ([next timeIntervalSince1970] + Offset1800) * Second64Bit;
			i.Offset = (int)([tz secondsFromGMTForDate:[next dateByAddingTimeInterval:60]]/60);
			
			#if DEBUG_DST_INFO
			{
				LDateTime dt;
				dt.Set(i.UtcTimeStamp);
				LgiTrace("%s:%i - Ts=%s Off=%i\n", _FL, dt.Get().Get(), i.Offset);
			}
			#endif
			
			[startDate release];
			startDate = next;
		}
	
	#elif defined(LINUX)

		if (!Zdump.Length())
		{	
			FILE *f = popen("zdump -v /etc/localtime", "r");
			if (f)
			{
				char s[256];
				size_t r;
				LStringPipe p(1024);
				while ((r = fread(s, 1, sizeof(s), f)) > 0)
				{
					p.Write(s, (int)r);
				}		
				fclose(f);
				
				LString ps = p.NewLStr();
				Zdump = ps.Split("\n");
			}
		}
			
		MonthHash Lut;
		LDateTime Prev;
		int PrevOff = 0;
		for (auto Line: Zdump)
		{
			auto l = Line.SplitDelimit(" \t");
			if (l.Length() >= 16 &&
				l[0].Equals("/etc/localtime"))
			{
				// /etc/localtime  Sat Oct  3 15:59:59 2037 UTC = Sun Oct  4 01:59:59 2037 EST isdst=0 gmtoff=36000
				// 0               1   2    3 4        5    6   7 8   9    10 11      12   13  14      15
				#if DEBUG_DST_INFO
				printf("DST: %s\n", Line);
				#endif				
				
				LDateTime Utc;
				Utc.Year(l[5].Int());
				auto Tm = l[4].SplitDelimit(":");
				if (Tm.Length() != 3)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Tm '%s' has wrong parts: %s\n", _FL, l[4], Line);
					#endif
					continue;
				}

				Utc.Hours(Tm[0].Int());
				Utc.Minutes(Tm[1].Int());
				Utc.Seconds(Tm[2].Int());
				if (Utc.Minutes() < 0)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Mins is zero: %s\n", _FL, l[4]);
					#endif
					continue;
				}

				int m = Lut.Find(l[2]);
				if (!m)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown month '%s'\n", _FL, l[2]);
					#endif
					continue;
				}

				Utc.Day(l[3].Int());
				Utc.Month(m);

				LAutoString Var, Val;
				if (!ParseValue(l[14], Var, Val) ||
					stricmp(Var, "isdst"))
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown value for isdst\n", _FL);
					#endif
					continue;
				}
				
				if (!ParseValue(l[15], Var, Val) ||
					stricmp(Var, "gmtoff"))
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown value for isdst\n", _FL);
					#endif
					continue;
				}
				
				int Off = atoi(Val) / 60;

				if (Utc.Ts() == 0)
					continue;

				if (Prev.Year() &&
					Prev < Start &&
					Start < Utc)
				{
					// Emit initial entry for 'start'
					auto &inf = Info.New();
					inf.UtcTimeStamp = Prev;
					inf.Offset = PrevOff;
					#if DEBUG_DST_INFO
					printf("Info: Start=%s %i\n", Prev.Get().Get(), inf.Offset);
					#endif
				}
				
				if (Utc > Start)
				{
					// Emit furthur entries for DST events between start and end.
					auto &inf = Info.New();
					inf.UtcTimeStamp = Utc;
					inf.Offset = Off;
					#if DEBUG_DST_INFO
					printf("Info: Next=%s %i\n", Utc.Get().Get(), inf.Offset);
					#endif
					
					if (End && Utc > *End)
					{
						// printf("Utc after end: %s > %s\n", Utc.Get().Get(), End->Get().Get());
						break;
					}
				}

				Prev = Utc;
				PrevOff = Off;
			}
		}
		Status = Info.Length() > 1;
	
	#else

		LAssert(!"Not implemented.");

	#endif

	return Status;
}

bool LDateTime::DstToLocal(LArray<GDstInfo> &Dst, LDateTime &dt)
{
	if (dt.GetTimeZone())
	{
		LAssert(!"Should be a UTC date.");
		return true;
	}

	// LgiTrace("DstToLocal: %s\n", dt.Get().Get());
	LAssert(Dst.Length() > 1); // Needs to have at least 2 entries...?
	for (size_t i=0; i<Dst.Length()-1; i++)
	{
		auto &a = Dst[i];
		auto &b = Dst[i+1];
		LDateTime start, end;
		start.Set(a.UtcTimeStamp);
		end.Set(b.UtcTimeStamp);

		// LgiTrace("Rng[%i]: %s -> %s\n", (int)i, start.Get().Get(), end.Get().Get());
		if (dt >= start && dt < end)
		{
			dt.SetTimeZone(a.Offset, true);
			return true;
		}
	}

	auto Last = Dst.Last();
	LDateTime d;
	d.Set(Last.UtcTimeStamp);
	if (dt >= d && dt.Year() == d.Year())
	{
		// If it's after the last DST change but in the same year... it's ok...
		// Just use the last offset.
		dt.SetTimeZone(Last.Offset, true);
		return true;
	}

	LAssert(!"No valid DST range for this date.");
	return false;
}

int LDateTime::DayOfWeek() const
{
	int Index = 0;
	int Day = IsLeapYear() ? 29 : 28;

	switch (_Year / 100)
	{
		case 19:
		{
			Index = 3;
			break;
		}
		case 20:
		{
			Index = 2;
			break;
		}
	}

	// get year right
	int y = _Year % 100;
	int r = y % 12;
	Index = (Index + (y / 12) + r + (r / 4)) % 7;
	
	// get month right
	if (_Month % 2 == 0)
	{
		// even month
		if (_Month > 2) Day = _Month;
	}
	else
	{
		// odd month
		switch (_Month)
		{
			case 1:
			{
				Day = 31;
				if (IsLeapYear())
				{
					Index = Index > 0 ? Index - 1 : Index + 6;
				}
				break;
			}
			case 11:
			case 3:
			{
				Day = 7;
				break;
			}
			case 5:
			{
				Day = 9;
				break;
			}
			case 7:
			{
				Day = 11;
				break;
			}
			case 9:
			{
				Day = 5;
				break;
			}
		}
	}

	// get day right
	int Diff = Index - (Day - _Day);
	while (Diff < 0) Diff += 7;

	return Diff % 7;
}

LDateTime LDateTime::Now()
{
	LDateTime dt;
	dt.SetNow();
	return dt;
}

LDateTime &LDateTime::SetNow()
{
    #ifdef WIN32

		SYSTEMTIME stNow;
		FILETIME ftNow;

		GetSystemTime(&stNow);
		SystemTimeToFileTime(&stNow, &ftNow);
		uint64 i64 = ((uint64)ftNow.dwHighDateTime << 32) | ftNow.dwLowDateTime;
		Set(i64);
    
    #else

		time_t now;
		time(&now);
		struct tm *time = localtime(&now);
		if (time)
			*this = time;
		#ifndef LGI_STATIC
		else
		{
    		LgiTrace("%s:%i - Error: localtime failed, now=%u\n", _FL, now);
		}
		#endif
	
	#endif

	return *this;
}

#define Convert24HrTo12Hr(h)			( (h) == 0 ? 12 : (h) > 12 ? (h) % 12 : (h) )
#define Convert24HrToAmPm(h)		( (h) >= 12 ? "p" : "a" )

LString LDateTime::GetDate() const
{
	char s[32];
	int Ch = GetDate(s, sizeof(s));
	return LString(s, Ch);
}

int LDateTime::GetDate(char *Str, size_t SLen) const
{
	int Ch = 0;

	if (Str && SLen > 0)
	{
		switch (_Format & GDTF_DATE_MASK)
		{
			case GDTF_MONTH_DAY_YEAR:
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%2.2i"  :"%i"  , _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%c%2.2i":"%c%i", DefaultSeparator, _Day);
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%c%i", DefaultSeparator, _Year);
				break;
			default:
			case GDTF_DAY_MONTH_YEAR:
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%2.2i"  :"%i"  , _Day);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%c%2.2i":"%c%i", DefaultSeparator, _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%c%i", DefaultSeparator, _Year);
				break;
			case GDTF_YEAR_MONTH_DAY:
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%i", _Year);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%c%2.2i":"%c%i", DefaultSeparator, _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%c%2.2i":"%c%i", DefaultSeparator, _Day);
				break;
		}
	}
	
	return Ch;
}

LString LDateTime::GetTime() const
{
	char s[32];
	int Ch = GetTime(s, sizeof(s));
	return LString(s, Ch);
}

int LDateTime::GetTime(char *Str, size_t SLen) const
{
	int Ch = 0;

	if (Str && SLen > 0)
	{
		switch (_Format & GDTF_TIME_MASK)
		{
			case GDTF_12HOUR:
			default:
			{
				Ch += sprintf_s(Str, SLen, "%i:%2.2i:%2.2i%s", Convert24HrTo12Hr(_Hours), _Minutes, _Seconds, Convert24HrToAmPm(_Hours));
				break;
			}
			case GDTF_24HOUR:
			{
				Ch += sprintf_s(Str, SLen, "%i:%2.2i:%2.2i", _Hours, _Minutes, _Seconds);
				break;
			}
		}
	}
	
	return Ch;
}

uint64 LDateTime::Ts() const
{
	uint64 ts = 0;
	Get(ts);
	return ts;
}

bool LDateTime::SetUnix(uint64 s)
{
	#if defined(WINDOWS)
	return Set(s * LDateTime::Second64Bit + 116445168000000000LL);
	#else
	return Set((s + Offset1800) * LDateTime::Second64Bit);
	#endif
}

bool LDateTime::Set(uint64 s)
{
	#if defined WIN32

		FILETIME Utc;
		SYSTEMTIME System;

		// Adjust to the desired timezone
		uint64 u = s + ((int64)_Tz * 60 * Second64Bit);

		Utc.dwHighDateTime = u >> 32;
		Utc.dwLowDateTime = u & 0xffffffff;
		if (!FileTimeToSystemTime(&Utc, &System))
			return false;
			
		_Year = System.wYear;
		_Month = System.wMonth;
		_Day = System.wDay;

		_Hours = System.wHour;
		_Minutes = System.wMinute;
		_Seconds = System.wSecond;
		_Thousands = System.wMilliseconds;

		return true;

	#else

		time_t t = (time_t) (((int64)(s / Second64Bit)) - Offset1800);
		Set(t);
		_Thousands = s % Second64Bit;
		return true;
	
	#endif
}

bool LDateTime::Set(time_t tt)
{
	struct tm *t;

#if !defined(_MSC_VER) || _MSC_VER < _MSC_VER_VS2005
	if (_Tz)
		tt += _Tz * 60;
	t = gmtime(&tt);
	if (t)
#else
	struct tm tmp;
	if (_localtime64_s(t = &tmp, &tt) == 0)
#endif
	{
		_Year = t->tm_year + 1900;
		_Month = t->tm_mon + 1;
		_Day = t->tm_mday;

		_Hours = t->tm_hour;
		_Minutes = t->tm_min;
		_Seconds = t->tm_sec;
		_Thousands = 0;
		
		// _Tz = SystemTimeZone();

		return true;
	}

	return false;
}

uint64_t LDateTime::OsTime() const
{
	#ifdef WINDOWS
	
		FILETIME Utc;
		SYSTEMTIME System;

		System.wYear			= _Year;
		System.wMonth			= limit(_Month, 1, 12);
		System.wDay				= limit(_Day, 1, 31);
		System.wHour			= limit(_Hours, 0, 23);
		System.wMinute			= limit(_Minutes, 0, 59);
		System.wSecond			= limit(_Seconds, 0, 59);
		System.wMilliseconds	= limit(_Thousands, 0, 999);
		System.wDayOfWeek		= DayOfWeek();

		if (SystemTimeToFileTime(&System, &Utc))
		{
			uint64_t s = ((uint64_t)Utc.dwHighDateTime << 32) | Utc.dwLowDateTime;

			if (_Tz)
				// Adjust for timezone
				s -= (int64)_Tz * 60 * Second64Bit;

			return s;
		}
		else
		{
			DWORD Err = GetLastError();
			LAssert(!"SystemTimeToFileTime failed.");
		}
	
	#else
	
		if (_Year < MIN_YEAR)
			return 0;

		struct tm t;
		ZeroObj(t);
		t.tm_year	= _Year - 1900;
		t.tm_mon	= _Month - 1;
		t.tm_mday	= _Day;

		t.tm_hour	= _Hours;
		t.tm_min	= _Minutes;
		t.tm_sec	= _Seconds;
		t.tm_isdst	= -1;
		
		time_t sec = timegm(&t);
		if (sec == -1)
			return 0;
		
		if (_Tz)
		{
			// Adjust the output to UTC from the current timezone.
			sec -= _Tz * 60;
		}
		
		return sec;
	
	#endif

	return 0;
}

bool LDateTime::OsTime(uint64_t ts)
{
	return Set((time_t)ts);
}

bool LDateTime::Get(uint64 &s) const
{
	#ifdef WINDOWS
	
		if (!IsValid())
		{
			LAssert(!"Needs a valid date.");
			return false;
		}

		s = OsTime();
		if (!s)
			return false;

		return true;
	
	#else
	
		if (_Year < MIN_YEAR)
			return false;

		auto sec = OsTime();				
		s = (uint64)(sec + Offset1800) * Second64Bit + _Thousands;
		
		return true;
	
	#endif
}

LString LDateTime::Get() const
{
	char buf[32];
	int Ch = GetDate(buf, sizeof(buf));
	buf[Ch++] = ' ';
	Ch += GetTime(buf+Ch, sizeof(buf)-Ch);
	return LString(buf, Ch);
}

void LDateTime::Get(char *Str, size_t SLen) const
{
	if (Str)
	{
		GetDate(Str, SLen);
		size_t len = strlen(Str);
		if (len < SLen - 1)
		{
			Str[len++] = ' ';
			GetTime(Str+len, SLen-len);
		}
	}
}

bool LDateTime::Set(const char *Str)
{
	if (!Str)
		return false;
	if (Strlen(Str) > 100)
		return false;

	char Local[256];
	strcpy_s(Local, sizeof(Local), Str);
	char *Sep = strchr(Local, ' ');
	if (Sep)
	{
		*Sep++ = 0;
		if (!SetTime(Sep))
			return false;
	}

	if (!SetDate(Local))
		return false;

	return true;
}

void LDateTime::Month(char *m)
{
	int i = IsMonth(m);
	if (i >= 0)
		_Month = i + 1;	
}

int DateComponent(const char *s)
{
	int64 i = Atoi(s);
	return i ? (int)i : LDateTime::IsMonth(s);
}

bool LDateTime::SetDate(const char *Str)
{
	bool Status = false;

	if (Str)
	{
		auto T = LString(Str).SplitDelimit("/-.,_\\");
		if (T.Length() == 3)
		{
			int i[3] = { DateComponent(T[0]), DateComponent(T[1]), DateComponent(T[2]) };
			int fmt = _Format & GDTF_DATE_MASK;
			
			// Do some guessing / overrides.
			// Don't let _Format define the format completely.
			if (i[0] > 1000)
			{
				fmt = GDTF_YEAR_MONTH_DAY;
			}
			else if (i[2] > 1000)
			{
				if (i[0] > 12)
					fmt = GDTF_DAY_MONTH_YEAR;
				else if (i[1] > 12)
					fmt = GDTF_MONTH_DAY_YEAR;
			}

			switch (fmt)
			{
				case GDTF_MONTH_DAY_YEAR:
				{
					_Month = i[0];
					_Day = i[1];
					_Year = i[2];
					break;
				}
				case GDTF_DAY_MONTH_YEAR:
				{
					_Day = i[0];
					_Month = i[1];
					_Year = i[2];
					break;
				}
				case GDTF_YEAR_MONTH_DAY:
				{
					_Year = i[0];
					_Month = i[1];
					_Day = i[2];
					break;
				}
				default:
				{
					_Year = i[2];
					if ((DefaultFormat & GDTF_DATE_MASK) == GDTF_MONTH_DAY_YEAR)
					{
						// Assume m/d/yyyy
						_Day = i[1];
						_Month = i[0];
					}
					else
					{
						// Who knows???
						// Assume d/m/yyyy
						_Day = i[0];
						_Month = i[1];
					}
					break;
				}
			}

			if (_Year < 100)
			{
				LAssert(_Day < 1000 && _Month < 1000);
				if (_Year >= 80)
					_Year += 1900;
				else
					_Year += 2000;
			}

			Status = true;
		}
		else
		{
			// Fall back to fuzzy matching
			auto T = LString(Str).SplitDelimit(" ,");
			MonthHash Lut;
			int FMonth = 0;
			int FDay = 0;
			int FYear = 0;
			for (unsigned i=0; i<T.Length(); i++)
			{
				char *p = T[i];
				if (IsDigit(*p))
				{
					int i = atoi(p);
					if (i > 0)
					{
						if (i >= 1000)
						{
							FYear = i;
						}
						else if (i < 32)
						{
							FDay = i;
						}
					}
				}
				else
				{
					int i = Lut.Find(p);
					if (i)
						FMonth = i;
				}
			}
			
			if (FMonth && FDay)
			{
				Day(FDay);
				Month(FMonth);
			}
			if (FYear)
			{
				Year(FYear);
			}
			else
			{
				LDateTime Now;
				Now.SetNow();
				Year(Now.Year());
			}
		}
	}

	return Status;
}

bool LDateTime::SetTime(const char *Str)
{
	if (!Str)
		return false;

	auto T = LString(Str).SplitDelimit(":.");
	if (T.Length() < 2 || T.Length() > 4)
		return false;

	#define SetClamp(out, in, minVal, maxVal) \
		out = (int)Atoi(in.Get(), 10, 0); \
		if (out > maxVal) out = maxVal; \
		else if (out < minVal) out = minVal

	SetClamp(_Hours, T[0], 0, 23);
	SetClamp(_Minutes, T[1], 0, 59);
	SetClamp(_Seconds, T[2], 0, 59);
	_Thousands = 0;

	const char *s = T.Last();
	if (s)
	{
		if (strchr(s, 'p') || strchr(s, 'P'))
		{
			if (_Hours != 12)
				_Hours += 12;
		}
		else if (strchr(s, 'a') || strchr(s, 'A'))
		{
			if (_Hours == 12)
				_Hours -= 12;
		}
	}

	if (T.Length() > 3)
	{
		LString t = "0.";
		t += s;
		_Thousands = (int) (t.Float() * 1000);
	}
	return true;
}

int LDateTime::IsWeekDay(const char *s)
{
	for (unsigned n=0; n<CountOf(WeekdaysShort); n++)
	{
		if (!_stricmp(WeekdaysShort[n], s) ||
			!_stricmp(WeekdaysLong[n], s))
			return n;
	}
	return -1;
}

int LDateTime::IsMonth(const char *s)
{
	for (unsigned n=0; n<CountOf(MonthsShort); n++)
	{
		if (!_stricmp(MonthsShort[n], s) ||
			!_stricmp(MonthsLong[n], s))
			return n;
	}
	return -1;
}

bool LDateTime::Parse(LString s)
{
	LString::Array a = s.Split(" ");

	Empty();

	for (unsigned i=0; i<a.Length(); i++)
	{
		const char *c = a[i];
		if (IsDigit(*c))
		{
			if (strchr(c, ':'))
			{
				LString::Array t = a[i].Split(":");
				if (t.Length() == 3)
				{
					Hours((int)t[0].Int());
					Minutes((int)t[1].Int());
					Seconds((int)t[2].Int());
				}
			}
			else if (strchr(c, '-') || strchr(c, '/'))
			{
				LString::Array t = a[i].SplitDelimit("-/");
				if (t.Length() == 3)
				{
					// What order are they in?
					if (t[0].Length() >= 4)
					{
						Year((int)t[0].Int());
						Month((int)t[1].Int());
						Day((int)t[2].Int());
					}
					else if (t[2].Length() >= 4)
					{
						Day((int)t[0].Int());
						Month((int)t[1].Int());
						Year((int)t[2].Int());
					}
					else
					{
						LAssert(!"Unknown date format?");
						return false;
					}
				}
			}
			else if (a[i].Length() == 4)
				Year((int)a[i].Int());
			else if (!Day())
				Day((int)a[i].Int());
		}
		else if (IsAlpha(*c))
		{
			int WkDay = IsWeekDay(c);
			if (WkDay >= 0)
				continue;
				
			int Mnth = IsMonth(c);
			if (Mnth >= 0)
				Month(Mnth + 1);
		}
		else if (*c == '-' || *c == '+')
		{
			c++;
			if (strlen(c) == 4)
			{
				// Timezone..
				int64 Tz = a[i].Int();
				int Hrs = (int) (Tz / 100);
				int Min = (int) (Tz % 100);
				SetTimeZone(Hrs * 60 + Min, false);
			}
		}
	}

	return IsValid();
}

int LDateTime::Sizeof()
{
	return sizeof(int) * 7;
}

bool LDateTime::Serialize(LFile &f, bool Write)
{
	int32 i;

	if (Write)
	{
		#define wf(fld) i = fld; f << i;
		wf(_Day);
		wf(_Month);
		wf(_Year);
		wf(_Thousands);
		wf(_Seconds);
		wf(_Minutes);
		wf(_Hours);
	}
	else
	{
		#define rf(fld) f >> i; fld = i;
		rf(_Day);
		rf(_Month);
		rf(_Year);
		rf(_Thousands);
		rf(_Seconds);
		rf(_Minutes);
		rf(_Hours);
	}

	return true;
}

/*
bool LDateTime::Serialize(ObjProperties *Props, char *Name, bool Write)
{
	#ifndef LGI_STATIC
	if (Props && Name)
	{
		struct _Date
		{
			uint8_t Day;
			uint8_t Month;
			int16_t Year;
			uint8_t Hour;
			uint8_t Minute;
			uint16_t ThouSec;
		};

		LAssert(sizeof(_Date) == 8);

		if (Write)
		{
			_Date d;

			d.Day = _Day;
			d.Month = _Month;
			d.Year = _Year;
			d.Hour = _Hours;
			d.Minute = _Minutes;
			d.ThouSec = (_Seconds * 1000) + _Thousands;

			return Props->Set(Name, &d, sizeof(d));
		}
		else // Read
		{
			void *Ptr;
			int Len;

			if (Props->Get(Name, Ptr, Len) &&
				sizeof(_Date) == Len)
			{
				_Date *d = (_Date*) Ptr;

				_Day = d->Day;
				_Month = d->Month;
				_Year = d->Year;
				_Hours = d->Hour;
				_Minutes = d->Minute;
				_Seconds = d->ThouSec / 1000;
				_Thousands = d->ThouSec % 1000;

				return true;
			}
		}
	}
	#endif

	return false;
}
*/

int LDateTime::Compare(const LDateTime *Date) const
{
	// this - *Date
	auto ThisTs = IsValid() ? Ts() : 0;
	auto DateTs = Date->IsValid() ? Date->Ts() : 0;

	// If these ever fire, the cast to int64_t will overflow
	LAssert((ThisTs & 0x800000000000000) == 0);
	LAssert((DateTs & 0x800000000000000) == 0);

	int64_t Diff = (int64_t)ThisTs - DateTs;
	if (Diff < 0)
		return -1;

	return Diff > 0 ? 1 : 0;
}

#define DATETIME_OP(op) \
	bool LDateTime::operator op(const LDateTime &dt) const \
	{ \
		auto a = Ts(); \
		auto b = dt.Ts(); \
		return a op b; \
	}
DATETIME_OP(<)
DATETIME_OP(<=)
DATETIME_OP(>)
DATETIME_OP(>=)

bool LDateTime::operator ==(const LDateTime &dt) const
{
	return	_Year == dt._Year &&
			_Month == dt._Month &&
			_Day == dt._Day &&
			_Hours == dt._Hours &&
			_Minutes == dt._Minutes &&
			_Seconds == dt._Seconds &&
			_Thousands == dt._Thousands;
}

bool LDateTime::operator !=(const LDateTime &dt) const
{
	return	_Year != dt._Year ||
			_Month != dt._Month ||
			_Day != dt._Day ||
			_Hours != dt._Hours ||
			_Minutes != dt._Minutes ||
			_Seconds != dt._Seconds ||
			_Thousands != dt._Thousands;
}

int LDateTime::DiffMonths(const LDateTime &dt)
{
	int a = (Year() * 12) + Month();
	int b = (dt.Year() * 12) + dt.Month();
	return b - a;
}

LDateTime LDateTime::operator -(const LDateTime &dt)
{
    uint64 a, b;
    Get(a);
    dt.Get(b);

    /// Resolution of a second when using 64 bit timestamps
    int64 Sec = Second64Bit;
    int64 Min = 60 * Sec;
    int64 Hr = 60 * Min;
    int64 Day = 24 * Hr;
    
    int64 d = (int64)a - (int64)b;
    LDateTime r;
    r._Day = (int16) (d / Day);
    d -= r._Day * Day;
    r._Hours = (int16) (d / Hr);
    d -= r._Hours * Hr;
    r._Minutes = (int16) (d / Min);
    d -= r._Minutes * Min;
    r._Seconds = (int16) (d / Sec);
	#ifdef WIN32
    d -= r._Seconds * Sec;
    r._Thousands = (int16) (d / 10000);
	#else
	r._Thousands = 0;
	#endif

	return r;
}

LDateTime LDateTime::operator +(const LDateTime &dt)
{
	LDateTime s = *this;

	s.AddMonths(dt.Month());
	s.AddDays(dt.Day());
	s.AddHours(dt.Hours());
	s.AddMinutes(dt.Minutes());
	// s.AddSeconds(dt.Seconds());

	return s;
}

LDateTime &LDateTime::operator =(const LDateTime &t)
{
	_Day = t._Day;
	_Year = t._Year;
	_Thousands = t._Thousands;
	_Month = t._Month;
	_Seconds = t._Seconds;
	_Minutes = t._Minutes;
	_Hours = t._Hours;
	_Tz = t._Tz;
	_Format = t._Format;

	return *this;
}

LDateTime &LDateTime::operator =(struct tm *time)
{
	if (time)
	{
		_Seconds = time->tm_sec;
		_Minutes = time->tm_min;
		_Hours = time->tm_hour;
		_Day = time->tm_mday;
		_Month = time->tm_mon + 1;
		_Year = time->tm_year + 1900;
	}
	else Empty();

	return *this;
}

bool LDateTime::IsSameDay(LDateTime &d) const
{
	return	Day() == d.Day() &&
			Month() == d.Month() &&
			Year() == d.Year();
}

bool LDateTime::IsSameMonth(LDateTime &d) const
{
	return	Day() == d.Day() &&
			Month() == d.Month();
}

bool LDateTime::IsSameYear(LDateTime &d) const
{
	return Year() == d.Year();
}

LDateTime LDateTime::StartOfDay() const
{
	LDateTime dt = *this;
	dt.Hours(0);
	dt.Minutes(0);
	dt.Seconds(0);
	dt.Thousands(0);
	return dt;
}

LDateTime LDateTime::EndOfDay() const
{
	LDateTime dt = *this;
	dt.Hours(23);
	dt.Minutes(59);
	dt.Seconds(59);
	dt.Thousands(999);
	return dt;
}

bool LDateTime::IsLeapYear(int Year) const
{
	if (Year < 0)
		Year = _Year;

	if (Year % 4 != 0)
		return false;

	if (Year % 400 == 0)
		return true;

	if (Year % 100 == 0)
		return false;

	return true;
}

int LDateTime::DaysInMonth() const
{
	if (_Month == 2 &&
		IsLeapYear())
	{
		return 29;
	}

	short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return _Month >= 1 && _Month <= 12 ? DaysInMonth[_Month-1] : 0;
}

void LDateTime::AddSeconds(int64 Seconds)
{
	uint64 i;
	if (Get(i))
	{
		i += Seconds * Second64Bit;
		Set(i);
	}
}

void LDateTime::AddMinutes(int64 Minutes)
{
	uint64 i;
	if (Get(i))
	{
		int64 delta = Minutes * 60 * Second64Bit;
		uint64 n = i + delta;
		// printf("AddMin " LPrintfInt64 " + " LPrintfInt64 " = " LPrintfInt64 "\n", i, delta, n);
		Set(n);
		
		#if 0
		uint64 i2;
		Get(i2);
		int64 diff = (int64)i2-(int64)i;
		#endif
	}
}

void LDateTime::AddHours(int64 Hours)
{
	uint64 i;
	if (Get(i))
	{
		i += Hours * LDateTime::HourLength * Second64Bit;
		Set(i);
	}
}

bool LDateTime::AddDays(int64 Days)
{
	if (!Days)
		return true;

	uint64 Ts;
	if (!Get(Ts))
		return false;

	Ts += Days * LDateTime::DayLength * Second64Bit;
	bool b = Set(Ts);
	return b;
}

void LDateTime::AddMonths(int64 Months)
{
	int64 m = _Month + Months;

	do
	{
		if (m < 1)
		{
			_Year--;
			m += 12;
		}
		else if (m > 12)
		{
			_Year++;
			m -= 12;
		}
		else
		{
			break;
		}
	}
	while (1);

	_Month = (int16) m;
	if (_Day > DaysInMonth())
		_Day = DaysInMonth();
}

LString LDateTime::DescribePeriod(double seconds)
{
	int mins = (int) (seconds / 60);
	seconds -= mins * 60;
	int hrs = mins / 60;
	mins -= hrs * 60;
	int days = hrs / 24;
	hrs -= days * 24;
	
	LString s;
	if (days > 0)
		s.Printf("%id %ih %im %is", days, hrs, mins, (int)seconds);
	else if (hrs > 0)
		s.Printf("%ih %im %is", hrs, mins, (int)seconds);
	else if (mins > 0)
		s.Printf("%im %is", mins, (int)seconds);
	else
		s.Printf("%is", (int)seconds);
	return s;
}

LString LDateTime::DescribePeriod(LDateTime to)
{
	auto ThisTs = Ts();
	auto ToTs = to.Ts();
	auto diff = ThisTs < ToTs ? ToTs - ThisTs : ThisTs - ToTs;

	auto seconds = (double)diff / LDateTime::Second64Bit;
	return DescribePeriod(seconds);
}

int LDateTime::MonthFromName(const char *Name)
{
	if (Name)
	{
		for (int m=0; m<12; m++)
		{
			if (strnicmp(Name, MonthsShort[m], strlen(MonthsShort[m])) == 0)
			{
				return m + 1;
				break;
			}
		}
	}
	
	return -1;
}

bool LDateTime::Decode(const char *In)
{
	// Test data:
	//
	//		Tue, 6 Dec 2005 1:25:32 -0800
	Empty();

	if (!In)
	{
		LAssert(0);
		return false;
	}

	bool Status = false;

	// Tokenize delimited by whitespace
	LString::Array T = LString(In).SplitDelimit(", \t\r\n");
	if (T.Length() < 2)
	{
		if (T[0].IsNumeric())
		{
			// Some sort of timestamp?
			uint64_t Ts = Atoi(T[0].Get());
			if (Ts > 0)
			{
				return SetUnix(Ts);
			}
			else return false;
		}
		else
		{
			// What now?
			return false;
		}
	}
	else
	{
		bool GotDate = false;

		for (unsigned i=0; i<T.Length(); i++)
		{
			LString &s = T[i];
				
			LString::Array Date;
			if (!GotDate)
				Date = s.SplitDelimit(".-/\\");
			if (Date.Length() == 3)
			{
				if (Date[0].Int() > 31)
				{
					// Y/M/D?
					Year((int)Date[0].Int());
					Day((int)Date[2].Int());
				}
				else if (Date[2].Int() > 31)
				{
					// D/M/Y?
					Day((int)Date[0].Int());
					Year((int)Date[2].Int());
				}
				else
				{
					// Ambiguous year...
					bool YrFirst = true;
					if (Date[0].Length() == 1)
						YrFirst = false;
					// else we really can't tell.. just go with year first
					if (YrFirst)
					{
						Year((int)Date[0].Int());
						Day((int)Date[2].Int());
					}
					else
					{
						Day((int)Date[0].Int());
						Year((int)Date[2].Int());
					}

					LDateTime Now;
					Now.SetNow();
					if (Year() + 2000 <= Now.Year())
						Year(2000 + Year());
					else
						Year(1900 + Year());
				}

				if (Date[1].IsNumeric())
					Month((int)Date[1].Int());
				else
				{
					int m = MonthFromName(Date[1]);
					if (m > 0)
						Month(m);
				}
						
				GotDate = true;
				Status = true;
			}
			else if (s.Find(":") >= 0)
			{
				// whole time

				// Do some validation
				bool Valid = true;
				for (char *c = s; *c && Valid; c++)
				{
					if (!(IsDigit(*c) || *c == ':'))
						Valid = false;
				}

				if (Valid)
				{
					LString::Array Time = s.Split(":");
					if (Time.Length() == 2 ||
						Time.Length() == 3)
					{
						// Hour
						int i = (int) Time[0].Int();
						if (i >= 0)
							Hours(i);
						if (s.Lower().Find("p") >= 0)
						{
							if (Hours() < 12)
								Hours(Hours() + 12);
						}

						// Minute
						i = (int) Time[1].Int();
						if (i >= 0)
							Minutes(i);

						if (Time.Length() == 3)
						{
							// Second
							i = (int) Time[2].Int();
							if (i >= 0)
								Seconds(i);
						}
						
						Status = true;
					}
				}
			}
			else if (IsAlpha(s(0)))
			{
				// text
				int m = MonthFromName(s);
				if (m > 0)
					Month(m);
			}
			else if (strchr("+-", *s))
			{
				// timezone
				DoTimeZone:
				LDateTime Now;
				double OurTmz = (double)Now.SystemTimeZone() / 60;

				if (s &&
					strchr("-+", *s) &&
					strlen(s) == 5)
				{
					#if 1

					int i = atoi(s);
					int hr = i / 100;
					int min = i % 100;
					SetTimeZone(hr * 60 + min, false);

					#else

					// adjust for timezone
					char Buf[32];
					memcpy(Buf, s, 3);
					Buf[3] = 0;

					double TheirTmz = atof(Buf);
					memcpy(Buf+1, s + 3, 2);

					TheirTmz += (atof(Buf) / 60);
					if (Tz)
					{
						*Tz = TheirTmz;
					}

					double AdjustHours = OurTmz - TheirTmz;

					AddMinutes((int) (AdjustHours * 60));

					#endif
				}
				else
				{
					// assume GMT
					AddMinutes((int) (OurTmz * 60));
				}
			}
			else if (s.IsNumeric())
			{
				int Count = 0;
				for (char *c = s; *c; c++)
				{
					if (!IsDigit(*c))
						break;
					Count++;
				}
					
				if (Count <= 2)
				{
					if (Day())
					{
						// We already have a day... so this might be
						// a 2 digit year...
						LDateTime Now;
						Now.SetNow();
						int Yr = atoi(s);
						if (2000 + Yr <= Now.Year())
							Year(2000 + Yr);
						else
							Year(1900 + Yr);
					}
					else
					{
						// A day number (hopefully)?
						Day((int)s.Int());
					}
				}
				else if (Count == 4)
				{
					if (!Year())
					{
						// A year!
						Year((int)s.Int());
						Status = true;
					}
					else
					{
						goto DoTimeZone;
					}
						
					// My one and only Y2K fix
					// d.Year((Yr < 100) ? (Yr > 50) ? 1900+Yr : 2000+Yr : Yr);
				}
			}
		}
	}

	return Status;
}

bool LDateTime::GetVariant(const char *Name, LVariant &Dst, char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case DateYear: // Type: Int32
			Dst = Year();
			break;
		case DateMonth: // Type: Int32
			Dst = Month();
			break;
		case DateDay: // Type: Int32
			Dst = Day();
			break;
		case DateHour: // Type: Int32
			Dst = Hours();
			break;
		case DateMinute: // Type: Int32
			Dst = Minutes();
			break;
		case DateSecond: // Type: Int32
			Dst = Seconds();
			break;
		case DateDate: // Type: String
		{
			char s[32];
			GetDate(s, sizeof(s));
			Dst = s;
			break;
		}
		case DateTime: // Type: String
		{
			char s[32];
			GetTime(s, sizeof(s));
			Dst = s;
			break;
		}
		case TypeString: // Type: String
		case DateDateAndTime: // Type: String
		{
			char s[32];
			Get(s, sizeof(s));
			Dst = s;
			break;
		}
		case TypeInt: // Type: Int64
		case DateTimestamp: // Type: Int64
		{
			uint64 i = 0;
			Get(i);
			Dst = (int64)i;
			break;
		}
		case DateSecond64Bit:
		{
			Dst = Second64Bit;
			break;
		}
		default:
		{
			return false;
		}
	}
	return true;
}

bool LDateTime::SetVariant(const char *Name, LVariant &Value, char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case DateYear:
			Year(Value.CastInt32());
			break;
		case DateMonth:
			Month(Value.CastInt32());
			break;
		case DateDay:
			Day(Value.CastInt32());
			break;
		case DateHour:
			Hours(Value.CastInt32());
			break;
		case DateMinute:
			Minutes(Value.CastInt32());
			break;
		case DateSecond:
			Seconds(Value.CastInt32());
			break;
		case DateDate:
			SetDate(Value.Str());
			break;
		case DateTime:
			SetTime(Value.Str());
			break;
		case DateDateAndTime:
			Set(Value.Str());
			break;
		case DateTimestamp:
			Set((uint64)Value.CastInt64());
			break;
		default:
			return false;
	}
	
	return true;
}

bool LDateTime::CallMethod(const char *Name, LVariant *ReturnValue, LArray<LVariant*> &Args)
{
	switch (LStringToDomProp(Name))
	{
		case DateSetNow:
			SetNow();
			if (ReturnValue)
				*ReturnValue = true;
			break;
		case DateSetStr:
			if (Args.Length() < 1)
				return false;

			bool Status;
			if (Args[0]->Type == GV_INT64)
				Status = Set((uint64) Args[0]->Value.Int64);
			else
				Status = Set(Args[0]->Str());
			if (ReturnValue)
				*ReturnValue = Status;
			break;
		case DateGetStr:
		{
			char s[256] = "";
			Get(s, sizeof(s));
			if (ReturnValue)
				*ReturnValue = s;
			break;
		}
		default:
			return false;
	}

	return true;
}

#ifdef _DEBUG
#define DATE_ASSERT(i) \
	if (!(i)) \
	{ \
		LAssert(!"LDateTime unit test failed."); \
		return false; \
	}

bool LDateTime_Test()
{
	// Check 64bit get/set
	LDateTime t("1/1/2017 0:0:0");
	uint64 i;
	DATE_ASSERT(t.Get(i));
	LgiTrace("Get='%s'\n", t.Get().Get());
	uint64 i2 = i + (24ULL * 60 * 60 * LDateTime::Second64Bit);
	LDateTime t2;
	t2.SetFormat(GDTF_DAY_MONTH_YEAR);
	t2.Set(i2);
	LString s = t2.Get();
	LgiTrace("Set='%s'\n", s.Get());
	DATE_ASSERT(!stricmp(s, "2/1/2017 12:00:00a") ||
				!stricmp(s, "2/01/2017 12:00:00a"));
	
	t.SetNow();
	LgiTrace("Now.Local=%s Tz=%.2f\n", t.Get().Get(), t.GetTimeZoneHours());
	t2 = t;
	t2.ToUtc();
	LgiTrace("Now.Utc=%s Tz=%.2f\n", t2.Get().Get(), t2.GetTimeZoneHours());
	t2.ToLocal();
	LgiTrace("Now.Local=%s Tz=%.2f\n", t2.Get().Get(), t2.GetTimeZoneHours());
	DATE_ASSERT(t == t2);
	
	return true;
}
#endif
