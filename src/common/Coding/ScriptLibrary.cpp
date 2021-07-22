#include <stdio.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Scripting.h"
#include "lgi/common/Token.h"
#include "lgi/common/SubProcess.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/FileSelect.h"
#include "ScriptingPriv.h"

//////////////////////////////////////////////////////////////////////////////////////
char16 sChar[]		= L"char";
char16 sInt[]		= { 'i','n','t', 0 };
char16 sUInt[]		= { 'u','i','n','t', 0 };
char16 sInt32[]		= { 'i','n','t','3','2', 0 };
char16 sUInt32[]	= { 'u','i','n','t','3','2', 0 };
char16 sInt64[]		= { 'i','n','t','6','4', 0 };
char16 sHWND[]		= { 'H','W','N','D', 0 };
char16 sDWORD[]		= { 'D','W','O','R','D', 0 };
char16 sLPTSTR[]	= { 's','L','P','T','S','T','R', 0 };
char16 sLPCTSTR[]	= { 's','L','P','C','T','S','T','R', 0 };
char16 sElse[]		= { 'e','l','s','e', 0 };
char16 sIf[]		= { 'i','f',0 };
char16 sFunction[]	= { 'f','u','n','c','t','i','o','n',0 };
char16 sExtern[]	= { 'e','x','t','e','r','n',0 };
char16 sFor[]		= { 'f','o','r',0 };
char16 sWhile[]		= { 'w','h','i','l','e',0 };
char16 sReturn[]	= { 'r','e','t','u','r','n',0 };
char16 sInclude[]	= { 'i','n','c','l','u','d','e',0 };
char16 sDefine[]	= { 'd','e','f','i','n','e',0 };
char16 sStruct[]	= { 's','t','r','u','c','t',0 };
char16 sTrue[]		= { 't','r','u','e',0 };
char16 sFalse[]		= { 'f','a','l','s','e',0 };
char16 sNull[]		= { 'n','u','l','l',0 };
char16 sOutParam[]  = { '_','o','u','t','_',0};

char16 sHash[]				= { '#', 0 };
char16 sPeriod[]			= { '.', 0 };
char16 sComma[]				= { ',', 0 };
char16 sSemiColon[]			= { ';', 0 };
char16 sStartRdBracket[]	= { '(', 0 };
char16 sEndRdBracket[]		= { ')', 0 };
char16 sStartSqBracket[]	= { '[', 0 };
char16 sEndSqBracket[]		= { ']', 0 };
char16 sStartCurlyBracket[]	= { '{', 0 };
char16 sEndCurlyBracket[]	= { '}', 0 };

//////////////////////////////////////////////////////////////////////////////////////
LExecutionStatus GHostFunc::Call(LScriptContext *Ctx, LScriptArguments &Args)
{
	return (Ctx->*(Func))(Args) ? ScriptSuccess : ScriptError;
}

const char *InstToString(GInstruction i)
{
	#undef _i
	#define _i(name, opcode, desc) \
		case name: return desc;

	switch (i)
	{
		AllInstructions
	}

	return "#err";
}

LStream LScriptArguments::NullConsole;

//////////////////////////////////////////////////////////////////////////////////////
int LScriptUtils::atoi(char16 *s)
{
	int i = 0;
	if (s)
	{
		char b[64];
		ssize_t Len = StrlenW(s) * sizeof(*s);
		ssize_t Bytes = LBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::atoi(b);
	}
	return i;
}

int64 LScriptUtils::atoi64(char16 *s)
{
	int64 i = 0;
	if (s)
	{
		#ifdef _MSC_VER

		i = _wtoi64(s);

		#else

		char b[64];
		ssize_t Len = StrlenW(s) * sizeof(*s);
		ssize_t Bytes = LBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;

		i = strtoll(b, 0, 10);
		
		#endif
	}
	return i;
}

double LScriptUtils::atof(char16 *s)
{
	double i = 0;
	if (s)
	{
		char b[64];
		ssize_t Len = StrlenW(s) * sizeof(*s);
		ssize_t Bytes = LBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::atof(b);
	}
	return i;
}

int LScriptUtils::htoi(char16 *s)
{
	int i = 0;
	if (s)
	{
		char b[64];
		ssize_t Len = StrlenW(s) * sizeof(*s);
		ssize_t Bytes = LBufConvertCp(b, "utf-8", sizeof(b), (const void*&)s, LGI_WideCharset, Len);
		b[Bytes/sizeof(*b)] = 0;
		i = ::htoi(b);
	}
	return i;
}


