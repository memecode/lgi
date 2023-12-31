#include <stdio.h>
#include <wchar.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/ParseCpp.h"
#include "lgi/common/ThreadEvent.h"
#include "lgi/common/LexCpp.h"
#include "lgi/common/Variant.h"
#include "lgi/common/XmlTree.h"
#include "lgi/common/Token.h"
#include "ScriptingPriv.h"

#define CheckToken(t)	if (!t) { Error = true; break; }
#if defined(WINDOWS) && defined(_MSC_VER) && !defined(_WIN64)
#define Debug			__asm int 3
#else
#define Debug			assert(0)
#endif

static const char16 sRoundBracketStart[] = { '(', 0 };
static const char16 sRoundBracketEnd[] = { ')', 0 };
static const char16 sDefined[] = {'d','e','f','i','n','e','d',0};
static const char16 sCpp[] = {'_','_','c','p','l','u','s','p','l','u','s',0};

enum LoopState
{
	LsLooping,
	LsFinished,
	LsError
};

struct PreprocessState
{
	bool ParentIsActive;
	bool IfBranchClaimed;
};

enum PreprocessSymbol
{
	SourceBlock,
	HashIf,
	HashElif,
	HashIfndef,
	HashIfdef,
	HashElse,
	HashEndif,
	HashPragma,
	HashError,
	HashWarning,
	HashDefine,
	HashUndef,
	HashInclude,
};

const char *PreprocessSymbolNames[] =
{
	"SourceBlock",
	"HashIf",
	"HashElif",
	"HashIfndef",
	"HashIfdef",
	"HashElse",
	"HashEndif",
	"HashPragma",
	"HashError",
	"HashWarning",
	"HashDefine",
	"HashUndef",
	"HashInclude",
};

/// A file is parsed into a stream of these preprocessed blocks. The content
/// of the block may be present in unlexed form, or parsed form. This saves
/// on having to re-lex files over and over.
class PreprocessBlock
{
	friend struct LSourceFile;
	
	LArray<int> Lines;

public:
	/// The type of block this is.
	PreprocessSymbol Type;
	
	/// The start line in the original source code of this block.
	int BlockLine;
	
	// Unparsed string data
	char16 *Start;
	ssize_t Len;
	
	// -or-
	
	// Lexed data
	int CurToken;
	int Line;
	LArray<char16*> Tokens;
	
	PreprocessBlock()
	{
		Type = SourceBlock;
		Start = NULL;
		Len = 0;
		CurToken = 0;
		BlockLine = 0; // Invalid line..
	}
	
	char16 *NextToken()
	{
		if (Start != NULL)
			LAssert(0); // Call LSourceFile::Lex first
			
		if (CurToken >= Tokens.Length())
			return NULL;
			
		Line = Lines[CurToken];
		return Tokens[CurToken++];
	}
	
	int GetLine()
	{
		if (CurToken >= Tokens.Length())
			Lines.Last();
		
		return Lines[CurToken];		
	}
};

bool CmpToken(char16 *a, const char *b)
{
	if (!a || !b)
		return false;
		
	while (*a && *b)
	{
		if (ToLower(*a) != ToLower(*b))
			return false;
		a++;
		b++;
	}
	
	return !*a && !*b;
}

bool HasToken(LArray<char16*> &a, const char *b)
{
	if (!a.Length() || !b)
		return false;
	
	for (int i=0; i<a.Length(); i++)
	{
		if (CmpToken(a[i], b))
			return true;
	}
	
	return false;
}

struct WorkUnit
{
	LArray<LAutoString> IncludePaths;
	LArray<LCppParser::ValuePair*> PreDefines;
	LArray<LAutoString> Source;

	LAutoString SearchTerm;
	
	WorkUnit
	(
		LArray<const char*> &IncPaths,
		LArray<LCppParser::ValuePair*> &PreDefs,
		LArray<char*> &Src
	)
	{
		int i;
		for (i=0; i<IncPaths.Length(); i++)
		{
			IncludePaths.New().Reset(NewStr(IncPaths[i]));
		}
		for (i=0; i<PreDefs.Length(); i++)
		{
			PreDefs.Add(new LCppParser::ValuePair(*PreDefs[i]));
		}
		for (i=0; i<Src.Length(); i++)
		{
			Source.New().Reset(NewStr(Src[i]));
		}
	}
	
	WorkUnit(const char *Search)
	{
		SearchTerm.Reset(NewStr(Search));
	}
	
	~WorkUnit()
	{
		PreDefines.DeleteObjects();
	}
};

struct LSymbol
{
	LSymbolType Type;
	char *File;
	uint32_t Line;
	LArray<char16*> Tokens;
	
	LSymbol *Parent;
	LArray<LSymbol*> Children;
	
	const char *DefineFile;
	int DefineLine;
	
	bool FriendDecl;
	bool ForwardDecl;
	
	LSymbol(const char *def_file, int def_line)
	{
		Parent = 0;
		Type = SymNone;
		File = NULL;
		Line = 0;
		FriendDecl = false;
		ForwardDecl = false;
		DefineFile = def_file;
		DefineLine = def_line;
	}

	~LSymbol()
	{
		Children.DeleteObjects();
	}
	
	void Add(LSymbol *s)
	{
		if (!Children.HasItem(s))
		{
			Children.Add(s);
			s->Parent = this;
		}
	}
};

struct LCppStringPool
{
	ssize_t Size;
	ssize_t Used;
	LArray<char16*> Mem;
	
	LCppStringPool(int sz)
	{
		Used = Size = sz;
	}	
	
	~LCppStringPool()
	{
		Mem.DeleteArrays();
	}
	
	char16 *Alloc(const char16 *s, ssize_t len)
	{
		if (len < 0)
			len = StrlenW(s);
		
		auto remaining = Size - Used;
		char16 *n;
		if (len + 1 > remaining)
		{
			n = new char16[Size];
			Mem.Add(n);
			Used = 0;
		}
		else
		{
			LAssert(Mem.Length() > 0);
			n = Mem.Last() + Used;
		}
		
		memcpy(n, s, sizeof(*s) * len);
		n[len] = 0;
		Used += len + 1;
		
		return n;
	}
};

char16 *LexPoolAlloc(void *Context, const char16 *s, ssize_t len)
{
	return ((LCppStringPool*)Context)->Alloc(s, len);
}

struct LSourceFile
{
	uint32_t Id;
	LAutoString Path;
	LCppStringPool Pool;

	LAutoWString Raw;
	bool Active;
	LArray<PreprocessState> Stack;

	int CurBlock;
	LArray<PreprocessBlock> Blocks;
	PreprocessBlock *Block;
	
	LSourceFile() : Pool(64 << 10)
	{
		Id = 0;
		CurBlock = 0;
		Block = NULL;
		Active = true;
	}

