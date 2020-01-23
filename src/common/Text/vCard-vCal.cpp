#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "vCard-vCal.h"
#include "GToken.h"
#include "ScribeDefs.h"
#include "LJson.h"

#define Push(s) Write(s, (int)strlen(s))

#define ClearFields() \
	Field.Empty(); \
	Params.Empty(); \
	Data.Empty()


#define IsType(str) (Params.Find(str) != 0)

struct TzMap
{
	int Offset;
	const char *Name;
};

#define TZ(h,m) (((h)*100) + (m))
static TzMap Timezones[] =
{
	{TZ(11,0), "Australia/Sydney"},
};

int TimezoneToOffset(const char *Tz)
{
	if (!Tz)
		return 0;

	for (int i=0; i<CountOf(Timezones); i++)
	{
		auto *t = Timezones + i;
		if (!Stricmp(t->Name, Tz))
			return t->Offset;
	}

	return (int)Atoi(Tz);
}

#if 1
bool IsVar(char *field, const char *s)
{
	if (!s)
		return false;

	char *dot = strchr(field, '.');
	if (dot)
		return _stricmp(dot + 1, s) == 0;

	return _stricmp(field, s) == 0;
}
#else
#define IsVar(field, str) (field != 0 && _stricmp(field, str) == 0)
#endif

char *DeEscape(char *s, bool QuotedPrintable)
{
	if (!s)
		return 0;

	char *i = s;
	char *o = s;
	while (*i)
	{
		if (*i == '\\')
		{
			i++;
			switch (*i)
			{
				case 'n':
				case 'N':
					*o++ = '\n';
					break;
				case ',':
				case ';':
				case ':':
					*o++ = *i;
					break;
				default:
					*o++ = '\\';
					i--;
					break;
			}
			
		}
		else if (QuotedPrintable && *i == '=' && i[1] && i[2])
		{
			i++;
			char h[3] = { i[0], i[1], 0};
			*o++ = htoi(h);
			i++;
		}
		else
		{
			*o++ = *i;
		}

		i++;
	}
	*o = 0;
	return s;
}

/////////////////////////////////////////////////////////////
// General IO class
class VIoPriv
{
public:
	GStringPipe Buf;
};

VIo::VIo() : d(new VIoPriv)
{
}

VIo::~VIo()
{
	DeleteObj(d);
}

bool VIo::ParseDate(LDateTime &Out, char *In)
{
	bool Status = false;

	if (In)
	{
		Out.SetTimeZone(0, false);

		GToken v(In, "T");
		if (v.Length() > 0)
		{
			char *d = v[0];
			if (d && strlen(d) == 8)
			{
				char Year[5] = {d[0], d[1], d[2], d[3], 0};
				char Month[3] = {d[4], d[5], 0};
				char Day[3] = {d[6], d[7], 0};
				Out.Year(atoi(Year));
				Out.Month(atoi(Month));
				Out.Day(atoi(Day));
				Status = true;
			}

			char *t = v[1];
			if (t && strlen(t) >= 6)
			{
				char Hour[3] = {t[0], t[1], 0};
				char Minute[3] = {t[2], t[3], 0};
				char Second[3] = {t[4], t[5], 0};
				Out.Hours(atoi(Hour));
				Out.Minutes(atoi(Minute));
				Out.Seconds(atoi(Second));
				Status = true;
			}
		}
	}

	return Status;
}

bool VIo::ParseDuration(LDateTime &Out, int &Sign, char *In)
{
	bool Status = false;

	if (In)
	{
		Sign = 1;
		if (*In == '-')
		{
			Sign = -1;
			In++;
		}

		if (toupper(*In++) == 'P' &&
			toupper(*In++) == 'T')
		{
			while (*In)
			{
				int i = atoi(In);
				while (IsDigit(*In)) In++;

				switch (toupper(*In++))
				{
					case 'W':
					{
						Out.Day(Out.Day() + (i * 7));
						break;
					}
					case 'D':
					{
						Out.Day(Out.Day() + i);
						break;
					}
					case 'H':
					{
						Out.Hours(Out.Hours() + i);
						break;
					}
					case 'M':
					{
						Out.Minutes(Out.Minutes() + i);
						break;
					}
					case 'S':
					{
						Out.Seconds(Out.Seconds() + i);
						break;
					}
				}
			}

			Status = true;
		}
	}

	return Status;
}

void VIo::Fold(GStreamI &o, const char *i, int pre_chars)
{
	int x = pre_chars;
	for (const char *s=i; s && *s;)
	{
		if (x >= 74)
		{
			// wrapping
			o.Write(i, (int)(s-i));
			o.Write((char*)"\r\n\t", 3);
			x = 0;
			i = s;
		}
		else if (*s == '=' || ((((uint8_t)*s) & 0x80) != 0))
		{
			// quoted printable
			o.Write(i, (int)(s-i));
			GStreamPrint(&o, "=%02.2x", (uint8_t)*s);
			x += 3;
			i = ++s;
		}
		else if (*s == '\n')
		{
			// new line
			o.Write(i, (int)(s-i));
			o.Write((char*)"\\n", 2);
			x += 2;
			i = ++s;
		}
		else if (*s == '\r')
		{
			o.Write(i, (int)(s-i));
			i = ++s;
		}
		else
		{
			s++;
			x++;
		}
	}
	o.Write(i, (int)strlen(i));
}