//////////////////////////////////////////////////////////////////////////////////////
SystemFunctions::SystemFunctions()
{
	Engine = NULL;
	Log = NULL;

	#ifdef WINNATIVE
	Brk = NULL;
	#endif
}

SystemFunctions::~SystemFunctions()
{
}

LStream *SystemFunctions::GetLog()
{
	return Log;
}

bool SystemFunctions::SetLog(LStream *log)
{
	LAssert(Log == NULL);
	Log = log;
	return true;
}

void SystemFunctions::SetEngine(LScriptEngine *Eng)
{
	Engine = Eng;
}

bool SystemFunctions::Assert(LScriptArguments &Args)
{
	*Args.GetReturn() = true;
	if (Args.Length() == 0)
		return true;

	auto v = Args[0]->CastInt32();
	if (!v)
	{
		const char *Msg = Args.Length() > 1 ? Args[1]->CastString() : NULL;
		*Args.GetReturn() = false;
		Args.Throw(NULL, -1, Msg);
	}

	return true;
}

bool SystemFunctions::DebuggerEnabled(LScriptArguments &Args)
{
	if (Args.Length() == 0)
		return false;
	Args.GetVm()->SetDebuggerEnabled(Args[0]->CastInt32() != 0);
	return true;
}

bool SystemFunctions::Throw(LScriptArguments &Args)
{
	const char *Msg = Args.Length() > 0 ? Args[0]->CastString() : NULL;
	Args.Throw(NULL, -1, Msg);
	return true;
}

bool SystemFunctions::LoadString(LScriptArguments &Args)
{
	if (Args.Length() != 1)
		return false;

	*Args.GetReturn() = LLoadString(Args[0]->CastInt32());
	return true;	
}

bool SystemFunctions::Sprintf(LScriptArguments &Args)
{
	if (Args.Length() < 1)
		return false;
	char *Fmt = Args[0]->Str();
	if (!Fmt)
		return false;

	#if defined(LINUX) || defined(MAC)
	
	// No support for sprintf with generated args... hack a string up
	// Formatting widths etc not supported.
	LArray<char> s;
	int i = 1;
	for (char *f = Fmt; *f; f++)
	{
		if (f[0] == '%' && f[1] != '%')
		{
			f++; // Skip '%'
			
			// char *Fmt = f;
			while (*f && !IsAlpha(*f))
				f++; // Skip formatting..
			
			if (i >= Args.Length())
				break; // No more arguments...
				
			switch (*f)
			{
				case 's':
				{
					// String...
					char *v = Args[i]->CastString();
					if (v)
						s.Add(v, strlen(v));
					else
						s.Add((char*)"(null)", 4);
					break;
				}
				case 'c':
				{
					char *Str = Args[i]->Str();
					s.Add(Str ? *Str : '?');
					break;
				}
				case 'f':
				case 'g':
				{
					break;
				}
				case 'u':
				case 'd':
				case 'i':
				{
					// Int...
					LString v;
					v.Printf("%i", Args[i]->CastInt32());
					s.Add(v.Get(), v.Length());
					break;
				}
			}
			f++;
			i++;
		}
		else s.Add(*f);
	}
	
	s.Add(0); // NULL terminate
	*Args.GetReturn() = s.AddressOf();
	
	#else
	
	LArray<UNativeInt> Params;
	va_list a;

	unsigned i = 1;
	for (char *f = Fmt; *f; f++)
	{
		if (f[0] == '%' && f[1] != '%')
		{
			char *t = f + 1;
			while (*t && !IsAlpha(*t))
				t++;

			if (i >= Args.Length())
			{
				LAssert(!"Not enough args.");
				break;
			}

			switch (*t)
			{
				case 's':
				{
					Params.Add((UNativeInt)Args[i++]->Str());
					break;
				}
				case 'c':
				{
					char *Str = Args[i++]->Str();
					Params.Add(Str ? *Str : '?');
					break;
				}
				case 'f':
				case 'g':
				{
					union tmp
					{
						double Dbl;
						struct
						{
							uint32_t High;
							uint32_t Low;
						};
					} Tmp;
					Tmp.Dbl = Args[i++]->CastDouble();

					Params.Add(Tmp.High);
					Params.Add(Tmp.Low);
					break;
				}
				default:
				{
					Params.Add(Args[i++]->CastInt32());
					break;
				}
			}
			
			f = *t ? t + 1 : t;
		}
	}

	a = (va_list) &Params[0];

	#ifndef WIN32
	#define _vsnprintf vsnprintf
	#endif

	char Buf[1024];
	vsprintf_s(Buf, sizeof(Buf), Fmt, a);
	*Args.GetReturn() = Buf;
	
	#endif

	return true;
}