	PreprocessBlock *Current()
	{
		if (!Block && CurBlock == 0)
			Block = &Blocks[CurBlock];
		return Block;
	}

	PreprocessBlock *Next()
	{
		CurBlock++;
		if (CurBlock >= Blocks.Length())
		{
			Block = NULL;
			return NULL;
		}
		
		Block = &Blocks[CurBlock];
		return Block;
	}
	
	bool Read(const char *path)
	{
		if (!Path.Reset(NewStr(path)))
			return false;
		
		LAutoString f(LReadTextFile(Path));
		if (!f)
			return false;

		Raw.Reset(Utf8ToWide(f));

		return true;
	}
	
	bool Lex(PreprocessBlock &blk)
	{
		if (blk.Start)
		{
			char16 *Cur = blk.Start;
			char16 *End = blk.Start + blk.Len;
			int Line = blk.BlockLine;
			char16 *t;

			// Temporarily NULL terminate this block
			char16 OldEndChar = *End;
			*End = 0;

			if (blk.Type == HashInclude)
			{
				// Special include parsing...
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					if (CmpToken(t, "<"))
					{
						char16 delim = '>';
						char16 *End = StrchrW(t = Cur, delim);
						*End = 0;
					}
					else if (*t == '\"' || *t == '\'')
					{
						// Trim the string markers
						char16 delim = *t++;
						char16 *End = StrchrW(t, delim);
						*End = 0;
					}
					
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
					break;
				}
			}
			else if (blk.Type == HashDefine)
			{
				// Special #define parsing to remove the line continuation characters...
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					if (CmpToken(t, "\\"))
					{
						// Is this a line continuation char?
						char16 *End = Cur;
						bool AllWhiteSpace = true;
						while (*End && *End != '\r' && *End != '\n')
						{
							if (*End != ' ' && *End != '\t')
							{
								AllWhiteSpace = false;
								break;
							}
							End++;
						}
						if (AllWhiteSpace)
						{
							continue;
						}
					}
					
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
				}
			}
			else
			{
				// General code parsing
				while (*Cur && (t = LexCpp(Cur, LexPoolAlloc, &Pool, &Line)))
				{
					blk.Tokens.Add(t);
					blk.Lines.Add(Line);
				}			
			}

			// Restore the string to normal
			*End = OldEndChar;
			
			// Clear the unparsed data ptr
			blk.Start = NULL;
		}
		
		return NULL;
	}
	
	void DumpBlocks()
	{
		#if 1
		LXmlTree t;
		LFile f;
		char *Leaf = strrchr(Path, DIR_CHAR);
		char Out[MAX_PATH_LEN];
		LMakePath(Out, sizeof(Out), "c:\\temp", Leaf + 1);
		char *Ext = LGetExtension(Out);
		strcpy(Ext, "xml");
		if (f.Open(Out, O_WRITE))
		{
			f.SetSize(0);
			LXmlTag r("Dump");
			r.SetAttr("Path", Path);
			for (int i=0; i<Blocks.Length(); i++)
			{
				PreprocessBlock &b = Blocks[i];
				LXmlTag *c = new LXmlTag(PreprocessSymbolNames[b.Type]);
				r.InsertTag(c);
				c->SetAttr("Line", b.BlockLine);
				
				LAutoString a(WideToUtf8(b.Start, b.Len));
				c->SetContent(a);
			}
			
			t.Write(&r, &f);
			f.Close();
		}
		else LAssert(0);
		#endif		
	}

	/*	
	bool At(const char *file, int line)
	{
		char *d = Path ? strrchr(Path, DIR_CHAR) : NULL;
		if (!d)
			return false;
		return stricmp(d+1, file) == 0 && Line == line;
	}
	*/
};

typedef LHashTbl<StrKeyPool<char16>, LSymbol*> SourceScopeMap;
struct LSourceScope : public SourceScopeMap
{
	LSourceScope() : SourceScopeMap(5000)
	{
	}

	~LSourceScope()
	{
		DeleteObjects();
	}

	bool Add(char16 *k, LSymbol *v)
	{
		if (Find(k))
		{
			return false;
		}
		
		return SourceScopeMap::Add(k, v);
	}

	LSymbol *Define(char16 *name, LSymbolType Type, const char *def_file, int def_line)
	{
		LSymbol *s = Find(name);
		if (!s) Add(name, s = new LSymbol(def_file, def_line));
		s->Type = Type;
		s->File = NULL;
		s->Line = 0;		
		return s;
	}
};

enum KeywordType
{
	KwNone,
	KwSpecifier,
	KwType,
	KwQualifier,
	KwModifier,
	KwUserType,
	KwVisibility,
	KwPreprocessor
};

enum MsgType
{
	MsgError,
	MsgWarning,
	MsgInfo,
};

template<typename T, typename V>
void AddHash(T tbl, const char *name, V type)
{
	char16 s[256];
	Strcpy(s, CountOf(s), name);
	tbl.Add(s, type);
}

struct LCppParserWorker : public LCompileTools
{
	LCppParserPriv *d;
	LAutoPtr<WorkUnit> w;
	LCppStringPool GeneralPool;
	
	LArray<LSourceFile*> Srcs;
	LArray<LSourceScope*> Scopes; // from global to local
	
	LHashTbl<StrKey<char16>, KeywordType> Keywords;
	LHashTbl<StrKey<char16>, PreprocessSymbol> PreprocessSyms;
	
	LHashTbl<ConstStrKey<char,false>, char*> IncludePathFiles;
	char *FindInclude(char *file);
	bool Preprocess(LSourceFile *sf);
	
	void InitScopes();
	LSourceScope *CurrentScope() { return Scopes[Scopes.Length()-1]; }
	LAutoWString GetSymbolName(LArray<char16*> &a, bool IsEnum = false);

	LSymbol *ParseDecl(LSourceFile *sf, char16 *t);
	LSymbol *ParseUserType(LSourceFile *sf, char16 *t);
	LSymbol *ParseTypedef(LSourceFile *sf);
	bool ParsePreprocessor(LSourceFile *sf);
	
public:
	LCppParserWorker(LCppParserPriv *priv);
	~LCppParserWorker();
	
	LSourceFile *ParseCpp(const char *Path);	
	LSymbol *Resolve(char16 *Symbol);
	int Evaluate(LArray<char16*> &Exp);
	void DoWork(WorkUnit *wk);
	int Main();
	void Msg(MsgType Type, const char *Fmt, ...);
};

struct LCppParserPriv
{
	LArray<char*> IncPaths;
	LAutoPtr<LCppParserWorker> Worker;
};