char *VIo::Unfold(char *In)
{
	if (In)
	{
		GStringPipe p(256);

		for (char *i=In; i && *i; i++)
		{
			if (*i == '\n')
			{
				if (i[1] && strchr(" \t", i[1]))
				{
					i++;
				}
				else
				{
					p.Write(i, 1);
				}
			}
			else
			{
				p.Write(i, 1);
			}
		}

		return p.NewStr();
	}

	return 0;
}

char *VIo::UnMultiLine(char *In)
{
	if (In)
	{
		GStringPipe p;
		char *n;
		for (char *i=In; i && *i; i=n)
		{
			n = stristr(i, "\\n");
			if (n)
			{
				p.Write(i, (int)(n-i));
				p.Push((char*)"\n");
				n += 2;
			}
			else
			{
				p.Push(i);
			}
		}

		return p.NewStr();
	}

	return 0;
}

/////////////////////////////////////////////////////////////
// VCard class
bool VCard::Import(GDataPropI *c, GStreamI *s)
{
	bool Status = false;

	if (!c || !s)
		return false;

	GString Field;
	ParamArray Params;
	GString Data;

	ssize_t PrefEmail = -1;
	GArray<char*> Emails;

	while (ReadField(*s, Field, &Params, Data))
	{
		if (_stricmp(Field, "begin") == 0 &&
			_stricmp(Data, "vcard") == 0)
		{
			while (ReadField(*s, Field, &Params, Data))
			{
				if (_stricmp(Field, "end") == 0 &&
					_stricmp(Data, "vcard") == 0)
					goto ExitLoop;

				if (IsVar(Field, "n"))
				{
					GToken Name(Data, ";", false);
					char *First = Name[1];
					char *Last = Name[0];
					char *Title = Name[3];

					if (First)
					{
						c->SetStr(FIELD_FIRST_NAME, First);
						Status = true;
					}

					if (Last)
					{
						c->SetStr(FIELD_LAST_NAME, Last);
						Status = true;
					}

					if (Title)
					{
						c->SetStr(FIELD_TITLE, Title);
						Status = true;
					}
				}
				else if (IsVar(Field, "nickname"))
				{
					c->SetStr(FIELD_NICK, Data);
				}
				else if (IsVar(Field, "tel"))
				{
					GToken Phone(Data, ";", false);
					for (uint32_t p=0; p<Phone.Length(); p++)
					{
						if (IsType("cell"))
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_MOBILE, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_MOBILE, Phone[p]);
							}
						}
						else if (IsType("fax"))
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_FAX, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_FAX, Phone[p]);
							}
						}
						else
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_PHONE, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_PHONE, Phone[p]);
							}
						}
					}
				}
				else if (IsVar(Field, "email"))
				{
					if (IsType("pref"))
					{
						PrefEmail = Emails.Length();
					}
					Emails.Add(NewStr(Data));
				}
				else if (IsVar(Field, "org"))
				{
					GToken Org(Data, ";", false);
					if (Org[0]) c->SetStr(FIELD_COMPANY, Org[0]);
				}
				else if (IsVar(Field, "adr"))
				{
					bool IsWork = IsType("work");
					// bool IsHome = IsType("home");
					GToken Addr(Data, ";", false);
					if (Addr[2])
					{
						GToken A(Addr[2], "\r\n");
						if (A.Length() > 1)
						{
							c->SetStr(IsWork ? FIELD_WORK_STREET : FIELD_HOME_STREET, A[0]);
							if (A[1]) c->SetStr(IsWork ? FIELD_WORK_SUBURB : FIELD_HOME_SUBURB, A[1]);
							if (A[2]) c->SetStr(IsWork ? FIELD_WORK_COUNTRY : FIELD_HOME_COUNTRY, A[2]);
						}
						else
						{
							c->SetStr(IsWork ? FIELD_WORK_STREET : FIELD_HOME_STREET, Addr[2]);
						}
					}

					if (Addr[3]) c->SetStr(IsWork ? FIELD_WORK_SUBURB : FIELD_HOME_SUBURB, Addr[3]);
					if (Addr[4]) c->SetStr(IsWork ? FIELD_WORK_STATE : FIELD_HOME_STATE, Addr[4]);
					if (Addr[5]) c->SetStr(IsWork ? FIELD_WORK_POSTCODE : FIELD_HOME_POSTCODE, Addr[5]);
					if (Addr[6]) c->SetStr(IsWork ? FIELD_WORK_COUNTRY : FIELD_HOME_COUNTRY, Addr[6]);
				}
				else if (IsVar(Field, "note"))
				{
					c->SetStr(FIELD_NOTE, Data);
				}
				else if (IsVar(Field, "uid"))
				{
					GToken n(Data, ";", false);
					c->SetStr(FIELD_UID, n[0]);
				}
				else if (IsVar(Field, "x-perm"))
				{
					int Perms = atoi(Data);
					c->SetInt(FIELD_PERMISSIONS, Perms);
				}
				else if (IsVar(Field, "url"))
				{
					bool IsWork = IsType("work");
					bool IsHome = IsType("home");
					if (IsWork)
					{
						c->SetStr(FIELD_HOME_WEBPAGE, Data);
					}
					else if (IsHome)
					{
						c->SetStr(FIELD_WORK_WEBPAGE, Data);
					}
				}
				else if (IsVar(Field, "nickname"))
				{
					c->SetStr(FIELD_NICK, Data);
				}
				else if (IsVar(Field, "photo"))
				{
					size_t B64Len = strlen(Data);
					ssize_t BinLen = BufferLen_64ToBin(B64Len);
					GAutoPtr<uint8_t> Bin(new uint8_t[BinLen]);
					if (Bin)
					{
						ssize_t Bytes = ConvertBase64ToBinary(Bin.Get(), BinLen, Data, B64Len);
						GVariant v;
						if (v.SetBinary(Bytes, Bin.Release(), true))
						{
							c->SetVar(FIELD_CONTACT_IMAGE, &v);
						}
					}
				}
			}			
		}
	}
	ExitLoop:

	if (Emails.Length())
	{
		if (PrefEmail < 0)
			PrefEmail = 0;

		c->SetStr(FIELD_EMAIL, Emails[PrefEmail]);
		Emails.DeleteAt(PrefEmail);

		if (Emails.Length())
		{
			GStringPipe p;
			for (uint32_t i=0; i<Emails.Length(); i++)
			{
				if (i) p.Print(",%s", Emails[i]);
				else p.Print("%s", Emails[i]);
			}

			char *v = p.NewStr();
			if (v)
			{
				c->SetStr(FIELD_ALT_EMAIL, v);
				DeleteArray(v);
			}
		}
	}

	ClearFields();

	return Status;
}

