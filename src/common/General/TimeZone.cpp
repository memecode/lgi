#include "lgi/common/Lgi.h"
#include "lgi/common/DateTime.h"

// From HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones

static LTimeZone AllTimeZones[] =
{
	{-12.00,	"Dateline (Eniwetok, Kwajalein)"},
	{-11.00,	"Samoa (Midway Island, Samoa)"},
	{-10.00,	"Hawaiian (Hawaii)"},
	{-9.00,		"Alaskan (Alaska)"},
	{-8.00,		"Pacific (Pacific Time (US & Canada); Tijuana)"},
	{-7.00,		"Mountain (Mountain Time (US & Canada))"},
	{-7.00,		"US Mountain (Arizona)"},
	{-6.00,		"Canada Central (Saskatchewan)"},
	{-6.00,		"Central America (Central America)"},
	{-6.00,		"Central (Central Time (US & Canada))"},
	{-6.00,		"Mexico (Mexico City)"},
	{-5.00,		"Eastern (Eastern Time (US & Canada))"},
	{-5.00,		"SA Pacific (Bogota, Lima, Quito)"},
	{-5.00,		"US Eastern (Indiana (East))"},
	{-4.00,		"Atlantic (Atlantic Time (Canada))"},
	{-4.00,		"Pacific SA (Santiago)"},
	{-4.00,		"SA Western (Caracas, La Paz)"},
	{-3.00,		"East South America (Brasilia)"},
	{-3.00,		"Greenland (Greenland)"},
	{-3.00,		"SA Eastern (Buenos Aires, Georgetown)"},
	{-2.50,		"Newfoundland (Newfoundland)"},
	{-2.00,		"Mid-Atlantic (Mid-Atlantic)"},
	{-1.00,		"Azores (Azores)"},
	{-1.00,		"Cape Verde (Cape Verde Is.)"},
	{0.00,		"GMT"},
	{1.00,		"Central Europe (Belgrade, Bratislava, Budapest, Ljubljana, Prague)"},
	{1.00,		"Central European (Sarajevo, Skopje, Sofija, Vilnius, Warsaw, Zagreb)"},
	{1.00,		"Romance (Brussels, Copenhagen, Madrid, Paris)"},
	{1.00,		"West Central Africa (West Central Africa)"},
	{1.00,		"West Europe (Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna)"},
	{2.00,		"East Europe (Bucharest)"},
	{2.00,		"Egypt (Cairo)"},
	{2.00,		"FLE (Helsinki, Riga, Tallin, Vilnius)"},
	{2.00,		"GTB (Athens, Istanbul, Minsk)"},
	{2.00,		"Jerusalem (Jerusalem)"},
	{2.00,		"South Africa (Harare, Pretoria)"},
	{3.00,		"Arab (Kuwait, Riyadh)"},
	{3.00,		"Arabic (Baghdad)"},
	{3.00,		"East Africa (Nairobi)"},
	{3.00,		"Russian (Moscow, St. Petersburg, Volgograd)"},
	{3.50,		"Iran (Tehran)"},
	{4.00,		"Arabian (Abu Dhabi, Muscat)"},
	{4.00,		"Caucasus (Baku, Tbilisi, Yerevan)"},
	{4.50,		"Afghanistan (Kabul)"},
	{5.00,		"Ekaterinburg (Ekaterinburg)"},
	{5.00,		"West Asia (Islamabad, Karachi, Tashkent)"},
	{5.50,		"India (Calcutta, Chennai, Mumbai, New Delhi)"},
	{5.75,		"Nepal (Kathmandu)"},
	{6.00,		"Central Asia (Astana, Dhaka)"},
	{6.00,		"N. Central Asia (Almaty, Novosibirsk)"},
	{6.00,		"Sri Lanka (Sri Jayawardenepura)"},
	{6.50,		"Myanmar (Rangoon)"},
	{7.00,		"North Asia (Krasnoyarsk)"},
	{7.00,		"SE Asia (Bangkok, Hanoi, Jakarta)"},
	{8.00,		"China (Beijing, Chongqing, Hong Kong, Urumqi)"},
	{8.00,		"North Asia East (Irkutsk, Ulaan Bataar)"},
	{8.00,		"Malay Peninsula (Kuala Lumpur, Singapore)"},
	{8.00,		"Taipei (Taipei)"},
	{8.00,		"West Australia (Perth)"},
	{9.00,		"Korea (Seoul)"},
	{9.00,		"Tokyo (Osaka, Sapporo, Tokyo)"},
	{9.00,		"Yakutsk (Yakutsk)"},
	{9.50,		"AUS Central (Darwin)"},
	{9.50,		"Cen. Australia (Adelaide)"},
	{10.00,		"AUS Eastern (Canberra, Melbourne, Sydney)"},
	{10.00,		"East Australia (Brisbane)"},
	{10.00,		"Tasmania (Hobart)"},
	{10.00,		"Vladivostok (Vladivostok)"},
	{10.00,		"West Pacific (Guam, Port Moresby)"},
	{11.00,		"Central Pacific (Magadan, Solomon Is., New Caledonia)"},
	{12.00,		"Fiji (Fiji, Kamchatka, Marshall Is.)"},
	{12.00,		"New Zealand (Auckland, Wellington)"},
	{13.00,		"Tonga (Nuku'alofa)"},
	{0,			0}
};