LCppParserWorker::LCppParserWorker(LCppParserPriv *priv) :
	GeneralPool(4 << 10)
{
	d = priv;
	
	AddHash(Keywords, "auto", KwSpecifier);
	AddHash(Keywords, "extern", KwSpecifier);
	AddHash(Keywords, "static", KwSpecifier);
	AddHash(Keywords, "register", KwSpecifier);
	AddHash(Keywords, "mutable", KwSpecifier);

	AddHash(Keywords, "bool", KwType);
	AddHash(Keywords, "char", KwType);
	AddHash(Keywords, "wchar_t", KwType);
	AddHash(Keywords, "short", KwType);
	AddHash(Keywords, "int", KwType);
	AddHash(Keywords, "long", KwType);
	AddHash(Keywords, "float", KwType);
	AddHash(Keywords, "double", KwType);
	AddHash(Keywords, "void", KwType);
	#ifdef WIN32
	AddHash(Keywords, "__int64", KwType);
	AddHash(Keywords, "_W64", KwModifier);
	#endif
	AddHash(Keywords, "unsigned", KwModifier);
	AddHash(Keywords, "signed", KwModifier);
	AddHash(Keywords, "__declspec", KwModifier);
	AddHash(Keywords, "template", KwModifier);
	AddHash(Keywords, "__stdcall", KwModifier);
	AddHash(Keywords, "friend", KwModifier);
	AddHash(Keywords, "operator", KwModifier);
	
	AddHash(Keywords, "class", KwUserType);
	AddHash(Keywords, "struct", KwUserType);
	AddHash(Keywords, "union", KwUserType);
	AddHash(Keywords, "enum", KwUserType);
	
	AddHash(Keywords, "public:", KwVisibility);
	AddHash(Keywords, "private:", KwVisibility);
	AddHash(Keywords, "protected:", KwVisibility);

	AddHash(Keywords, "#if", KwPreprocessor);
	AddHash(Keywords, "#ifdef", KwPreprocessor);
	AddHash(Keywords, "#ifndef", KwPreprocessor);
	AddHash(Keywords, "#elif", KwPreprocessor);
	AddHash(Keywords, "#else", KwPreprocessor);
	AddHash(Keywords, "#endif", KwPreprocessor);
	AddHash(Keywords, "#include", KwPreprocessor);
	AddHash(Keywords, "#define", KwPreprocessor);
	AddHash(Keywords, "#undef", KwPreprocessor);
	AddHash(Keywords, "#error", KwPreprocessor);
	AddHash(Keywords, "#warning", KwPreprocessor);
	AddHash(Keywords, "#pragma", KwPreprocessor);

	AddHash(PreprocessSyms, "#if", HashIf);
	AddHash(PreprocessSyms, "#elif", HashElif);
	AddHash(PreprocessSyms, "#ifndef", HashIfndef);
	AddHash(PreprocessSyms, "#ifdef", HashIfdef);
	AddHash(PreprocessSyms, "#else", HashElse);
	AddHash(PreprocessSyms, "#endif", HashEndif);
	AddHash(PreprocessSyms, "#pragma", HashPragma);
	AddHash(PreprocessSyms, "#error", HashError);
	AddHash(PreprocessSyms, "#warning", HashWarning);
	AddHash(PreprocessSyms, "#define", HashDefine);
	AddHash(PreprocessSyms, "#undef", HashUndef);
	AddHash(PreprocessSyms, "#include", HashInclude);
}

LCppParserWorker::~LCppParserWorker()
{
	Srcs.DeleteObjects();
	Scopes.DeleteObjects();
	IncludePathFiles.DeleteArrays();
}

void LCppParserWorker::Msg(MsgType Type, const char *Fmt, ...)
{
	va_list Arg;
	va_start(Arg, Fmt);
	#ifdef WIN32
	char Buf[256];
	#ifdef _MSC_VER
	vsnprintf_s(Buf, sizeof(Buf), _TRUNCATE, Fmt, Arg);
	#else
	vsnprintf(Buf, sizeof(Buf), Fmt, Arg);
	#endif
	OutputDebugStringA(Buf);
	#else
	vprintf(Fmt, Arg);
	#endif
	va_end(Arg);
	if (Type == MsgError)
		Debug;
}

char *LCppParserWorker::FindInclude(char *File)
{
	if (IncludePathFiles.Length() == 0)
	{
		for (int i=0; i<w->IncludePaths.Length(); i++)
		{
			LDirectory dir;
			char p[MAX_PATH_LEN];
			for (bool b=dir.First(w->IncludePaths[i]); b; b=dir.Next())
			{
				auto Leaf = dir.GetName();
				if (!dir.IsDir())
				{
					char *ext = LGetExtension(Leaf);
					if (ext && ext[0] == 'h' && !ext[1])
					{
						if (dir.Path(p, sizeof(p)))
						{
							if (!IncludePathFiles.Find(Leaf))
								IncludePathFiles.Add(Leaf, NewStr(p));
						}
					}
				}
			}
		}
	}
	
	char *p = IncludePathFiles.Find(File);
	return p;
}

void LCppParserWorker::InitScopes()
{
	Scopes.DeleteObjects();

	Scopes.Add(new LSourceScope);
	
	#ifdef WIN32
	char16 buf[256];
	LSymbol *s;

	#ifdef _MSC_VER
	s = Scopes[0]->Define(L"_MSC_VER", SymDefineValue, _FL);
	int ch = swprintf_s(buf, CountOf(buf), L"%i", _MSC_VER);
	s->Tokens.Add(GeneralPool.Alloc(buf, ch));
	s->File = "none";
	#endif

	s = Scopes[0]->Define(L"WIN32", SymDefineValue, _FL);
	s->File = "none";
	#endif
}

struct RoundBracket
{
	int Depth;
	int Sections;
	bool HasPtr;
	
	RoundBracket()
	{
		Depth = 0;
		Sections = 0;
		HasPtr = false;
	}
	
	void Parse(LArray<char16*> &a)
	{
		for (int i=0; i<a.Length(); i++)
		{
			char16 *t = a[i];
			if (CmpToken(t, "__declspec"))
			{
				while ((t = a[++i]))
				{
					if (CmpToken(t, ")"))
						break;
				}
			}
			else if (CmpToken(t, "("))
			{
				if (Depth == 0)
					Sections++;
				Depth++;
			}
			else if (CmpToken(t, ")"))
			{
				Depth--;
				LAssert(Depth >= 0);
			}
			else if (CmpToken(t, "*") && Sections == 1)
			{
				HasPtr = true;
			}
		}
		
		LAssert(Depth == 0);
	}
};