bool VIo::ReadField(GStreamI &s, GString &Name, ParamArray *Params, GString &Data)
{
	bool Status = false;
	ParamArray LocalParams;

	Name.Empty();
	Data.Empty();

	if (Params) Params->Empty();
	else Params = &LocalParams;

	char Temp[256];
	GArray<char> p;
	bool Done = false;

	while (!Done)
	{
		bool EatNext = false;
		ReadNextLine:
		
		Temp[0] = 0;
		int64 r = d->Buf.Pop(Temp, sizeof(Temp));
		if (r <= 0)
		{
			// Try reading more data...
			r = s.Read(Temp, sizeof(Temp));
			if (r > 0)
				d->Buf.Write(Temp, (int)r);
			else
				break;

			r = d->Buf.Pop(Temp, sizeof(Temp));
		}

		if (r <= 0)
			break;

		// Unfold
		for (char *c = Temp; *c; c++)
		{
			if (*c == '\r')
			{
				// do nothing
			}
			else if (*c == '\n')
			{
				char Next;
				r = d->Buf.Peek((uchar*) &Next, 1);
				if (r == 0)
				{
					r = s.Read(Temp, sizeof(Temp));
					if (r <= 0)
						break;

					d->Buf.Write(Temp, (int)r);
					r = d->Buf.Peek((uchar*) &Next, 1);
				}
				
				if (r == 1)
				{
					if (Next == ' ' || Next == '\t')
					{
						// Wrapped, do nothing
						EatNext = true;
						goto ReadNextLine;
					}
					else
					{
						Done = true;
						break;
					}
				}
				else
				{
					break;
				}
			}
			else if (EatNext)
			{
				EatNext = false;
			}
			else
			{
				p.Add(*c);
			}
		}
	}


	p.Add(0);
	char *f = p.Length() > 1 ? &p[0] : 0;
	if (f)
	{
		char *e = strchr(f, ':');
		if (e)
		{
			*e++ = 0;
			GToken t(f, ";");
			if (t.Length() > 0)
			{
				Name = t[0];
				for (uint32_t i=1; i<t.Length(); i++)
				{
					char *var = t[i];
					char *val = strchr(var, '=');
					if (val)
					{
						*val++ = 0;

						Parameter &p = Params->New();
						p.Field = var;
						p.Value = val;
					}
				}
			}
		}

		bool QuotedPrintable = false;
		const char *Enc = Params->Find("encoding");
		if (Enc)
		{
			if (_stricmp(Enc, "quoted-printable") == 0)
			{
				QuotedPrintable = true;
			}
			else LgiTrace("%s:%i - Unknown encoding '%s'\n", __FILE__, __LINE__, Enc);
		}

		DeEscape(e, QuotedPrintable);

		const char *Charset = Params->Find("charset");
		if (Charset)
		{
			GAutoString u((char*)LgiNewConvertCp("utf-8", e, Charset));
			Data = u.Get();
		}
		else
		{
			Data = e;
		}

		Status = Name.Length() > 0;
	}

	return Status;
}