LTimeZone *LTimeZone::GetTimeZones()
{
	return AllTimeZones;
}

LDateTime LDstInfo::GetLocal()
{
	LDateTime d;
	d.Set(Utc);
	d.SetTimeZone(0, false);
	d.SetTimeZone(Offset, true);
	return d;
}

bool LTimeZone::GetDaylightSavingsInfo(LArray<LDstInfo> &Info, LDateTime &Start, LDateTime *End)
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
				i->Utc = tmp;
			}
			else
			{
				// Year is: Standard->Daylight->Standard
				LDateTime tmp = s;
				i->Offset = -(Tzi.Bias + Tzi.StandardBias);
				tmp.AddMinutes(-i->Offset);
				i->Utc = tmp;
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
					i->Utc = tmp;

					// Cur year, second event: start of DST
					i = &Info.New();
					tmp = ConvertSysTime(Tzi.DaylightDate, y);
					i->Offset = -(Tzi.Bias + Tzi.DaylightBias);
					tmp.AddMinutes(-i->Offset);
					i->Utc = tmp;
				}
				else
				{
					// Cur year, first event: start of DST
					i = &Info.New();
					auto tmp = ConvertSysTime(Tzi.DaylightDate, Start.Year());
					i->Offset = -(Tzi.Bias + Tzi.DaylightBias);
					tmp.AddMinutes(-i->Offset);
					i->Utc = tmp;

					// Cur year, second event: end of DST
					i = &Info.New();
					tmp = ConvertSysTime(Tzi.StandardDate, Start.Year());
					i->Offset = -(Tzi.Bias + Tzi.StandardBias);
					tmp.AddMinutes(-i->Offset);
					i->Utc = tmp;
				}
			}

			Status = true;
		}
	
	#elif defined(MAC)
	
		LDateTime From = Start;
		From.AddMonths(-6);
		LDateTime To = End ? *End : Start;
		To.AddMonths(6);
		auto ToUnix = To.GetUnix();

		auto tz = [NSTimeZone systemTimeZone];
		auto startDate = [[NSDate alloc] initWithTimeIntervalSince1970:(From.Ts().Get() / Second64Bit) - Offset1800];
		while (startDate)
		{
			auto next = [tz nextDaylightSavingTimeTransitionAfterDate:startDate];
			auto &i = Info.New();
			
			auto nextTs = (time_t)[next timeIntervalSince1970];
			i.Utc = nextTs;
			i.Offset = (int)([tz secondsFromGMTForDate:[next dateByAddingTimeInterval:60]]/60);
			
			#if DEBUG_DST_INFO
			{
				LDateTime dt;
				dt.Set(i.Utc);
				LgiTrace("%s:%i - Ts=%s Off=%i\n", _FL, dt.Get().Get(), i.Offset);
			}
			#endif

			if (nextTs >= ToUnix)
				break;

			[startDate release];
			startDate = next;
		}
		if (startDate)
			[startDate release];

	#elif USE_ZDUMP

		if (!Zdump.Length())
		{
			static bool First = true;
			
			auto linkLoc = "/etc/localtime";
			#if defined(LINUX)
				auto zoneLoc = "/usr/share/zoneinfo";
			#elif defined(HAIKU)
				auto zoneLoc = "/boot/system/data/zoneinfo";
			#else
				#error "Impl me"
			#endif

			if (!LFileExists(linkLoc))
			{
				if (First)
				{
					LgiTrace("%s:%i - LDateTime::GetDaylightSavingsInfo error: '%s' doesn't exist.\n"
							 "    It should link to something in the '%s' tree.\n",
							 _FL,
							 linkLoc,
							 zoneLoc);
					#ifdef HAIKU
					LgiTrace("    To fix that: pkgman install timezone_data and then create the '%s' link.\n", linkLoc);
					#endif
				}
				return First = false;
			}

			auto f = popen(LString::Fmt("zdump -v %s", linkLoc), "r");
			if (f)
			{
				char s[1024];
				size_t r;
				LStringPipe p(1024);
				while ((r = fread(s, 1, sizeof(s), f)) > 0)
					p.Write(s, (int)r);

				fclose(f);
				Zdump = p.NewLStr().Split("\n");
			}
			else
			{
				if (First)
				{
					LgiTrace("%s:%i - LDateTime::GetDaylightSavingsInfo error: zdump didn't run.\n", _FL);
					#ifdef HAIKU
					LgiTrace("To fix that: pkgman install timezone_data\n");
					#endif
				}
				return First = false;
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
				
				LDateTime Utc;
				Utc.Year(l[5].Int());

				#if DEBUG_DST_INFO
				if (Utc.Year() < 2020)
					continue;
				// printf("DST: %s\n", Line.Get());
				#endif				

				auto Tm = l[4].SplitDelimit(":");
				if (Tm.Length() != 3)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Tm '%s' has wrong parts: %s\n", _FL, l[4].Get(), Line.Get());
					#endif
					continue;
				}

				Utc.Hours(Tm[0].Int());
				Utc.Minutes(Tm[1].Int());
				Utc.Seconds(Tm[2].Int());
				if (Utc.Minutes() < 0)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Mins is zero: %s\n", _FL, l[4].Get());
					#endif
					continue;
				}

				int m = Lut.Find(l[2]);
				if (!m)
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown month '%s'\n", _FL, l[2].Get());
					#endif
					continue;
				}

				Utc.Day(l[3].Int());
				Utc.Month(m);

				LString Var, Val;
				if (!ParseValue(l[14], Var, Val) ||
					Var != "isdst")
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown value for isdst\n", _FL);
					#endif
					continue;
				}
				
				if (!ParseValue(l[15], Var, Val) ||
					Var != "gmtoff")
				{
					#if DEBUG_DST_INFO
					printf("%s:%i - Unknown value for isdst\n", _FL);
					#endif
					continue;
				}
				
				int Off = atoi(Val) / 60;

				if (Utc.Ts().Valid() == 0)
					continue;

				if (Prev.Year() &&
					Prev < Start &&
					Start < Utc)
				{
					// Emit initial entry for 'start'
					auto &inf = Info.New();
					Prev.Get(inf.Utc);
					inf.Offset = PrevOff;
					#if DEBUG_DST_INFO
					printf("Info: Start=%s %i\n", Prev.Get().Get(), inf.Offset);
					#endif
				}
				
				if (Utc > Start)
				{
					// Emit furthur entries for DST events between start and end.
					auto &inf = Info.New();
					Utc.Get(inf.Utc);
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

	#elif defined(HAIKU)

		LTimeZoneInfo tzinfo;
		
		if (!tzinfo.Read())
		{
			#if DEBUG_DST_INFO
			LgiTrace("%s:%i - info read failed.\n", _FL);
			#endif
			return false;
		}
		
		Status = tzinfo.GetDaylightSavingsInfo(Info, Start, End);
		#if DEBUG_DST_INFO
		if (!Status)
			printf("%s:%i - GetDaylightSavingsInfo failed.\n", _FL);
		#endif
		
	
	#else

		LAssert(!"Not implemented.");

	#endif

	return Status;
}