LAutoWString LCppParserWorker::GetSymbolName(LArray<char16*> &in, bool IsEnum)
{
	LArray<char16*> a = in;
	LAutoWString ret;
	LStringPipe p(64);
	RoundBracket Rb;
	int LastName = -1;
	bool FuncPtr = false;
	int Operator = -1;
	char16 *t, *prev = NULL;

	Rb.Parse(a);
	if (Rb.Sections == 2 && Rb.HasPtr)
		FuncPtr = true;
		
	Rb.Sections = 0;
	for (int i=0; i<a.Length(); i++)
	{
		t = a[i];
		LSymbol *sym;
		
		KeywordType kt = Keywords.Find(t);
		if (kt)
		{
			if (kt == KwModifier)
			{
				if (CmpToken(t, "__declspec"))
				{
					t = a[++i];
					if (!CmpToken(t, "("))
					{
						Msg(MsgError, "%s:%i - unexpected token.\n", _FL);
						break;
					}
					while ((t = a[++i]))
					{
						if (CmpToken(t, ")"))
						{
							break;
						}
					}
				}				
				else if (CmpToken(t, "operator"))
				{
					Operator = i;
				}
			}
		}
		else if ((sym = Resolve(t)))
		{
			if (sym->Type == SymDefineValue ||
				sym->Type == SymDefineFunction)
			{
				if (sym->Type == SymDefineFunction)
				{
					// Resolve the arguments...
					t = a[++i];
					if (!CmpToken(t, "("))
					{
						Msg(MsgError, "Missing '(' in call to '%S' define.\n", t);
						break;
					}
					
					int Depth = 1;
					while ((t = a[++i]))
					{
						if (CmpToken(t, "("))
						{
							Depth++;
						}
						else if (CmpToken(t, ")"))
						{
							if (--Depth <= 0)
								break;

						}
					}

					Msg(MsgWarning, "NotImpl: Parsing of #define arguments.\n");
				}
				
				a.DeleteAt(i, true);
				for (int n=0; n<sym->Tokens.Length(); n++)
				{
					char16 *s = sym->Tokens[n];
					a.AddAt(i+n, s);
				}
				i--;
			}
			else if (sym->Type == SymTypedef)
			{
				a.DeleteAt(i, true);
				if (i < a.Length())
				{
					char16 *Next = a[i];
					a.DeleteAt(i, true);
					
					for (int n=0; n<sym->Tokens.Length(); n++)
					{
						char16 *s = sym->Tokens[n];
						if (StricmpW(s, t))
							a.AddAt(i+n, s);
						else
							a.AddAt(i+n, Next);
					}
					
					i--;
					
					RoundBracket rb;
					rb.Parse(a);
					if (rb.Sections == 2 && rb.HasPtr)
						FuncPtr = true;
				}
				else
				{
					// This can happen when you define a typedef to twice
					LAutoWString Empty;
					return Empty;
				}
			}
			else
			{
				goto IsAlpha;
			}
		}
		else if (CmpToken(t, "("))
		{
			if (Rb.Depth == 0)
				Rb.Sections++;
			Rb.Depth++;
		}
		else if (CmpToken(t, ")"))
		{
			Rb.Depth--;
			LAssert(Rb.Depth >= 0);
		}
		else if (IsAlpha(*t) || *t == '_')
		{
			IsAlpha:
			if (FuncPtr)
			{
				if (Rb.Depth == 1 &&
					Rb.Sections == 1)
				{
					LastName = i;
				}
			}
			else if (Rb.Depth == 0)
			{
				LastName = i;
			}
		}
		
		prev = t;
	}

	if (Operator >= 0)
	{
		for (int i=Operator; i<a.Length(); i++)
		{
			char16 *t = a[i];
			if (i > Operator)
				p.Write(L"_", sizeof(char16));
			p.Write(t, StrlenW(t) * sizeof(char16) );
		}
	}
	else
	{
		if (IsEnum && LastName < 0)
			return LAutoWString();
		
		LAssert(LastName >= 0);
		t = a[LastName];
		p.Write(t, StrlenW(t) * sizeof(char16) );
	}
	
	ret.Reset(p.NewStrW());
	LAssert(ret);
	
	return ret;
}

LSymbol *LCppParserWorker::Resolve(char16 *Symbol)
{
	for (auto i = Scopes.Length()-1; i >= 0; i--)
	{
		LSourceScope *scope = Scopes[i];
		LSymbol *sym = scope->Find(Symbol);
		if (sym)
			return sym;
	}
	
	return NULL;
}

LVariant *GetArg(LArray<LVariant> &Values, int i)
{
	if (i < 0 || i >= Values.Length())
		return NULL;
	if (Values[i].Type == GV_OPERATOR)
		return NULL;
	return &Values[i];	
}

int FindMatchingBracket(LArray<char16*> &Exp, int Start)
{
	int Depth = 1;
	for (int i = Start + 1; i<Exp.Length(); i++)
	{
		char16 *t = Exp[i];
		if (!StrcmpW(t, sRoundBracketStart))
		{
			Depth++;
		}
		else if (!StrcmpW(t, sRoundBracketEnd))
		{
			if (--Depth == 0)
				return i;
		}
	}
	
	return -1;	
}