bool SystemFunctions::LReadTextFile(LScriptArguments &Args)
{
	if (Args.Length() == 1 &&
		LFileExists(Args[0]->CastString()))
	{
		if (Args.GetReturn()->OwnStr(::LReadTextFile(Args[0]->CastString())))
			return true;
	}

	return false;
}

bool SystemFunctions::WriteTextFile(LScriptArguments &Args)
{
	if (Args.Length() == 2)
	{
		LFile f;
		if (f.Open(Args[0]->CastString(), O_WRITE))
		{
			f.SetSize(0);
			
			LVariant *v = Args[1];
			if (v)
			{
				switch (v->Type)
				{
					default: break;
					case GV_STRING:
					{
						size_t Len = strlen(v->Value.String);
						*Args.GetReturn() = f.Write(v->Value.String, Len) == Len;
						return true;
						break;
					}
					case GV_BINARY:
					{
						*Args.GetReturn() = f.Write(v->Value.Binary.Data, v->Value.Binary.Length) == v->Value.Binary.Length;
						return true;
						break;
					}
				}
			}
		}
	}

	return false;
}

LView *SystemFunctions::CastGView(LVariant &v)
{
	switch (v.Type)
	{
		default: break;
		case GV_DOM:
			return dynamic_cast<LView*>(v.Value.Dom);
		case GV_GVIEW:
			return v.Value.View;
	}
	return 0;
}

bool SystemFunctions::SelectFiles(LScriptArguments &Args)
{
	LFileSelect s;
	
	if (Args.Length() > 0)
		s.Parent(CastGView(*Args[0]));
	
	GToken t(Args.Length() > 1 ? Args[1]->CastString() : 0, ",;:");
	for (unsigned i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		char *sp = strrchr(c, ' ');
		if (sp)
		{
			*sp++ = 0;
			s.Type(sp, c);
		}
		else
		{
			char *dot = strrchr(c, '.');
			if (dot)
			{
				char Type[256];
				sprintf_s(Type, sizeof(Type), "%s files", dot + 1);
				s.Type(Type, c);
			}
		}
	}
	s.Type("All Files", LGI_ALL_FILES);

	s.InitialDir(Args.Length() > 2 ? Args[2]->CastString() : 0);
	s.MultiSelect(Args.Length() > 3 ? Args[3]->CastInt32() != 0 : true);
	bool SaveAs = Args.Length() > 4 ? Args[4]->CastInt32() != 0 : false;

	if (SaveAs ? s.Save() : s.Open())
	{
		Args.GetReturn()->SetList();
		for (unsigned i=0; i<s.Length(); i++)
		{
			Args.GetReturn()->Value.Lst->Insert(new LVariant(s[i]));
		}
	}

	return true;
}

bool SystemFunctions::SelectFolder(LScriptArguments &Args)
{
	LFileSelect s;
	
	if (Args.Length() > 0)
		s.Parent(CastGView(*Args[0]));
	s.InitialDir(Args.Length() > 1 ? Args[1]->CastString() : 0);

	if (s.OpenFolder())
		*Args.GetReturn() = s.Name();
	else
		Args.GetReturn()->Empty();

	return true;
}

bool SystemFunctions::Sleep(LScriptArguments &Args)
{
	if (Args.Length() != 1)
		return false;

	LSleep(Args[0]->CastInt32());
	return true;
}

bool SystemFunctions::ToString(LScriptArguments &Args)
{
	LStringPipe p;
	const char *Sep = ", ";

	for (unsigned i=0; i<Args.Length(); i++)
	{
		LString s = Args[i]->ToString();
		p.Print("%s%s", i?Sep:"", s.Get());
	}

	Args.GetReturn()->OwnStr(p.NewStr());
	return true;
}