bool LTimeZone::DstToLocal(LArray<LDstInfo> &Dst, LDateTime &dt)
{
	if (dt.GetTimeZone())
	{
		LAssert(!"Should be a UTC date.");
		return true;
	}

	#if DEBUG_DST_INFO
	LgiTrace("DstToLocal: %s\n", dt.Get().Get());
	#endif
	LAssert(Dst.Length() > 1); // Needs to have at least 2 entries...?
	for (size_t i=0; i<Dst.Length()-1; i++)
	{
		auto &a = Dst[i];
		auto &b = Dst[i+1];
		LDateTime start, end;
		start.Set(a.Utc);
		end.Set(b.Utc);

		auto InRange = dt >= start && dt < end;
		if (InRange)
		{
			dt.SetTimeZone(a.Offset, true);
			#if DEBUG_DST_INFO
			LgiTrace("\tRng[%i]: %s -> %s, SetTimeZone(%g), dt=%s\n",
				(int)i,
				start.Get().Get(), end.Get().Get(),
				(double)a.Offset/60.0,
				dt.Get().Get());
			#endif
			return true;
		}
	}

	auto Last = Dst.Last();
	LDateTime d;
	d.Set(Last.Utc);
	if (dt >= d && dt.Year() == d.Year())
	{
		// If it's after the last DST change but in the same year... it's ok...
		// Just use the last offset.
		dt.SetTimeZone(Last.Offset, true);
		return true;
	}

	#if DEBUG_DST_INFO
	for (auto d: Dst)
		LgiTrace("Dst: %s = %i\n", d.GetLocal().Get().Get(), d.Offset);
	#endif
	LgiTrace("%s:%i - No valid DST range for: %s\n", _FL, dt.Get().Get());
	static bool first = true;
	if (first)
	{
		first = false;
		LAssert(!"No valid DST range for this date.");
	}
	return false;
}