int LCppParserWorker::Evaluate(LArray<char16*> &Exp)
{
	bool Error = false;
	LArray<LVariant> Values;
	
	for (int i=0; i<Exp.Length(); i++)
	{
		char16 *t = Exp[i];
		if (!StricmpW(t, sDefined))
		{
			t = Exp[++i];
			CheckToken(t);
			char16 *Define = t;
			if (!StricmpW(t, sRoundBracketStart))
			{
				Define = Exp[++i];
				CheckToken(Define);
				t = Exp[++i];
				CheckToken(t);
				if (StricmpW(t, sRoundBracketEnd))
				{
					Error = true;
					break;
				}
			}
			LSymbol *DefineSym = Resolve(Define);
			Values.New() = DefineSym != NULL;
		}
		else if (*t == '(')
		{
			int End = FindMatchingBracket(Exp, i);
			if (End < 0)
			{
				Msg(MsgError, "Couldn't find matching ')'\n");
				Error = true;
				break;
			}

			// Create array of sub-expression tokens and then evaluate that			
			LArray<char16*> Sub;
			for (int n=i+1; n<End; n++)
			{
				Sub.Add(Exp[n]);
			}
			i = End;			
			int Result = Evaluate(Sub);
			Values.New() = Result;
		}
		else if (IsDigit(*t))
		{
			Values.New() = AtoiW(t);
		}
		else
		{
			LOperator Op = IsOp(t, false);
			if (Op != OpNull)
			{
				LVariant &o = Values.New();
				o.Type = GV_OPERATOR;
				o.Value.Op = Op;
			}
			else
			{
				char16 *Next = i < Exp.Length() - 1 ? Exp[i + 1] : NULL;
				if (Next && !StrcmpW(Next, sRoundBracketStart))
				{
					// Macro call
					int End = FindMatchingBracket(Exp, i + 1);
					if (End < 0)
					{
						Error = true;
						Msg(MsgError, "No matching ')'\n");
						break;
					}
					
					LSymbol *Sym = Resolve(t);
					if (Sym)
					{
						Msg(MsgError, "Implement macro calling.\n");
					}

					i = End;
					Values.New() = 0; // No macro defined... just zero it
				}
				else
				{
					// Straight value
					LSymbol *Sym = Resolve(t);
					if (Sym)
					{
						int Val = Evaluate(Sym->Tokens);
						Values.New() = Val;
					}
					else
					{
						Values.New() = 0;
					}
				}
			}
		}
	}

	if (Error)
		return false;
	
	// Evaluate the contents of Values
	while (Values.Length() > 1)
	{
		auto StartLength = Values.Length();
		int OpIdx = -1;
		int Prec = -1;
		for (int i=0; i<Values.Length(); i++)
		{
			LVariant &v = Values[i];
			if (v.Type == GV_OPERATOR)
			{
				int p = GetPrecedence(v.Value.Op);
				if (OpIdx < 0 || p < Prec)
				{
					OpIdx = i;
					Prec = p;
				}
			}
		}
		
		if (OpIdx < 0)
		{
			Msg(MsgError, "No Operator?\n");
			break;
		}
		
		LVariant &OpVar = Values[OpIdx];
		OperatorType Type = OpType(OpVar.Value.Op);
		switch (Type)
		{
			case OpPrefix:
			{
				LVariant *a = GetArg(Values, OpIdx + 1);
				if (!a)
				{
					Msg(MsgError, "Missing op arg.\n");
					break;
				}

				switch (OpVar.Value.Op)
				{
					case OpNot:
					{
						OpVar = !a->CastBool();
						break;
					}
					default:
					{
						Msg(MsgError, "Impl this op.\n");
						break;
					}
				}

				Values.DeleteAt(OpIdx + 1, true);
				break;
			}
			case OpInfix:
			{
				LVariant *a = GetArg(Values, OpIdx - 1);
				LVariant *b = GetArg(Values, OpIdx + 1);
				if (!a || !b)
				{
					Msg(MsgError, "Missing op args.\n");
					break;
				}

				switch (OpVar.Value.Op)
				{
					#define CombineUsingOp(op, type, cpp_op) \
						case op: \
						{ \
							*a = a->Cast##type() cpp_op b->Cast##type(); \
							break; \
						}
					CombineUsingOp(OpOr, Bool, ||);
					CombineUsingOp(OpAnd, Bool, &&);
					CombineUsingOp(OpGreaterThan, Int32, >);
					CombineUsingOp(OpGreaterThanEqual, Int32, >=);
					CombineUsingOp(OpLessThan, Int32, <);
					CombineUsingOp(OpLessThanEqual, Int32, <=);
					CombineUsingOp(OpEquals, Int32, ==);
					CombineUsingOp(OpNotEquals, Int32, !=);
					CombineUsingOp(OpPlus, Int32, +);
					CombineUsingOp(OpMinus, Int32, -);
					CombineUsingOp(OpMul, Int32, *);
					CombineUsingOp(OpDiv, Int32, /);
					CombineUsingOp(OpMod, Int32, %);
					default:
					{
						Msg(MsgError, "Impl this op.\n");
						break;
					}
				}

				Values.DeleteAt(OpIdx, true);
				Values.DeleteAt(OpIdx, true);
				break;
			}
			default:
			{
				Msg(MsgError, "Impl this op type.\n");
				break;
			}
		}
		
		
		LAssert(StartLength > Values.Length());
		if (Values.Length() >= StartLength)
		{
			Msg(MsgError, "Nothing happened.\n");
			break;
		}
	}

	return Values.Length() == 1 ? Values[0].CastInt32() : 0;
}

#define SkipNewLine(Cur) \
	if (Cur[0] == '\r' && Cur[1] == '\n') \
		Cur += 2; \
	else if (Cur[0] == '\r') \
		Cur++; \
	else if (Cur[0] == '\n') \
		Cur++; \
	Line++;

bool LCppParserWorker::Preprocess(LSourceFile *sf)
{
	char16 *Cur = sf->Raw;
	int Line = 1;
	char16 Cmd[256];
	PreprocessBlock *Blk = NULL;
	
	while (*Cur)
	{
		// Find Start of line
		while (IsWhite(*Cur))
			Cur++;

		if (*Cur == '#')
		{
			// Emit a source code block
			if (Blk)
			{
				Blk->Len = Cur - Blk->Start;
				if (Blk->Len <= 8)
				{
					bool IsWhiteSpace = true;
					for (int i=0; i<Blk->Len; i++)
					{
						if (Blk->Start[i] != ' ' &&
							Blk->Start[i] != '\t' &&
							Blk->Start[i] != '\r' &&
							Blk->Start[i] != '\n')
						{
							IsWhiteSpace = false;
						}
					}
					if (IsWhiteSpace)
					{
						Blk->Start = NULL;
						Blk->Len = 0;
					}
					else
					{
						Blk = NULL;
					}
				}
				else
				{
					Blk = NULL;
				}
			}
			
			// Read over the preprocessor command and save the name
			char16 *o = Cmd;
			char16 *e = Cmd + 255;
			*o++ = *Cur++;
			while (IsWhite(*Cur))
				Cur++;
			while (	*Cur &&
					!IsWhite(*Cur) &&
					*Cur != '\r' &&
					*Cur != '\n' &&
					o < e)
			{
				*o++ = *Cur++;
			}
			*o++ = 0;
			while (IsWhite(*Cur))
				Cur++;
			
			// Create a block for the statement
			if (Blk == NULL)
				Blk = &sf->Blocks.New();
			Blk->BlockLine = Line;
			Blk->Type = PreprocessSyms.Find(Cmd);
			LAssert(Blk->Type != SourceBlock);
			Blk->Start = Cur;

			// Scan till the end of line...		
			char16 *LastNonWhite = NULL;			
			do 
			{
				while (*Cur && *Cur != '\r' && *Cur != '\n')
				{
					if (!IsWhite(*Cur))
						LastNonWhite = Cur;
					Cur++;
				}
				if (LastNonWhite && *LastNonWhite == '\\' && Blk->Type == HashDefine)
				{
					// Multi-line #define...
					SkipNewLine(Cur);
				}
				else
				{
					SkipNewLine(Cur);
					break;
				}
			}
			while (*Cur);
			
			Blk->Len = Cur - Blk->Start;
			while (Blk->Len > 0 && strchr(" \r\t\n", Blk->Start[Blk->Len - 1]))
				Blk->Len--;
			Blk = NULL;
		}
		else
		{
			if (!Blk)
			{
				Blk = &sf->Blocks.New();
				Blk->BlockLine = Line;
				Blk->Start = Cur;
			}
			
			// Scan till the end of line...		
			while (*Cur && *Cur != '\r' && *Cur != '\n')
			{
				if (Cur[0] == '/' && Cur[1] == '*')
				{
					// Process multi-line comment...
					for (Cur += 2; *Cur; )
					{
						if (*Cur == '\r' || *Cur == '\n')
						{
							SkipNewLine(Cur);
						}
						else if (Cur[0] == '*' && Cur[1] == '/')
						{
							Cur += 2;
							break;
						}
						else Cur++;
					}
				}
				else
				{				
					Cur++;
				}
			}
				
			// Skip over new line...
			SkipNewLine(Cur);
		}
	}	

	if (Blk)
	{
		Blk->Len = Cur - Blk->Start;
	}

	// sf->DumpBlocks();	
	return true;
}