bool SystemFunctions::Print(LScriptArguments &Args)
{
	LStream *Out = Log ? Log : (Engine ? Engine->GetConsole() : NULL);

	for (unsigned n=0; Out && n<Args.Length(); n++)
	{
		if (!Args[n])
			continue;

		LVariant v = *Args[n];
		char *f = v.CastString();
		if (!f)
		{
			Out->Write("NULL", 4);
			continue;
		}

		#if 1
		size_t Len = strlen(f);
		Out->Write(f, Len);
		#else
		char *i = f, *o = f;
		for (; *i; i++)
		{
			if (*i == '\\')
			{
				i++;
				switch (*i)
				{
					case 'n':
						*o++ = '\n';
						break;
					case 'r':
						*o++ = '\r';
						break;
					case 't':
						*o++ = '\t';
						break;
					case '\\':
						*o++ = '\\';
						break;
					case '0':
						*o++ = 0;
						break;
				}
			}
			else
			{
				*o++ = *i;
			}
		}
		*o = 0;
		Out->Write(f, o - f);
		#endif
	}

	return true;
}

bool SystemFunctions::FormatSize(LScriptArguments &Args)
{
	if (Args.Length() != 1)
		return false;

	char s[64];
	LFormatSize(s, sizeof(s), Args[0]->CastInt64());
	*Args.GetReturn() = s;
	return true;
}

bool SystemFunctions::ClockTick(LScriptArguments &Args)
{
	*Args.GetReturn() = (int64)LCurrentTime();
	return true;
}

bool SystemFunctions::Now(LScriptArguments &Args)
{
	Args.GetReturn()->Empty();
	Args.GetReturn()->Type = GV_DATETIME;
	Args.GetReturn()->Value.Date = new LDateTime;
	Args.GetReturn()->Value.Date->SetNow();
	return true;
}

bool SystemFunctions::New(LScriptArguments &Args)
{
	if (Args.Length() < 1 || !Args[0])
		return false;

	Args.GetReturn()->Empty();
	char *sType = Args[0]->CastString();
	if (!sType)
		return false;

	if (IsDigit(*sType))
	{
		// Binary block
		int Bytes = ::atoi(sType);
		if (!Bytes)
			return false;
		
		return Args.GetReturn()->SetBinary(Bytes, new char[Bytes], true);
	}

	LVariant *Ret = Args.GetReturn();
	GDomProperty Type = LStringToDomProp(sType);
	switch (Type)
	{	
		case TypeList:
		{
			Ret->SetList();
			break;
		}
		case TypeHashTable:
		{
			Ret->SetHashTable();
			break;
		}
		case TypeSurface:
		{
			Ret->Empty();
			Ret->Type = GV_LSURFACE;
			if ((Ret->Value.Surface.Ptr = new LMemDC))
			{
				Ret->Value.Surface.Ptr->AddRef();
				Ret->Value.Surface.Own = true;
			}
			break;
		}
		case TypeFile:
		{
			Ret->Empty();
			
			#if 1
			Ret->Type = GV_STREAM;
			Ret->Value.Stream.Ptr = new LFile;
			if (Ret->Value.Stream.Ptr)
				Ret->Value.Stream.Own = true;
			#else
			Ret->Type = GV_GFILE;
			if ((Ret->Value.File.Ptr = new LFile))
			{
				Ret->Value.File.Ptr->AddRef();
				Ret->Value.File.Own = true;
			}
			#endif
			
			break;
		}
		case TypeDateTime:
		{
			Ret->Empty();
			Ret->Type = GV_DATETIME;
			Ret->Value.Date = new LDateTime;
			break;
		}
		default:
		{
			Ret->Empty();

			LCompiledCode *c = Engine ? Engine->GetCurrentCode() : NULL;
			if (!c)
				return false;

			LAutoWString o(Utf8ToWide(sType));
			LCustomType *t = c->GetType(o);
			if (t)
			{
				int ArrayLength = Args.Length() > 1 ? Args[1]->CastInt32() : 1;
				if (ArrayLength > 0)
				{
					Ret->Type = GV_CUSTOM;
					Ret->Value.Custom.Dom = t;
					Ret->Value.Custom.Data = new uint8_t[t->Sizeof() * ArrayLength];
				}
			}
		}
	}

	return true;
}

bool SystemFunctions::Delete(LScriptArguments &Args)
{
	if (Args.Length() != 1)
		return false;

	LVariant *v = Args[0];
	if (v->Type == GV_CUSTOM)
	{
		DeleteArray(v->Value.Custom.Data);
		v->Empty();
	}
	else
	{
		v->Empty();
	}

	*Args.GetReturn() = true;
	return true;
}