void VIo::WriteField(GStreamI &s, const char *Name, ParamArray *Params, const char *Data)
{
	if (Name && Data)
	{
		int64 Size = s.GetSize();

		GStreamPrint(&s, "%s", Name);
		if (Params)
		{
			for (uint32_t i=0; i<Params->Length(); i++)
			{
				Parameter &p = (*Params)[i];
				GStreamPrint(&s, "%s%s=%s", i?"":";", p.Field.Get(), p.Value.Get());
			}
		}
		
		bool Is8Bit = false;
		bool HasEq = false;
		for (uint8_t *c = (uint8_t*)Data; *c; c++)
		{
			if ((*c & 0x80) != 0)
			{
				Is8Bit = true;
			}
			else if (*c == '=')
			{
				HasEq = true;
			}
		}
		if (Is8Bit || HasEq)
		{
			if (Is8Bit) s.Write((char*)";charset=utf-8", 14);
			s.Write((char*)";encoding=quoted-printable", 26);
		}

		s.Write((char*)":", 1);
		Fold(s, Data, (int) (s.GetSize() - Size));
		s.Write((char*)"\r\n", 2);
	}	
}

bool VCard::Export(GDataPropI *c, GStreamI *o)
{
	if (!c || !o)
		return false;

	bool Status = true;
	char s[512];
	const char *Empty = "";

	o->Push("begin:vcard\r\n");
	o->Push("version:3.0\r\n");

	const char *First = 0, *Last = 0, *Title = 0;
	First = c->GetStr(FIELD_FIRST_NAME);
	Last = c->GetStr(FIELD_LAST_NAME);
	Title = c->GetStr(FIELD_TITLE);
	if (First || Last)
	{
		sprintf_s(s, sizeof(s),
				"%s;%s;%s",
				Last?Last:Empty,
				First?First:Empty,
				Title?Title:Empty);
		WriteField(*o, "n", 0, s);
	}

	#define OutputTypedField(Field, Name, Type)		\
		{ const char *str = 0;								\
		if ((str = c->GetStr(Name)))					\
		{											\
			WriteField(*o, Field, Type, str);			\
		} }

	#define OutputField(Field, Name)				\
		{ const char *str = 0;								\
		if ((str = c->GetStr(Name)))					\
		{											\
			WriteField(*o, Field, 0, str);				\
		} }

	ParamArray Work("Work"), WorkCell("Work,Cell"), WorkFax("Work,Fax");
	ParamArray Home("Home"), HomeCell("Home,Cell"), HomeFax("Home,Fax");
	ParamArray InetPref("internet,pref"), Inet("internet");
	OutputTypedField("tel", FIELD_WORK_PHONE, &Work);
	OutputTypedField("tel", FIELD_WORK_MOBILE, &WorkCell);
	OutputTypedField("tel", FIELD_WORK_FAX, &WorkFax);
	OutputTypedField("tel", FIELD_HOME_PHONE, &Home);
	OutputTypedField("tel", FIELD_HOME_MOBILE, &HomeCell);
	OutputTypedField("tel", FIELD_HOME_FAX, &HomeFax);
	OutputField("org", FIELD_COMPANY);
	OutputTypedField("email", FIELD_EMAIL, &InetPref);
	const char *Alt;
	if ((Alt = c->GetStr(FIELD_ALT_EMAIL)))
	{
		GToken t(Alt, ",");
		for (unsigned i=0; i<t.Length(); i++)
		{
			WriteField(*o, "email", &Inet, t[i]);
		}
	}

	const char * Uid;
	if ((Uid = c->GetStr(FIELD_UID)))
	{
		GStreamPrint(o, "UID:%s\r\n", Uid);
	}

	const char *Street, *Suburb, *PostCode, *State, *Country;
	Street = Suburb = PostCode = State = Country = 0;
	Street = c->GetStr(FIELD_HOME_STREET);
	Suburb = c->GetStr(FIELD_HOME_SUBURB);
	PostCode = c->GetStr(FIELD_HOME_POSTCODE);
	State = c->GetStr(FIELD_HOME_STATE);
	Country = c->GetStr(FIELD_HOME_COUNTRY);

	if (Street || Suburb || PostCode || State || Country)
	{
		sprintf_s(s, sizeof(s),
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		
		WriteField(*o, "adr", &Home, s);
	}

	Street = Suburb = PostCode = State = Country = 0;
	Street = c->GetStr(FIELD_WORK_STREET);
	Suburb = c->GetStr(FIELD_WORK_SUBURB);
	PostCode = c->GetStr(FIELD_WORK_POSTCODE);
	State = c->GetStr(FIELD_WORK_STATE);
	Country = c->GetStr(FIELD_WORK_COUNTRY);
	if (Street || Suburb || PostCode || State || Country)
	{
		sprintf_s(s, sizeof(s),
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		
		WriteField(*o, "adr", &Work, s);
	}

	// OutputField("X-Perm", FIELD_PERMISSIONS);

	const char *Url;
	if ((Url = c->GetStr(FIELD_HOME_WEBPAGE)))
	{
		WriteField(*o, "url", &Home, Url);
	}
	if ((Url = c->GetStr(FIELD_WORK_WEBPAGE)))
	{
		WriteField(*o, "url", &Work, Url);
	}
	const char *Nick;
	if ((Nick = c->GetStr(FIELD_NICK)))
	{
		WriteField(*o, "nickname", 0, Nick);
	}
	const char *Note;
	if ((Note = c->GetStr(FIELD_NOTE)))
	{
		WriteField(*o, "note", 0, Note);
	}
	
	const GVariant *Photo = c->GetVar(FIELD_CONTACT_IMAGE);
	if (Photo && Photo->Type == GV_BINARY)
	{
		ssize_t B64Len = BufferLen_BinTo64(Photo->Value.Binary.Length);
		GAutoPtr<char> B64Buf(new char[B64Len]);
		if (B64Buf)
		{
			ssize_t Bytes = ConvertBinaryToBase64(B64Buf, B64Len, (uchar*)Photo->Value.Binary.Data, Photo->Value.Binary.Length);
			if (Bytes > 0)
			{
				GStreamPrint(o, "photo;type=jpeg;encoding=base64:\r\n");
				
				int LineChar = 76;
				for (int i=0; i<Bytes; )
				{
					ssize_t Remain = Bytes - i;
					ssize_t Wr = Remain > LineChar ? LineChar : Remain;

					o->Write(" ", 1);
					o->Write(B64Buf + i, Wr);
					o->Write("\r\n", 2);

					i += Wr;
				}
				o->Write("\r\n", 2);
			}
		}
	}

	o->Push("end:vcard\r\n");

	return Status;
}

/////////////////////////////////////////////////////////////
// VCal class
int StringToWeekDay(const char *s)
{
	const char *days[] = {"SU","MO","TU","WE","TH","FR","SA"};
	for (unsigned i=0; i<CountOf(days); i++)
	{
		if (!_strnicmp(days[i], s, 2))
			return i;
	}
	return -1;
}

bool EvalRule(LDateTime &out, VIo::TimeZoneSection &tz, int yr)
{
	GString::Array p = tz.Rule.SplitDelimit(";");
	VIo::ParamArray Params;
	for (unsigned i=0; i<p.Length(); i++)
	{
		GString::Array v = p[i].SplitDelimit("=", 1);
		if (v.Length() == 2)
			Params.New().Set(v[0], v[1]);
	}
	
	const char *ByDay = Params.Find("byday");
	const char *ByMonth = Params.Find("bymonth");
	if (!ByDay || !ByMonth)
	{
		LgiTrace("%s:%i - Missing byday/bymonth\n", _FL);
		return false;
	}

	out.Year(yr);

//	RRULE:FREQ=YEARLY;INTERVAL=1;BYDAY=2SU;BYMONTH=3
	if (!IsDigit(*ByMonth))
	{
		LgiTrace("%s:%i - Unexpected format for ByMonth: %s\n", _FL, ByMonth);
		return false;
	}

	out.Month(atoi(ByMonth));
	
	if (!IsDigit(*ByDay))
	{
		LgiTrace("%s:%i - Unexpected format for ByDay: %s\n", _FL, ByDay);
		return false;
	}
	
	int Idx = atoi(ByDay);
	while (*ByDay && IsDigit(*ByDay))
		ByDay++;
		
	int WeekDay = StringToWeekDay(ByDay);
	if (WeekDay < 0)
	{
		LgiTrace("%s:%i - No week day in ByDay: %s\n", _FL, ByDay);
		return false;
	}
	
	int Pos = 1;
	for (int day = 1; day <= out.DaysInMonth(); day++)
	{
		out.Day(day);
		if (out.DayOfWeek() == WeekDay)
		{
			if (Idx == Pos)
				break;
			Pos++;
		}
	}
	
	return true;
}

struct LAlarm
{
	/* e.g.	BEGIN:VALARM
			ACTION:EMAIL
			DESCRIPTION:REMINDER
			TRIGGER:-PT10M
			X-PARAM:SomeParam
			END:VALARM */

	GString Action, Desc, Trigger, Param;

	GString GetStr()
	{
		// Fields are: Number, CalendarReminderUnits, CalendarReminderType, Param
		CalendarReminderType Type = CalMaxType;
		if (Action.Equals("DISPLAY")) Type = CalPopup;
		else if (Action.Equals("EMAIL")) Type = CalEmail;
		else if (Action.Equals("X-SCRIPT")) Type = CalScriptCallback;
		else return NULL;

		CalendarReminderUnits Units = CalMaxUnit;
		char Last = Trigger.Strip()(-1);
		if (ToUpper(Last) == 'M') Units = CalMinutes;
		else if (ToUpper(Last) == 'H') Units = CalHours;
		else if (ToUpper(Last) == 'D') Units = CalDays;

		char Buf[64];
		int ch = 0;
		for (const char *c = Trigger; *c; c++)
		{
			if (IsDigit(*c) || *c == '-')
				Buf[ch++] = *c;
		}
		Buf[ch] = 0;

		if (!Trigger)
			return NULL;

		GString s;
		s.Printf(LPrintfInt64 ",%i,%i,%s", -Atoi(Buf), (int)Units, (int)Type, Param?Param.Get():"");
		return s;
	}
};

bool VCal::Import(GDataPropI *c, GStreamI *In)
{
	bool Status = false;

	if (!c || !In)
		return false;

	GString Field, Data;
	ParamArray Params;

	bool IsEvent = false;
	bool IsTimeZone = false;
	bool IsAlarm = false;
	GString SectionType;
	LDateTime EventStart, EventEnd;
	
	GString StartTz, EndTz;
	GArray<TimeZoneInfo> TzInfos;
	TimeZoneInfo *TzInfo = NULL;
	bool IsNormalTz = false, IsDaylightTz = false;
	LJson To;
	int Attendee = 0;
	GArray<LAlarm> Alarms;
	int AlarmIdx = -1;

	while (ReadField(*In, Field, &Params, Data))
	{
		if (!_stricmp(Field, "begin"))
		{
			if (_stricmp(Data, "vevent") == 0
				||
				_stricmp(Data, "vtodo") == 0)
			{
				IsEvent = true;
				SectionType = Data;

				int Type = _stricmp(Data, "vtodo") == 0 ? 1 : 0;
				c->SetInt(FIELD_CAL_TYPE, Type);
			}
			else if (!_stricmp(Data, "vtimezone"))
			{
				IsTimeZone = true;
				TzInfo = &TzInfos.New();
			}
			else if (_stricmp(Data, "vcalendar") == 0)
				IsCal = true;
			else if (!_stricmp(Data, "standard"))
				IsNormalTz = true;
			else if (!_stricmp(Data, "daylight"))
				IsDaylightTz = true;
			else if (!_stricmp(Data, "valarm"))
			{
				IsAlarm = true;
				AlarmIdx++;
			}
		}
		else if (_stricmp(Field, "end") == 0)
		{
			if (_stricmp(Data, "vcalendar") == 0)
			{
				IsCal = false;
			}
			else if (SectionType && _stricmp(Data, SectionType) == 0)
			{
				Status = true;
				IsEvent = false;
				break; // exit loop
			}
			else if (!_stricmp(Data, "vtimezone"))
			{
				IsTimeZone = false;
				TzInfo = NULL;
			}
			else if (!_stricmp(Data, "standard"))
				IsNormalTz = false;
			else if (!_stricmp(Data, "daylight"))
				IsDaylightTz = false;
			else if (!_stricmp(Data, "valarm"))
				IsAlarm = false;
		}
		else if (IsEvent)
		{
			if (IsAlarm)
			{
				auto &a = Alarms[AlarmIdx];
				if (IsVar(Field, "ACTION"))
					a.Action = Data;
				else if (IsVar(Field, "DESCRIPTION"))
					a.Desc = Data;
				else if (IsVar(Field, "TRIGGER"))
					a.Trigger = Data;
				else if (IsVar(Field, "X-PARAM"))
					a.Param = Data;
			}
			else if (IsVar(Field, "dtstart"))
			{
				ParseDate(EventStart, Data);
				StartTz = Params.Find("TZID");
			}
			else if (IsVar(Field, "dtend"))
			{
				ParseDate(EventEnd, Data);
				EndTz = Params.Find("TZID");
			}
			else if (IsVar(Field, "summary"))
			{
				c->SetStr(FIELD_CAL_SUBJECT, Data);
			}
			else if (IsVar(Field, "description"))
			{
				GAutoString Sum(UnMultiLine(Data));
				if (Sum)
					c->SetStr(FIELD_CAL_NOTES, Sum);
			}
			else if (IsVar(Field, "location"))
			{
				c->SetStr(FIELD_CAL_LOCATION, Data);
			}
			else if (IsVar(Field, "uid"))
			{
				char *Uid = Data;
				c->SetStr(FIELD_UID, Uid);
			}
			else if (IsVar(Field, "x-showas"))
			{
				char *n = Data;
				if (Stricmp(n, "TENTATIVE") == 0)
					c->SetInt(FIELD_CAL_SHOW_TIME_AS, CalTentative);
				else if (Stricmp(n, "BUSY") == 0)
					c->SetInt(FIELD_CAL_SHOW_TIME_AS, CalBusy);
				else if (Stricmp(n, "OUT") == 0)
					c->SetInt(FIELD_CAL_SHOW_TIME_AS, CalOut);
				else
					c->SetInt(FIELD_CAL_SHOW_TIME_AS, CalFree);
			}
			else if (IsVar(Field, "attendee"))
			{
				auto Email = Data.SplitDelimit(":=").Last();
				if (LIsValidEmail(Email))
				{
					const char *Name = Params.Find("CN");
					const char *Role = Params.Find("Role");
					
					GString k;
					k.Printf("[%i].email", Attendee);
					To.Set(k, Email);

					if (Name)
					{
						k.Printf("[%i].name", Attendee);
						To.Set(k, Name);
					}

					if (Role)
					{
						k.Printf("[%i].role", Attendee);
						To.Set(k, Role);
					}

					Attendee++;
				}
			}
			else if (IsVar(Field, "status"))
			{
				c->SetStr(FIELD_CAL_STATUS, Data);
			}
		}
		else if (IsTimeZone && TzInfo)
		{
			/* e.g.:
			
			TZID:Pacific Standard Time

			BEGIN:STANDARD
				DTSTART:16010101T020000
				TZOFFSETFROM:-0700
				TZOFFSETTO:-0800
				RRULE:FREQ=YEARLY;INTERVAL=1;BYDAY=1SU;BYMONTH=11
			END:STANDARD

			BEGIN:DAYLIGHT
				DTSTART:16010101T020000
				TZOFFSETFROM:-0800
				TZOFFSETTO:-0700
				RRULE:FREQ=YEARLY;INTERVAL=1;BYDAY=2SU;BYMONTH=3
			END:DAYLIGHT
			
			*/
			
			if (IsVar(Field, "TZID"))
			{
				TzInfo->Name = Data;
			}
			else if (IsNormalTz || IsDaylightTz)
			{
				TimeZoneSection &Sect = IsNormalTz ? TzInfo->Normal : TzInfo->Daylight;
				if (IsVar(Field, "DTSTART"))
					ParseDate(Sect.Start, Data);
				else if (IsVar(Field, "TZOFFSETFROM"))
					Sect.From = (int)Data.Int();
				else if (IsVar(Field, "TZOFFSETTO"))
					Sect.To = (int)Data.Int();
				else if (IsVar(Field, "RRULE"))
					Sect.Rule = Data;				
			}			
		}
	}

	if (Attendee > 0)
	{
		auto j = To.GetJson();
		c->SetStr(FIELD_ATTENDEE_JSON, j);
	}
	
	if (StartTz || EndTz)
	{
		// Did we get a timezone defn?
		TimeZoneInfo *Match = NULL;
		for (unsigned i=0; i<TzInfos.Length(); i++)
		{
			if (TzInfos[i].Name.Equals(StartTz))
			{
				Match = &TzInfos[i];
				break;
			}
		}

		// Set the TZ
		int EffectiveTz = 0;
		if (Match)
		{
			LDateTime Norm, Dst;
			if (EvalRule(Norm, Match->Normal, EventStart.Year()) &&
				EvalRule(Dst, Match->Daylight, EventStart.Year()))
			{
				bool IsDst = false;
				if (Dst < Norm)
				{
					// DST in the middle of the year
					// |Jan----DST------Norm----Dec|
					if (EventStart >= Dst && EventStart <= Norm)
						IsDst = true;
				}
				else
				{
					// DST over the start and end of the year
					// |Jan----Norm------DST----Dec|
					if (EventStart >= Norm && EventStart <= Dst)
						IsDst = false;
					else
						IsDst = true;
				}
				
				#ifdef _DEBUG
				LgiTrace("Eval Start=%s, Norm=%s, Dst=%s, IsDst=%i\n", EventStart.Get().Get(), Norm.Get().Get(), Dst.Get().Get(), IsDst);
				#endif
				
				EffectiveTz = IsDst ? Match->Daylight.To : Match->Normal.To;
				GString sTz;
				sTz.Printf("%4.4i,%s", EffectiveTz, StartTz.Get());
				c->SetStr(FIELD_CAL_TIMEZONE, sTz);
			}
			else goto StoreStringTz;
		}
		else
		{
			// Store whatever they gave us
			StoreStringTz:
			if (StartTz.Equals(EndTz))
				c->SetStr(FIELD_CAL_TIMEZONE, StartTz);
			else if (StartTz.Get() && EndTz.Get())
			{
				GString s;
				s.Printf("%s,%s", StartTz.Get(), EndTz.Get());
				c->SetStr(FIELD_CAL_TIMEZONE, s);
			}
			else if (StartTz)
				c->SetStr(FIELD_CAL_TIMEZONE, StartTz);
			else if (EndTz)
				c->SetStr(FIELD_CAL_TIMEZONE, EndTz);
			
			if (StartTz)
				EffectiveTz = TimezoneToOffset(StartTz);
		}
		
		if (EffectiveTz)
		{
			// Convert the event to UTC
			int e = abs(EffectiveTz);
			int Mins = (((e / 100) * 60) + (e % 100)) * (EffectiveTz < 0 ? -1 : 1);
			#ifdef _DEBUG
			LgiTrace("%s:%i - EffectiveTz=%i, Mins=%i\n", _FL, EffectiveTz, Mins);
			#endif
			if (EventStart.IsValid())
			{
				#ifdef _DEBUG
				LgiTrace("EventStart=%s\n", EventStart.Get().Get());
				#endif

				EventStart.AddMinutes(-Mins);
				#ifdef _DEBUG
				LgiTrace("EventStart=%s\n", EventStart.Get().Get());
				#endif
			}
			if (EventEnd.IsValid())
				EventEnd.AddMinutes(-Mins);
		}
	}
	
	if (EventStart.IsValid())
		c->SetDate(FIELD_CAL_START_UTC, &EventStart);
	if (EventEnd.IsValid())
		c->SetDate(FIELD_CAL_END_UTC, &EventEnd);

	if (Alarms.Length())
	{
		GString::Array s;
		for (LAlarm &a: Alarms)
		{
			GString r = a.GetStr();
			if (r) s.Add(r);
		}
		if (s.Length())
		{
			c->SetStr(FIELD_CAL_REMINDERS, GString("\n").Join(s));
		}
	}

	ClearFields();

	return Status;
}

GString ToString(LDateTime &dt)
{
	GString s;
	s.Printf("%04.4i%02.2i%02.2iT%02.2i%02.2i%02.2i%s",
		dt.Year(), dt.Month(), dt.Day(), dt.Hours(), dt.Minutes(), dt.Seconds(),
		dt.GetTimeZone() ? "" : "Z");
	return s;
}

bool VCal::Export(GDataPropI *c, GStreamI *o)
{
	if (!c || !o)
		return false;

	int64 Type = c->GetInt(FIELD_CAL_TYPE);
	// auto *TimeZone = c->GetStr(FIELD_CAL_TIMEZONE);
	char *TypeStr = Type == 1 ? (char*)"VTODO" : (char*)"VEVENT";

	o->Push((char*)"BEGIN:VCALENDAR\r\n"
			"VERSION:2.0\r\n"
			"CALSCALE:GREGORIAN\r\n"
			"PRODID:Memecode vcal filter\r\n");

	if (c)
	{
		GStreamPrint(o, "BEGIN:%s\r\n", TypeStr);

		#define OutputStr(Field, Name)						\
		{													\
			const char *_s = 0;								\
			if ((_s = c->GetStr(Field)))					\
			{												\
				WriteField(*o, Name, 0, _s);				\
			}												\
		}

		LDateTime Now;
		Now.SetNow();
		Now.ToUtc();
		GStreamPrint(o, "DTSTAMP:%s\r\n", ToString(Now).Get());

		OutputStr(FIELD_CAL_SUBJECT, "SUMMARY");
		OutputStr(FIELD_CAL_LOCATION, "LOCATION");
		OutputStr(FIELD_CAL_NOTES, "DESCRIPTION");
		OutputStr(FIELD_CAL_STATUS, "STATUS");

		int64 ShowAs;
		if ((ShowAs = c->GetInt(FIELD_CAL_SHOW_TIME_AS)))
		{
			switch (ShowAs)
			{
				default:
				case CalFree:
					o->Push((char*)"X-SHOWAS:FREE\r\n");
					break;
				case CalTentative:
					o->Push((char*)"X-SHOWAS:TENTATIVE\r\n");
					break;
				case CalBusy:
					o->Push((char*)"X-SHOWAS:BUSY\r\n");
					break;
				case CalOut:
					o->Push((char*)"X-SHOWAS:OUT\r\n");
					break;
			}
		}

		const char *Uid;
		if ((Uid = c->GetStr(FIELD_UID)))
		{
			GStreamPrint(o, "UID:%s\r\n", Uid);
		}

		const LDateTime *Dt;
		if ((Dt = c->GetDate(FIELD_CAL_START_UTC)))
		{
			LDateTime dt = *Dt;
			dt.ToUtc();
			GStreamPrint(o, "DTSTART:%s\r\n", ToString(dt).Get());
		}
		if ((Dt = c->GetDate(FIELD_CAL_END_UTC)))
		{
			LDateTime dt = *Dt;
			dt.ToUtc();
			GStreamPrint(o, "DTEND:%s\r\n", ToString(dt).Get());
		}

		const char *Reminders;
		if ((Reminders = c->GetStr(FIELD_CAL_REMINDERS)))
		{
			auto Lines = GString(Reminders).SplitDelimit("\n");
			for (auto Ln: Lines)
			{
				// Fields are: Number, CalendarReminderUnits, CalendarReminderType, Param
				auto p = Ln.SplitDelimit(",");
				if (p.Length() >= 3)
				{
					const char *Duration = NULL;
					int64 Count = p[0].Int();
					switch (p[1].Int())
					{
						case CalMinutes: Duration = "M"; break;
						case CalHours: Duration = "H"; break;
						case CalDays: Duration = "D"; break;
						case CalWeeks: Duration = "D"; Count *= 7; break;
					}

					const char *Action = NULL;
					switch (p[2].Int())
					{
						case CalEmail: Action = "EMAIL"; break;
						case CalPopup: Action = "DISPLAY"; break;
						case CalScriptCallback: Action = "X-SCRIPT"; break;
					}

					if (Action && Duration)
					{
						GString s;
						s.Printf(	"BEGIN:VALARM\r\n"
									"ACTION:%s\r\n"
									"DESCRIPTION:REMINDER\r\n"
									"TRIGGER:-PT" LPrintfInt64 "%s\r\n",
									Action,
									Count, Duration
								);
						if (p.Length() > 3)
							s += GString("X-PARAM: ") + p[3];
						s += "END:VALARM\r\n";
						o->Write(s.Get(), s.Length());
					}
					else LgiAssert(0);
				}
			}
		}

		LJson j(c->GetStr(FIELD_ATTENDEE_JSON));
		for (auto g: j.GetArray(NULL))
		{
			// e.g.: ATTENDEE;CN="Matthew Allen";ROLE=REQ-PARTICIPANT;RSVP=TRUE:MAILTO:matthew@company.com.au
			auto Email = g.Get("email");
			if (Email)
			{
				auto Name = g.Get("name");
				auto Role = g.Get("role");

				char s[400];
				int ch = sprintf_s(s, sizeof(s), "ATTENDEE");
				if (Name)
					ch += sprintf_s(s+ch, sizeof(s)-ch, ";CN=\"%s\"", Name.Get());
				if (Role)
					ch += sprintf_s(s+ch, sizeof(s)-ch, ";ROLE=\"%s\"", Role.Get());
				ch += sprintf_s(s+ch, sizeof(s)-ch, ":MAILTO=%s\r\n", Email.Get());
				o->Write(s, ch);
			}				
		}

		GStreamPrint(o, "END:%s\r\n", TypeStr);
	}

	o->Push((char*)"END:VCALENDAR\r\n");

	return true;
}