LSourceFile *LCppParserWorker::ParseCpp(const char *Path)
{
	LSourceFile *sf = NULL;

	for (int i=0; i<Srcs.Length(); i++)
	{
		LSourceFile *s = Srcs[i];
		if (s->Path && !strcmp(s->Path, Path))
		{
			sf = s;
			break;
		}
	}
	
	if (!sf)
	{
		sf = new LSourceFile;
		if (sf)
		{
			if (!sf->Read(Path))
			{
				delete sf;
				return NULL;
			}
			
			Srcs.Add(sf);
			
			Preprocess(sf);
		}
		else return NULL;
	}
	
	bool IsExternC = false;
	char16 *t;

	PreprocessBlock *blk;
	while ((blk = sf->Current()))
	{
		if (blk->Type != SourceBlock)
		{
			if (!ParsePreprocessor(sf))
				break;
		}
		else if (sf->Active)
		{
			bool Error = false;
			
			sf->Lex(*blk);
						
			while (!Error)
			{
				t = blk->NextToken();
				if (!t)
					break;
				
				KeywordType kt = Keywords.Find(t);
				switch (kt)
				{
					case KwPreprocessor:
					{
						LAssert(!"We shouldn't see these here at all.");
						Error = true;
						break;
					}
					case KwUserType:
					{
						LSymbol *sym = ParseUserType(sf, t);
						if (sym)
						{
							if (sf->Active)
							{
								LAutoWString SymName = GetSymbolName(sym->Tokens, sym->Type == SymEnum);
								if (SymName)
								{
									if (CurrentScope()->Find(SymName))
									{
										DeleteObj(sym);
									}
									else
									{
										CurrentScope()->Add(SymName, sym);
									}
								}
								else if (sym->Type == SymEnum)
								{
									// Unnamed enum
									static int Idx = 1;
									#ifdef _MSC_VER
									char16 Buf[256];
									swprintf_s(Buf, CountOf(Buf), L"UnnamedEnum%i", Idx++);
									#else
									char Tmp[256];
									sprintf_s(Tmp, CountOf(Tmp), "UnnamedEnum%i", Idx++);
									LAutoWString Buf(Utf8ToWide(Tmp));
									#endif

									if (CurrentScope()->Find(SymName))
									{
										DeleteObj(sym);
									}
									else
									{
										CurrentScope()->Add(Buf, sym);
									}
								}
								else
								{
									Msg(MsgError, "%s:%i - Failed to get symbol name.\n", _FL);
									DeleteObj(sym);
								}
							}
							else
							{
								DeleteObj(sym);
							}
						}
						
						if (sf->Current())
							blk = sf->Current();
						break;
					}
					default:
					{
						if (IsExternC && CmpToken(t, "}"))
						{
							if (sf->Active)
								IsExternC = false;
						}
						else if (CmpToken(t, "typedef"))
						{
							LSymbol *sym = ParseTypedef(sf);
							if (sym)
							{
								LAutoWString Name = GetSymbolName(sym->Tokens);
								if (Name)
								{
									LSymbol *existing = CurrentScope()->Find(Name);
									if (existing)
									{
										Msg(MsgError, "Symbol '%S' already exists.\n", Name.Get());
									}
									else
									{
										CurrentScope()->Add(Name, sym);
										sym->File = sf->Path;
										sym->Line = blk->GetLine();
										sym = NULL;
									}
								}
								
								DeleteObj(sym);
							}
						}
						else if (CmpToken(t, ";"))
						{
						}
						else
						{
							LSymbol *sym = ParseDecl(sf, t);
							if (sym)
							{
								if (sf->Active)
								{
									if (sym->Type == SymExternC)
									{
										IsExternC = true;
										delete sym;
									}
									else
									{
										LAutoWString Name = GetSymbolName(sym->Tokens);
										if (Name)
										{
											LSymbol *e = Scopes[0]->Find(Name);
											if (e)
											{
												DeleteObj(sym);
											}
											else
											{
												Scopes[0]->Add(Name, sym);
											}
										}
										else
										{
											Msg(MsgError, "%s:%i - Failed to get name of decl.\n", _FL);
											DeleteObj(sym);
										}
									}
								}
								else
								{
									DeleteObj(sym);
								}
							}
							
							if (sf->Current())
								blk = sf->Current();
						}
						break;
					}
				}
			}			
			
			sf->Next();
		}
		else
		{
			sf->Next();
		}
	}

	return sf;
}

LSymbol *LCppParserWorker::ParseDecl(LSourceFile *sf, char16 *t)
{
	PreprocessBlock *blk = sf->Current();
	int ExternC = CmpToken(t, "extern") ? 1 : 0;
	LSymbol *sym = new LSymbol(_FL);

	sym->Type = SymVariable;
	sym->File = sf->Path;
	sym->Line = blk->GetLine();
	
	if (CmpToken(t, "virtual"))
		sym->Type = SymFunction;
		
	if (CmpToken(t, "friend"))
		sym->FriendDecl = true;
	else
		sym->Tokens.Add(t);

	LArray<char16*> Temp;
	if (CmpToken(t, "template"))
	{
		Temp.Add(t);
		while ((t = blk->NextToken()))
		{
			Temp.Add(t);
			if (CmpToken(t, ">"))
				break;
		}
		if (!t)
			blk = sf->Next();
	}

	LoopState Loop = LsLooping;
	int ScopeDepth = 0;
	while (Loop == LsLooping && blk)
	{
		if (blk->Type != SourceBlock)
		{
			// if (stristr(sf->Path, "GDragAndDrop.cpp") && blk->BlockLine == 
			
			if (!ParsePreprocessor(sf))
			{
				Loop = LsError;
				break;
			}
			
			blk = sf->Current();
		}
		else if (!sf->Active)
		{
			blk = sf->Next();
		}
		else
		{
			sf->Lex(*blk);
			
			while (Loop == LsLooping)
			{
				t = blk->NextToken();
				if (!t)
				{
					blk = sf->Next();
					break;
				}
				
				if (CmpToken(t, "{"))
				{
					if (ExternC == 2 && CmpToken(t, "{"))
					{
						sym->Type = SymExternC;
						Loop = LsFinished;
						break;
					}
					
					ScopeDepth++;
				}
				else if (CmpToken(t, "}"))
				{
					if (ScopeDepth <= 0)
					{
						Loop = LsError;
						break;
					}

					if (--ScopeDepth <= 0)
					{
						Loop = LsFinished;
						break;
					}
				}
				else if (ScopeDepth == 0)
				{
					KeywordType kt = Keywords.Find(t);
					if (kt == KwUserType && sym->Type != SymFunction)
					{
						LSymbol *ut = ParseUserType(sf, t);
						if (ut)
							ut->FriendDecl = sym->FriendDecl;
						DeleteObj(sym);
						sym = ut;
						Loop = LsFinished;
						
						if (sf->Current())
							blk = sf->Current();
						break;
					}
					
					if (ExternC > 0)
					{
						if (ExternC == 1 && CmpToken(t, "\"C\""))
						{
							ExternC++;
						}
						else if (ExternC == 2 && CmpToken(t, "{"))
						{
							sym->Type = SymExternC;
							Loop = LsFinished;
							break;
						}
						else
						{
							ExternC = 0;
						}
					}

					if (CmpToken(t, ";"))
					{
						Loop = LsFinished;
						break;
					}

					if (CmpToken(t, "("))
					{
						sym->Type = SymFunction;
					}
					
					sym->Tokens.Add(t);
				}
			}
		}
	}

	return sym;
}