class GFileListEntry : public GDom
{
	bool Folder;
	LVariant Name;
	int64 Size;
	LDateTime Modified;

public:
	GFileListEntry(LDirectory *d)
	{
		Folder = d->IsDir();
		Name = d->GetName();
		Size = d->GetSize();
		Modified.Set(d->GetLastWriteTime());
	}

	bool GetVariant(const char *Var, LVariant &Value, char *Arr = 0)
	{
		GDomProperty p = LStringToDomProp(Var);
		switch (p)
		{
			case ObjName:
				Value = Name;
				break;
			case ObjLength:
				Value = Size;
				break;
			case FileFolder:
				Value = Folder;
				break;
			case FileModified:
				Value = &Modified;
				break;
			default:
				return false;
		}
		return true;
	}
};

bool SystemFunctions::DeleteFile(LScriptArguments &Args)
{
	if (Args.Length() != 1)
		return false;

	char *f = Args[0]->CastString();
	if (f)
		*Args.GetReturn() = FileDev->Delete(Args[0]->CastString());
	else
		*Args.GetReturn() = false;
	return true;
}

bool SystemFunctions::CurrentScript(LScriptArguments &Args)
{
	LCompiledCode *Code;
	if (Engine &&
		(Code = Engine->GetCurrentCode()))
	{
		*Args.GetReturn() = Code->GetFileName();
		return true;
	}
	return false;
}

bool SystemFunctions::PathExists(LScriptArguments &Args)
{
	if (Args.Length() == 0)
		return false;
		
	LDirectory d;
	if (d.First(Args[0]->CastString(), NULL))
	{
		if (d.IsDir())
			*Args.GetReturn() = 2;
		else
			*Args.GetReturn() = 1;
	}
	else
	{
		*Args.GetReturn() = 0;
	}
	
	return true;		
}

bool SystemFunctions::PathJoin(LScriptArguments &Args)
{
	char p[MAX_PATH] = "";
	for (unsigned i=0; i<Args.Length(); i++)
	{
		char *s = Args[i]->CastString();
		if (i)
			LMakePath(p, sizeof(p), p, s);
		else
			strcpy_s(p, sizeof(p), s);
	}
	
	if (*p)
		*Args.GetReturn() = p;
	else
		Args.GetReturn()->Empty();
	return true;
}

bool SystemFunctions::PathSep(LScriptArguments &Args)
{
	*Args.GetReturn() = DIR_STR;
	return true;
}

bool SystemFunctions::ListFiles(LScriptArguments &Args)
{
	if (Args.Length() < 1)
		return false;

	Args.GetReturn()->SetList();
	LDirectory d;
	char *Pattern = Args.Length() > 1 ? Args[1]->CastString() : 0;
	
	char *Folder = Args[0]->CastString();
	for (int b=d.First(Folder); b; b=d.Next())
	{
		if (!Pattern || MatchStr(Pattern, d.GetName()))
		{
			Args.GetReturn()->Value.Lst->Insert(new LVariant(new GFileListEntry(&d)));
		}
	}

	return true;
}

bool SystemFunctions::CreateSurface(LScriptArguments &Args)
{
	Args.GetReturn()->Empty();

	if (Args.Length() < 2)
		return false;

	int x = Args[0]->CastInt32();
	int y = Args[1]->CastInt32();
	LColourSpace Cs = CsNone;

	if (Args.Length() > 2)
	{
		LVariant *Type = Args[2];
		const char *c;
		if (Type->IsInt())
		{
			// Bit depth... convert to default Colour Space.
			Cs = GBitsToColourSpace(Type->CastInt32());
		}
		else if ((c = Type->Str()))
		{
			// Parse string colour space def
			Cs = GStringToColourSpace(Type->Str());
		}
	}

	if (!Cs) // Catch all error cases and make it the default screen depth.
		Cs = GdcD->GetColourSpace();

	if ((Args.GetReturn()->Value.Surface.Ptr = new LMemDC(x, y, Cs)))
	{
		Args.GetReturn()->Type = GV_LSURFACE;
		Args.GetReturn()->Value.Surface.Own = true;
		Args.GetReturn()->Value.Surface.Ptr->AddRef();
	}

	return true;
}

bool SystemFunctions::MessageDlg(LScriptArguments &Args)
{
	if (Args.Length() < 4)
		return false;

	LViewI *Parent = CastGView(*Args[0]);
	char *Msg = Args[1]->Str();
	char *Title = Args[2]->Str();
	uint32_t Btns = Args[3]->CastInt32();

	int Btn = LgiMsg(Parent, Msg, Title, Btns);
	*Args.GetReturn() = Btn;

	return true;
}