LSymbol *LCppParserWorker::ParseTypedef(LSourceFile *sf)
{
	LSymbol *sym = new LSymbol(_FL);
	sym->Type = SymTypedef;
	PreprocessBlock *blk = sf->Current();
	
	char16 *t = blk->NextToken();
	if (t == NULL)
	{
		delete sym;
		return NULL;
	}

	KeywordType kt = Keywords.Find(t);
	if (kt == KwUserType)
	{
		DeleteObj(sym);
		return ParseUserType(sf, t);
	}

	sym->Tokens.Add(t);
	
	while ((t = blk->NextToken()))
	{
		if (CmpToken(t, ";"))
			break;

		sym->Tokens.Add(t);
	}
	
	return sym;
}

LSymbol *LCppParserWorker::ParseUserType(LSourceFile *sf, char16 *t)
{
	if (!sf || !t)
		return NULL;

	LSymbol *sym = new LSymbol(_FL);
	
	int InScope = 0;
	bool InParentList = false;
	bool Finished = false;
	
	if (CmpToken(t, "union"))
		sym->Type = SymUnion;
	else if (CmpToken(t, "class"))
		sym->Type = SymClass;
	else if (CmpToken(t, "struct"))
		sym->Type = SymStruct;
	else if (CmpToken(t, "enum"))
		sym->Type = SymEnum;
	else
	{
		delete sym;
		return NULL;
	}	
	
	PreprocessBlock *blk = sf->Current();
	
	sym->Tokens.Add(t);
	sym->File = sf->Path;
	sym->Line = blk->GetLine();
	
	while (blk && !Finished)
	{
		if (blk->Type != SourceBlock)
		{
			if (!ParsePreprocessor(sf))
				Finished = true;
			
			blk = sf->Current();
		}
		else if (!sf->Active)
		{
			blk = sf->Next();
		}
		else
		{
			sf->Lex(*blk);
			
			while ((t = blk->NextToken()))
			{
				if (CmpToken(t, "{"))
				{
					InScope++;
				}
				else if (CmpToken(t, "}"))
				{
					ProcessEndBracket:
					if (InScope > 0)
					{
						InScope--;
					}
					else
					{
						LAssert(!"End scope mis-match.");
						Finished = true;
						break;
					}
				}
				else if (CmpToken(t, ":"))
				{
					if (InScope == 0)
					{
						InParentList = true;
					}
				}
				else if (CmpToken(t, ";"))
				{
					if (!InScope)
					{
						Finished = true;
						break;
					}
				}
				else
				{
					KeywordType kt = Keywords.Find(t);
					if (kt == KwVisibility)
					{
					}
					else if (kt == KwPreprocessor)
					{
						LAssert(0);
					}
					else if (kt == KwUserType)
					{
						LSymbol *cls = ParseUserType(sf, t);
						if (cls)
						{
							if (sf->Active)
							{
								sym->Add(cls);
							}
							else
							{
								DeleteObj(cls);
							}
						}

						if (sf->Current())
							blk = sf->Current();
					}
					else if (InScope > 0)
					{
						if (sym->Type == SymEnum)
						{
							LSymbol *def = new LSymbol(_FL);
							if (def)
							{
								bool GotEquals = false;
								
								def->File = sf->Path;
								def->Line = blk->GetLine();
								
								while ((t = blk->NextToken()))
								{
									if (CmpToken(t, ","))
										break;
									
									if (CmpToken(t, "}"))
									{
										sym->Add(def);
										goto ProcessEndBracket;
									}

									if (CmpToken(t, "="))
										GotEquals = true;
									else if (GotEquals)							
										def->Tokens.Add(t);
								}
								
								sym->Add(def);
							}
							else
							{
								Msg(MsgError, "%s:%i - Failed to create defn.\n", _FL);
							}
						}
						else
						{
							LSymbol *def = ParseDecl(sf, t);
							if (def)
							{
								if (sf->Active && !def->FriendDecl)
								{
									sym->Add(def);
								}
								else
								{
									DeleteObj(def);
								}
							}
							else
							{
								Msg(MsgError, "%s:%i - Failed to parse defn.\n", _FL);
							}

							if (sf->Current())
								blk = sf->Current();
						}
					}
					else if (InParentList)
					{
					}
					else
					{
						sym->Tokens.Add(t);
					}
				}
			}
			
			if (!Finished)
				blk = sf->Next();
		}
	}
	
	return sym;
}

char16 *FindEol(char16 *s)
{
	char16 *LastNonWhite = NULL;
	while (*s)
	{
		if (!strchr(LWhiteSpace, *s))
			LastNonWhite = s;
		else if (*s == '\n')
		{
			if (!LastNonWhite || *LastNonWhite != '\\')	
				break;
		}
		s++;
	}
	return s;
}

bool HasNonWhiteSpace(char16 *Start, char16 *End)
{
	while (*Start && Start < End)
	{
		if (Start[0] == '/' &&
			Start[1] == '/')
		{
			// Single line comments don't contribute to non-whitespace
			for (Start += 2; Start<End; Start++)
			{
				if (*Start == '\n')
					break;
			}
			if (Start >= End)
				return false;
			else
				continue;
		}
		else if (Start[0] == '/' &&
				 Start[1] == '*')
		{
			// Start of multi-line comment that doesn't contribute to non-whitespace
			for (Start += 2; Start<End; Start++)
			{
				if (Start[0] == '*' &&
					Start[1] == '/')
				{
					Start += 2;
					break;
				}
			}
			if (Start >= End)
				return false;
			else
				continue;
		}
		
		if (!strchr(" \t\r", *Start))
		{
			return true;
		}
		
		Start++;
	}
	return false;
}

bool LCppParserWorker::ParsePreprocessor(LSourceFile *sf)
{
	char16 *t;
	
	PreprocessBlock *blk = sf->Current();
	if (blk->Type == SourceBlock)
	{
		LAssert(0);
		return false;
	}
		
	bool Status = true;
	sf->Lex(*blk);
	
	/*
	LgiTrace("%s:%i - %s, stack=%i\n",
		sf->Path.Get(),
		blk->BlockLine,
		PreprocessSymbolNames[blk->Type],
		sf->Stack.Length());
	*/
	
	switch (blk->Type)
	{
		case HashIf:
		{
			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;

			ps.IfBranchClaimed = Evaluate(blk->Tokens) != 0;
			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;			
			break;
		}
		case HashElif:
		{
			PreprocessState &ps = sf->Stack.Last();
			if (!ps.IfBranchClaimed)
			{
				ps.IfBranchClaimed = Evaluate(blk->Tokens) != 0;
				if (sf->Active)
					sf->Active = ps.IfBranchClaimed;
			}
			else
			{
				sf->Active = false;
			}
			break;
		}
		case HashIfndef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			LSymbol *def = Resolve(t);

			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;
			ps.IfBranchClaimed = def == NULL ||
								(
									def->Type != SymDefineValue &&
									def->Type != SymDefineFunction
								);

			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;
			break;
		}
		case HashIfdef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}
				
			LSymbol *def = Resolve(t);
			PreprocessState &ps = sf->Stack.New();
			ps.ParentIsActive = sf->Active;
			ps.IfBranchClaimed = def != NULL &&
								(
									def->Type == SymDefineValue ||
									def->Type == SymDefineFunction
								);
			if (sf->Active)
				sf->Active = ps.IfBranchClaimed;
			break;
		}
		case HashElse:
		{
			PreprocessState &ps = sf->Stack.Last();
			if (!ps.IfBranchClaimed)
			{
				ps.IfBranchClaimed = true;
				if (ps.ParentIsActive)
					sf->Active = true;
			}
			else
			{
				sf->Active = false;
			}
			break;
		}
		case HashEndif:
		{
			LAssert(sf->Stack.Length() > 0);
			PreprocessState &ps = sf->Stack.Last();
			sf->Active = ps.ParentIsActive;
			sf->Stack.Length(sf->Stack.Length() - 1);
			break;
		}
		case HashPragma:
		case HashError:
		case HashWarning:
		{
			break;
		}
		case HashDefine:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			LSymbol *sym = NULL;
			if (sf->Active)
			{
				sym = Scopes[0]->Define(t, SymDefineValue, _FL);
				sym->File = sf->Path;
				sym->Line = blk->BlockLine;
				if (blk->Tokens.Length() > 1)
				{
					sym->Tokens = blk->Tokens;
					sym->Tokens.DeleteAt(0, true);
				}
				else
				{
					sym->Tokens.Length(0);
				}
				
				if (sym->Tokens.Length() > 0 &&
					CmpToken(sym->Tokens[0], "("))
					sym->Type = SymDefineFunction;
			}
			break;
		}
		case HashUndef:
		{
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}

			LSymbol *s = Scopes[0]->Find(t);
			if (s)
			{
				Scopes[0]->Delete(t);
				delete s;
			}
			break;
		}
		case HashInclude:
		{
			// Include a file
			t = blk->NextToken();
			if (!t)
			{
				Status = false;
				break;
			}
			
			LAutoString FileName8(WideToUtf8(t));
			if (!FileName8)
			{
				Status = false;
				break;
			}

			// Resolve filename
			char p[MAX_PATH_LEN];
			bool Exists = false;
			char *IncPath = NULL;
			if (LIsRelativePath(FileName8))
			{
				LMakePath(p, sizeof(p), sf->Path, "..");
				LMakePath(p, sizeof(p), p, FileName8);
				if ((Exists = LFileExists(p)))
					IncPath = p;
			}
			
			if (!Exists)
			{
				if ((IncPath = FindInclude(FileName8)))
					Exists = true;
			}
			
			if (Exists)
			{
				ParseCpp(IncPath);
			}
			break;
		}
		default:
		{
			LAssert(!"Impl me");
			Status = false;
			break;
		}
	}
	
	sf->Next();	
	return Status;
}

void LCppParserWorker::DoWork(WorkUnit *wk)
{
	w.Reset(wk);
	
	if (w->SearchTerm)
	{
		// Do search...
	}
	else if (w->Source.Length())
	{
		// Parse source code...
		for (int i=0; i<w->Source.Length(); i++)
		{
			char *ext = LGetExtension(w->Source[i]);
			if (!ext)
				continue;
			
			if (!stricmp(ext, "c") ||
				!stricmp(ext, "cpp"))
			{
				InitScopes();
				
				uint64 Start = LCurrentTime();

				if (!stricmp(ext, "cpp"))
				{
					Scopes[0]->Define((char16*)sCpp, SymDefineValue, _FL);
				}	

				ParseCpp(w->Source[i]);
				uint64 Time = LCurrentTime() - Start;
				
				#if 0
				char16 *k;
				for (LSymbol *s = Scopes[0]->First(&k); s; s = Scopes[0]->Next(&k))
				{
					LgiTrace("    %S = %p\n", k, s);
				}
				#endif

				LgiTrace("%i of %i: %s, %i syms, %i ms\n", i, w->Source.Length(), w->Source[i].Get(), Scopes[0]->Length(), (int)Time);
			}
			else
			{
				// No parser impl yet
			}
			
			Srcs.DeleteObjects();
		}
	}
	else LAssert(!"Unknown work unit type.");
}

LCppParser::LCppParser()
{
	d = new LCppParserPriv;
}

LCppParser::~LCppParser()
{
	delete d;
}

void LCppParser::ParseCode
(
	LArray<const char*> &IncludePaths,
	LArray<ValuePair*> &PreDefines,
	LArray<char*> &Source
)
{
	WorkUnit *w = new WorkUnit(IncludePaths, PreDefines, Source);
	if (w)
	{
		if (!d->Worker)
			d->Worker.Reset(new LCppParserWorker(d));
		d->Worker->DoWork(w);
	}
}

void LCppParser::Search(const char *Str, SearchResultsCb Callback, void *CallbackData)
{
	WorkUnit *w = new WorkUnit(Str);
	if (w)
	{
		if (!d->Worker)
			d->Worker.Reset(new LCppParserWorker(d));
		d->Worker->DoWork(w);
	}
}