bool SystemFunctions::GetInputDlg(LScriptArguments &Args)
{
	if (Args.Length() < 4)
		return false;

	LViewI *Parent = CastGView(*Args[0]);
	char *InitVal = Args[1]->Str();
	char *Msg = Args[2]->Str();
	char *Title = Args[3]->Str();
	bool Pass = Args.Length() > 4 ? Args[4]->CastInt32() != 0 : false;
	GInput Dlg(Parent, InitVal, Msg, Title, Pass);
	if (Dlg.DoModal())
	{
		*Args.GetReturn() = Dlg.GetStr();
	}

	return true;
}

bool SystemFunctions::GetViewById(LScriptArguments &Args)
{
	Args.GetReturn()->Empty();

	if (Args.Length() < 2)
		return false;

	LViewI *Parent = CastGView(*Args[0]);
	int Id = Args[1]->CastInt32();
	if (!Parent || Id <= 0)
		return false;

	if (Parent->GetViewById(Id, Args.GetReturn()->Value.View))
	{
		Args.GetReturn()->Type = GV_GVIEW;
	}

	return true;
}

bool SystemFunctions::Execute(LScriptArguments &Args)
{
	if (Args.Length() < 2)
		return false;

	LStringPipe p;
	char *Exe = Args[0]->CastString();
	char *Arguments = Args[1]->CastString();
	LSubProcess e(Exe, Arguments);
	bool Status = e.Start();
	if (Status)
	{
		e.Communicate(&p);
		LAutoString o(p.NewStr());
		*Args.GetReturn() = o;
	}
	else if (Log)
	{
		uint32_t ErrCode = e.GetErrorCode();
		LString ErrMsg = LErrorCodeToString(ErrCode);
		if (ErrMsg)
			Log->Print("Error: Execute(\"%s\",\"%s\") failed with '%s'\n", Exe, Arguments, ErrMsg.Get());
		else
			Log->Print("Error: Execute(\"%s\",\"%s\") failed with '0x%x'\n", Exe, Arguments, ErrCode);
	}
	
	return Status;
}

bool SystemFunctions::System(LScriptArguments &Args)
{
	if (Args.Length() < 2)
		return false;

	char *Exe = Args[0]->Str();
	char *Arg = Args[1]->Str();
	*Args.GetReturn() = LExecute(Exe, Arg);
	return true;
}

bool SystemFunctions::OsName(LScriptArguments &Args)
{
	*Args.GetReturn() = LGetOsName();
	return true;
}

bool SystemFunctions::OsVersion(LScriptArguments &Args)
{
	LArray<int> Ver;
	LGetOs(&Ver);
	Args.GetReturn()->SetList();
	for (int i=0; i<3; i++)
		Args.GetReturn()->Value.Lst->Insert(new LVariant(Ver[i]));

	return true;
}

#define DefFn(Name) \
	GHostFunc(#Name, 0, (ScriptCmd)&SystemFunctions::Name)

GHostFunc SystemLibrary[] =
{
	// Debug
	DefFn(Assert),
	DefFn(Throw),
	DefFn(DebuggerEnabled),

	// String handling
	DefFn(LoadString),
	DefFn(FormatSize),
	DefFn(Sprintf),
	DefFn(Print),
	DefFn(ToString),
	
	// Containers/objects
	DefFn(New),
	DefFn(Delete),

	// Files
	DefFn(LReadTextFile),
	DefFn(WriteTextFile),
	DefFn(SelectFiles),
	DefFn(SelectFolder),
	DefFn(ListFiles),
	DefFn(DeleteFile),

	DefFn(CurrentScript),
	DefFn(PathJoin),
	DefFn(PathExists),
	DefFn(PathSep),

	// Time	
	DefFn(ClockTick),
	DefFn(Sleep),
	DefFn(Now),

	// Images
	DefFn(CreateSurface),

	// UI
	DefFn(MessageDlg),
	DefFn(GetInputDlg),
	DefFn(GetViewById),

	// System
	DefFn(Execute),
	DefFn(System),
	DefFn(OsName),
	DefFn(OsVersion),

	// End of list marker
	GHostFunc(0, 0, 0),
};

GHostFunc *SystemFunctions::GetCommands()
{
	return SystemLibrary;
}

