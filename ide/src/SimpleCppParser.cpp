/*

Future work:
	https://github.com/ccontavalli/sbexr
	Uses clang to index the code, should probably look at doing the same.
	Better quality results.
	Seems to use various clang libs:
		-lclangParse \
		https://clang.llvm.org/doxygen/classclang_1_1Parser.html

Known bugs:

	1) Duplicate brackets in #defines, e.g:

			#if SOME_DEF
				if (expr) {
			#else
				if (different_expr) {
			#endif

		Breaks the bracket counting. Causing the depth to get out of sync with capture...
		Workaround: Move the brackets outside of the #define'd area if possible.

	2) This syntax is wrongly assumed to be a structure:

			struct netconn*
			netconn_alloc(enum netconn_type t, netconn_callback callback)
			{
				...
			}


	Can't find these symbols:
		do_recv (in api_msg.c)
		process_apcp_msg (in ape-apcp.c)
		recv_udp (in api_msg.c)
	Wrong position:
		lwip_select (out by 4 lines?)
		nad_apcp_send

	tcpip_callback_with_block appears 3 times?
	

*/
#include "lgi/common/Lgi.h"
#include "lgi/common/DocView.h"
#include "ParserCommon.h"

// #define DEBUG_FILE			"DragAndDrop.h"
// #define DEBUG_LINE		17
#define PARSE_ALL_FILES		1 // else parse just the 'DEBUG_FILE'

#ifdef DEBUG_FILE
#define DEBUG_LOG(...) if (Debug) do { LgiTrace(__VA_ARGS__); } while (0)
#else
#define DEBUG_LOG(...)
#endif

static bool IsFuncNameChar(char c)
{
	return	strchr("_:=~[]<>-+", c) ||
			IsAlpha(c) ||
			IsDigit(c);
}

const char *toString(DefnType t)
{
	switch (t)
	{
		default:
		case DefnNone:      return "DefnNone";
		case DefnDefine:    return "DefnDefine";
		case DefnFunc:      return "DefnFunc";
		case DefnClass:     return "DefnClass";
		case DefnEnum:      return "DefnEnum";
		case DefnEnumValue: return "DefnEnumValue";
		case DefnTypedef:   return "DefnTypedef";
		case DefnVariable:  return "DefnVariable";
		case DefnScope:     return "DefnScope";
		case DefnExternC:   return "DefnExternC";
	}
}

bool ParseFunction(LRange &Return, LRange &Name, LRange &Args, const char *Defn)
{
	if (!Defn)
		return false;

	int Depth = 0;
	const char *c, *Last = nullptr;
	for (c = Defn; *c; c++)
	{
		if (*c == '(')
		{
			if (Depth == 0)
				Last = c;
			Depth++;
		}
		else if (*c == ')')
		{
			if (Depth > 0)
				Depth--;
			else
			{
				LgiTrace("%s:%i - Fn parse error '%s' (%i)\n", _FL, Defn, (int)(c - Defn));
				return false;
			}
		}
	}

	if (!Last)
		return false;

	Args.Start = Last - Defn;
	Args.Len = c - Last;

	while (Last > Defn &&
		IsWhite(Last[-1]))
		Last--;

	while (Last > Defn &&
		IsFuncNameChar(Last[-1]))
		Last--;

	Return.Start = 0;
	Return.Len = Last - Defn;
	Name.Start = Last - Defn;
	Name.Len = Args.Start - Name.Start;

	if (Name.Len == 0 ||
		Args.Len == 0)
	{
		/*
		LgiTrace("%s:%i - Fn parse empty section '%s' (%i,%i,%i)\n", _FL, Defn,
			(int)Return.Len,
			(int)Name.Len,
			(int)Args.Len);
		*/
		return false;
	}

	return true;
}

struct CppContext
{
	constexpr static const char *WhiteSpace		= " \t\r\n";

	const char *FileName = nullptr;
	int LineNum = 1;
	int LimitTo = DefnNone;
	bool Debug = false;
	char16 *LastDecl = nullptr;
	#ifdef DEBUG_LINE
	int PrevLine = 0;
	#endif

	bool EmitDefn(DefnType t) const
	{
		return	LimitTo == DefnNone ||
				(LimitTo & t) != 0;
	}

	struct TScope
	{
		// The type of scope starting
		DefnType type;
		
		// Line that the scope started on (1..lines)
		int line;

		// Ptr to source at the start of the scope (for debugging)
		char16 *source;

		TScope()
			: type(DefnNone)
			, line(1)
			, source(nullptr)
		{}

		TScope(DefnType t, int l, char16 *s)
			: type(t)
			, line(l)
			, source(s)
		{}

		LString toString() const
		{
			return LString::Fmt("%s:%i", ::toString(type), line);
		}
	};

	LArray<TScope> Scopes;

	// Array of classes, from outer to inner scope
	LString::Array ClassNames;

	bool IsEnum = 0, IsClass = false, IsStruct = false;
	bool FnEmit = false;	// don't emit functions between a f(n) and the next '{'
							// they are only parent class initializers
	LArray<int> ConditionalIndex;
	int ConditionalDepth = 0;
	bool ConditionalFirst = true;
	bool ConditionParsingErr = false;

	// Get the full class name, including parent classes
	LString curClass()
	{
		return LString("::").Join(ClassNames);
	};

	// Print a list of scopes for debugging
	LString curScope()
	{
		LString::Array a;
		for (auto &s: Scopes)
			a.New() = s.toString();
		auto desc = LString(">").Join(a);
		return LString::Fmt("%i(%s)", (int)Scopes.Length(), desc?desc.Get():"");
	};

	DefnType lastScope()
	{
		if (Scopes.Length())
			return Scopes.Last().type;
		return DefnNone;
	}

	bool popScope()
	{
		if (Scopes.Length() == 0)
			return false;

		auto s = Scopes.PopLast();
		if (s.type == DefnClass)
		{
			if (ClassNames.Length())
			{
				auto cls = ClassNames.PopLast();
				DEBUG_LOG("%s:%i - removing class name: %s\n", _FL, cls.Get());
			}
			else
			{
				DEBUG_LOG("%s:%i - popping class scope without class name?\n", _FL);
				return false;
			}
		}

		return true;
	}

	bool IsFirst(LArray<int> &a, int depth)
	{
		if (depth == 0)
			return true;

		for (int i=0; i<depth; i++)
			if (a[i] > 1)
				return false;

		return true;
	}

	bool SeekPtr(char16 *&s, char16 *end, int &Line)
	{
		if (s > end)
		{
			LAssert(!"seek pointer is after the end of buffer");
			return false;
		}

		while (s < end)
		{
			if (*s == '\n')
				Line++;
			s++;
		}

		return true;
	}

	DefnType GuessDefnType(LString &def, bool debug)
	{
		// In the context of a class member, this could be a variable defn:
		//
		//		bool myVar = true;
		//		bool myVar = someFn();
		//
		// or a function defn:
		//
		//		bool myFunction(int someArg = 10);
		//
		// Try to guess which it is:

		int roundDepth = 0;
		int equalsAtZero = 0;
		for (auto s = def.Get(); *s; s++)
		{
			if (*s == '(')
				roundDepth++;
			else if (*s == ')')
				roundDepth--;
			else if (*s == '=')
			{
				if (roundDepth == 0)
					equalsAtZero++;
			}
		}

		if (def.Find("operator") >= 0)
			return DefnFunc;

		return equalsAtZero ? DefnVariable : DefnFunc;
	}

	template<typename A, typename B>
	bool Compare(A *doc,
				B *match,
				// true if you need both strings to fully match, ie NULL terminated token
				// false if matching a string in a longer body of text that is not NULL terminated
				bool fullMatch)
	{
		if (!doc || !match)
		{
			return false;
		}
	
		while (*match && *doc == *match)
		{
			doc++;
			match++;
		}

		return (!fullMatch || !*doc) && !*match;
	}

	struct ClassInfo
	{
		bool isTemplate = false;
		char16 *templateStart = nullptr;
		char16 *templateEnd = nullptr;
		char16 *typeName = nullptr; // 'struct', 'class' etc
		char16 *className = nullptr;
		char16 *colon = nullptr;
		char16 *body = nullptr;

		LString name;

		operator bool() const
		{
			return typeName && body && *body == '{' && name;
		}
	};

	ClassInfo ParseClassDefn(char16 *&n)
	{
		ClassInfo c;

		char16 *t;
		LString::Array parts;
	
		while (n && *n)
		{
			skipws(n);
			
			if (*n == '#')
			{
				// Skip preprocessor command?
				if (auto eol = Strchr(n, '\n'))
				{
					DEBUG_LOG("%s: skip preproc='%.*S'\n", __func__, (int)(eol-n), n);
					n = eol + 1;
				}
			}
			
			char16 *startTok = n;
			if (!(t = LexCpp(n, LexStrdup)))
			{
				LgiTrace("%s:%i - LexCpp failed at %s:%i.\n", _FL, FileName, LineNum);
				break;
			}

			DEBUG_LOG("%s: t='%S' @ line %i\n", __func__, t, Line+1);

			if (Compare(t, "template", true))
			{
				c.isTemplate = true;
				skipws(n);
				if (*n == '<')
				{
					c.templateStart = n++;

					// Parse over the template parameters
					while ((t = LexCpp(n, LexStrdup)))
					{
						bool end = Compare(t, ">", true);
						// DEBUG_LOG("%s: template tok '%S' end=%i.\n", __func__, t, end);
						DeleteArray(t);
						if (end)
						{
							c.templateEnd = n;
							break;
						}
					}

					// DEBUG_LOG("%s: template ended.\n", __func__);
					if (!c.templateEnd)
					{
						// Error: no template end found...
						DEBUG_LOG("%s: error: no template end found: %.32S\n", __func__, startTok);
						break;
					}
				}
				else
				{
					DEBUG_LOG("%s: error: no start template, n='%.20S'.\n", __func__, n);
					break;
				}
			}
			else if (Compare(t, ";", true) ||
					 Compare(t, "friend", true))
			{
				DeleteArray(t);
				break;
			}
			else if (Compare(t, ":", true))
			{
				c.colon = startTok;
				DeleteArray(t);
			}
			else if (Compare(t, "LgiClass", true) ||
					 Compare(t, "ScribeClass", true))
			{
				// Ignore these...
				DeleteArray(t);
			}
			else if (Compare(t, "class", true) ||
					 Compare(t, "struct", true))
			{
				c.typeName = startTok;
			}
			else if (Compare(t, "{", true))
			{
				skipws(startTok);
				c.body = startTok;
				DeleteArray(t);

				c.name = LString("").Join(parts);

				DEBUG_LOG("%s: class name='%s' @ line %i\n", __func__, c.name.Get(), Line+1);
				break;
			}
			else if (c.typeName && !c.colon)
			{
				parts.Add(t);
			}
			else
			{
				DeleteArray(t);
			}
		}

		return c;
	}

	bool Parse(char16 *Cpp, LArray<DefnInfo> &Defns, LError &err)
	{	
		char16 *s = Cpp;
		LastDecl = s;

		#ifdef DEBUG_FILE
		Debug = !Stricmp(LGetLeaf(FileName), DEBUG_FILE);
		#endif
		#if !PARSE_ALL_FILES
		if (!Debug)
			return true;
		#endif

		while (s && *s)
		{
			// skip ws
			while (*s && strchr(" \t\r", *s)) s++;
		
			#ifdef DEBUG_LINE
			if (Debug)
			{
				if (Line >= DEBUG_LINE - 1)
				{
					int asd=0;
				}
				else if (PrevLine != Line)
				{
					PrevLine = Line;
					// LgiTrace("%s:%i '%.10S'\n", FileName, Line + 1, s);
				}
			}
			#endif

			// tackle decl
			switch (*s)
			{
				case 0:
					break;
				case '\n':
				{
					LineNum++;
					s++;
					break;
				}
				case '#':
				{
					const char16 *Hash = s;
				
					s++;
					skipws(s)

					const char16 *End = s;
					while (*End && IsAlpha(*End))
						End++;

					if
					(
						EmitDefn(DefnDefine)
						&&
						(End - s) == 6
						&&
						Compare(s, "define", false)
					)
					{
						s += 6;
						defnskipws(s);
						LexCpp(s, LexNoReturn);

						char16 r = *s;
						*s = 0;					
						Defns.New().Set(DefnDefine, FileName, Hash, LineNum);
						*s = r;
					
					}

					char16 *Eol = Strchr(s, '\n');
					if (!Eol) Eol = s + Strlen(s);

					// bool IsIf = false, IsElse = false, IsElseIf = false;
					if
					(
						((End - s) == 2 && !Strncmp(L"if", s, End - s))
						||
						((End - s) == 5 && !Strncmp(L"ifdef", s, End - s))
						||
						((End - s) == 6 && !Strncmp(L"ifndef", s, End - s))
					)
					{
						ConditionalIndex[ConditionalDepth] = 1;
						ConditionalDepth++;
						ConditionalFirst = IsFirst(ConditionalIndex, ConditionalDepth);
						if (Debug)
							LgiTrace("%s:%i - ConditionalDepth++=%i Line=%i: %.*S\n", _FL, ConditionalDepth, LineNum, Eol - s + 1, s - 1);
					}
					else if
					(
						((End - s) == 4 && !Strncmp(L"else", s, End - s))
						||
						((End - s) == 7 && !Strncmp(L"else if", s, 7))
					)
					{
						if (ConditionalDepth <= 0 &&
							!ConditionParsingErr)
						{
							ConditionParsingErr = true;
							LgiTrace("%s:%i - Error parsing pre-processor conditions: %s:%i\n", _FL, FileName, LineNum);
						}

						if (ConditionalDepth > 0)
						{
							ConditionalIndex[ConditionalDepth-1]++;
							ConditionalFirst = IsFirst(ConditionalIndex, ConditionalDepth);
							if (Debug)
								LgiTrace("%s:%i - ConditionalDepth=%i Idx++ Line=%i: %.*S\n", _FL, ConditionalDepth, LineNum, Eol - s + 1, s - 1);
						}
					}
					else if
					(
						((End - s) == 5 && !Strncmp(L"endif", s, End - s))
					)
					{
						if (ConditionalDepth <= 0 &&
							!ConditionParsingErr)
						{
							ConditionParsingErr = true;
							// LgiTrace("%s:%i - Error parsing pre-processor conditions: %s:%i\n", _FL, FileName, Line+1);
						}

						if (ConditionalDepth > 0)
							ConditionalDepth--;
						ConditionalFirst = IsFirst(ConditionalIndex, ConditionalDepth);
						if (Debug)
							LgiTrace("%s:%i - ConditionalDepth--=%i Line=%i: %.*S\n", _FL, ConditionalDepth, LineNum, Eol - s + 1, s - 1);
					}

					while (*s)
					{
						if (*s == '\n')
						{
							// could be end of # command
							char Last = (s[-1] == '\r') ? s[-2] : s[-1];
							if (Last != '\\') break;
							LineNum++;
						}

						s++;
						LastDecl = s;
					}
					break;
				}
				case '\"':
				case '\'':
				{
					char16 Delim = *s;
					s++;
					while (*s)
					{
						if (*s == Delim) { s++; break; }
						if (*s == '\\') s++;
						if (*s == '\n') LineNum++;
						s++;
					}
					break;
				}
				case '{':
				{
					s++;
					if (ConditionalFirst)
						Scopes.Add({DefnScope, LineNum, s});
					FnEmit = false;
					LastDecl = s;
					#ifdef DEBUG_FILE
					if (Debug)
						LgiTrace("%s:%i - FnEmit=%i Scope=%s @ line %i\n", _FL, FnEmit, curScope().Get(), Line+1);
					#endif				
					break;
				}
				case '}':
				{
					s++;

					if (ConditionalFirst)
					{
						if (Scopes.Length() > 0)
						{
							popScope();
							DEBUG_LOG("%s:%i - Scope=%s @ line %i\n", _FL, curScope().Get(), Line+1);
						}
						else
						{
							DEBUG_LOG("%s:%i - ERROR Depth zero @ line %i\n", _FL, Line+1);
						}

					}

					defnskipws(s);
					LastDecl = s;
					if (IsEnum)
					{
						if (IsAlpha(*s)) // typedef'd enum?
						{
							LAutoWString t(LexCpp(s, LexStrdup));
							if (t)
								Defns.New().Set(DefnEnum, FileName, t.Get(), LineNum);
						}

						IsEnum = false;
					}
					break;
				}
				case ';':
				{
					LastDecl = ++s;
				
					/*
					if (Scopes.Length() == 0 && ClassNames.Length())
					{
						// Check for typedef struct name
						char16 *Start = s - 1;
						while (Start > Cpp && Start[-1] != '}')
							Start--;
						auto TypeDef = LString(Start, s - Start - 1).Strip();
						if (TypeDef.Length() > 0)
						{
							if (EmitDefn(DefnClass))
							{
								Defns.New().Set(DefnClass, FileName, TypeDef, Line + 1);
							}
						}

						// End the class def
						if (Scopes.Length() > 0)
						{
							auto d = Scopes.PopLast();
							if (d.type == DefnClass)
								ClassNames.PopLast();
						}
						DEBUG_LOG("%s:%i - Scope=%s @ line %i\n", _FL, curScope().Get(), Line+1);
					}
					*/
					break;
				}
				case '/':
				{
					s++;
					if (*s == '/')
					{
						// one line comment
						while (*s && *s != '\n') s++;
						LastDecl = s;
					}
					else if (*s == '*')
					{
						// multi line comment
						s++;
						while (*s)
						{
							if (s[0] == '*' && s[1] == '/')
							{
								s += 2;
								break;
							}

							if (*s == '\n') LineNum++;
							s++;
						}
						LastDecl = s;
					}
					break;
				}
				case '(':
				{
					s++;

					auto context = lastScope();
					if (context != DefnScope &&
						context != DefnFunc &&
						!FnEmit &&
						LastDecl &&
						ConditionalFirst)
					{
						// function?
					
						// find start:
						char16 *Start = LastDecl;
						skipws(Start);
						if (Strnstr(Start, L"__attribute__", s - Start))
							break;
					
						// find end (matching closing bracket)
						int RoundDepth = 1;
						char16 *End = s;
						while (*End)
						{
							if (End[0] == '/' &&
								End[1] == '/')
							{
								if (auto nl = Strchr(End, '\n'))
									End = nl;
								else
									break;
							}
							else if (*End == '(')
							{
								RoundDepth++;
							}
							else if (*End == ')')
							{
								if (--RoundDepth == 0)
								{
									End++;
									break;
								}
							}
							else if (*End == '{' ||
									 *End == ';')
							{
								break;
							}

							End++;
						}

						if (End && *End)
						{
							if (auto Buf = LString(Start, End-Start))
							{
								DEBUG_LOG("	Buf='%s' line=%i context=%s\n", Buf.Get(), Line, TypeToStr(context));

								// remove new-lines
								auto Out = Buf.Get();
								for (auto In = Buf.Get(); *In;)
								{
									if (*In == '\r' || *In == '\n' || *In == '\t' || *In == ' ')
									{
										*Out++ = ' ';
										skipws(In);
									}
									else if (In[0] == '/' && In[1] == '/')
									{
										In = Strchr(In, '\n');
										if (!In)
											break;
									}
									else
									{
										*Out++ = *In++;
									}
								}
								Buf.Length(Out - Buf.Get());

								// DEBUG_LOG("	Buf='%s'\n", Buf.Get());
								if (ClassNames.Length())
								{
									auto pos = Buf.Find("(");
									if (pos >= 0)
									{
										auto opPos = Buf.Find("operator");
										if (opPos >= 0 && opPos < pos)
										{
											pos = opPos;
										}
										else
										{
											// Skip whitespace between name and '('
											while
											(
												pos > 0
												&&
												strchr(WhiteSpace, Buf(pos-1))
											)
											{
												pos--;
											}

											// Skip over to the start of the name..
											while
											(
												pos > 0
												&&
												Buf(pos-1) != '*'
												&&
												Buf(pos-1) != '&'
												&&
												!strchr(WhiteSpace, Buf(pos-1))
											)
											{
												pos--;
											}
										}

										// 'b' now points to the start of the function
										// now we add 'CurClassDecl' and '::' before the function name,
										LString before = Buf(0, pos);
										LString after = Buf(pos, -1);
										Buf = before + curClass() + "::" + after;
										DEBUG_LOG("	ClsBuf='%s'\n", Buf.Get());
									}
								}

								// cache f(n) def
								auto Type = GuessDefnType(Buf, Debug);
								if
								(
									EmitDefn(Type)
									&&
									*Buf != ')'
								)
									Defns.New().Set(Type, FileName, Buf, LineNum);

								while (*End && !strchr(";:{#", *End))
									End++;

								SeekPtr(s, End, LineNum);

								switch (*End)
								{
									case ';':
									{
										// Forward definition
										break;
									}
									case '{':
									{
										// Has a function implementation, so consume the starting bracket and
										// emit a new function scope...
										Scopes.Add({DefnFunc, LineNum, s++});
										break;
									}
									default:
									{
										break;
									}
								}
								#if 0 // def DEBUG_FILE
								DEBUG_LOG("%s:%i - FnEmit=%i Depth=%i @ line %i\n", _FL, FnEmit, Depth, Line+1);
								#endif
							}
						}
					}
					else
					{
						#ifdef DEBUG_FILE
						if (Debug)
							LgiTrace("%s:%i - Not attempting fn parse: Scope=%s, fnEmit=%i, CondFirst=%i, %s:%i:%.20S\n",
								_FL, curScope().Get(), FnEmit, ConditionalFirst,
								LGetLeaf(FileName), Line, s-1);
						#endif
					}
					break;
				}
				default:
				{
					bool InTypedef = false;

					if (IsAlpha(*s) || IsDigit(*s) || *s == '_')
					{
						char16 *Start = s;
					
						s++;
						defnskipsym(s);
					
						auto TokLen = s - Start;
						if (TokLen == 6 && Compare(Start, "extern", false))
						{
							// extern "C" block?
							LAutoWString t(LexCpp(s, LexStrdup));
							if (Compare(t.Get(), "\"C\"", true))
							{
								defnskipws(s);
								if (*s == '{')
								{
									Scopes.Add({DefnExternC, LineNum, Start});
									s++;
								}
							}
						}
						else if (
							(TokLen == 5 && Compare(Start, "const", false)) ||
							(TokLen == 9 && Compare(Start, "constexpr", false))
						)
						{
							// const/constexpr
							skipws(s);

							auto n = s;
							LArray<char16*> parts;
							ssize_t equals = -1;
							while (auto t = LexCpp(n, LexStrdup))
							{
								if (!Stricmp(t, L";"))
								{
									DeleteArray(t);
									break;
								}

								if (!Stricmp(t, L"="))
									equals = parts.Length();
								parts.Add(t);
							}

							if (equals > 0 && parts.Length() > 0)
							{
								auto var = parts[equals-1];

								if (ClassNames.Length())
								{
									// Class scope var?
									LString::Array a = ClassNames;
									a.Add(var);
									auto full = LString("::").Join(a);
									Defns.New().Set(DefnVariable, FileName, full, LineNum);
								}
								else
								{
									// Global scope var?
									Defns.New().Set(DefnVariable, FileName, var, LineNum);
								}
							}

							parts.DeleteArrays();
						}
						else if (TokLen == 7 && Compare(Start, "typedef", false))
						{
							// Typedef
							skipws(s);
						
							IsStruct = Compare(s, "struct", false);
							IsClass  = Compare(s, "class", false);
							if (IsStruct || IsClass)
							{
								Start = s;
								InTypedef = true;
								goto DefineStructClass;
							}

							IsEnum = Compare(s, "enum", false);
							if (IsEnum)
							{
								Start = s;
								s += 4;
								goto DefineEnum;
							}
						
							LStringPipe p;
							char16 *i;
							for (i = Start; i && *i;)
							{
								switch (*i)
								{
									case ' ':
									case '\t':
									{
										p.Push(Start, i - Start);
									
										defnskipws(i);

										char16 sp[] = {' ', 0};
										p.Push(sp);

										Start = i;
										break;
									}
									case '\n':
										LineNum++;
										// fall thru
									case '\r':
									{
										p.Push(Start, i - Start);
										i++;
										Start = i;
										break;
									}
									case '{':
									{
										p.Push(Start, i - Start);
									
										int Depth = 1;
										i++;
										while (*i && Depth > 0)
										{
											switch (*i)
											{
												case '{':
													Depth++;
													break;
												case '}':
													Depth--;
													break;
												case '\n':
													LineNum++;
													break;
											}
											i++;
										}
										Start = i;
										break;
									}
									case ';':
									{
										p.Push(Start, i - Start);
										s = i;
										i = 0;
										break;
									}
									default:
									{
										i++;
										break;
									}
								}
							}
						
							if (auto Typedef = p.NewStrW())
							{
								if (EmitDefn(DefnTypedef))
								{
									Defns.New().Set(DefnTypedef, FileName, Typedef, LineNum);
								}
								DeleteArray(Typedef);
							}
						}
						else if
						(
							(
								TokLen == 5
								&&
								(IsClass = Compare(Start, "class", false))
							)
							||
							(
								TokLen == 6
								&&
								(IsStruct = Compare(Start, "struct", false))
							)
						)
						{
							DefineStructClass:

							// Class / Struct
							if (lastScope() == DefnScope)
							{
								DEBUG_LOG("%s:%i - CLASS/STRUCT defn: not capturing, Scopes=%s @ line %i\n",
											_FL, curScope().Get(), Line+1);
							}
							else
							{
								// Check if this is really a class/struct definition or just a reference
								char16 *next = LastDecl;
								auto classDef = ParseClassDefn(next);
								if (classDef)
								{
									// Full definition
									Scopes.Add({DefnClass, LineNum, s});

									DEBUG_LOG("%s:%i - CLASS/STRUCT defn: Scopes=%s @ line %i\n",
											_FL, curScope().Get(), Line+1);
								
									ClassNames.Add(classDef.name);
								
									if (EmitDefn(DefnClass))
									{
										Defns.New().Set(DefnClass, FileName, curClass(), LineNum);
									}

									if (s < classDef.body)
									{
										SeekPtr(s, classDef.body + 1/*seek past the colon, we have emitted the class scope*/, LineNum);
										LastDecl = s;
										DEBUG_LOG("post class s='%s'\n",
											LString::Escape(LString(s, 20)).Get()
											);
									}
									else
									{
										DEBUG_LOG("class bad ptr: body='%s'  s='%s'\n",
											LString::Escape(LString(classDef.body, 20)).Get(),
											LString::Escape(LString(s, 20)).Get()
											);
									}

									DEBUG_LOG("	class='%s' @ line %i\n", curClass().Get(), Line+1);
								}
								else if (InTypedef)
								{
									// Typedef'ing some other structure...
									// char16 *Start = s;
									LexCpp(s, LexNoReturn);
									defnskipws(s);

									LArray<char16*> a;
									char16 *t;
									ssize_t StartRd = -1, EndRd = -1;
									while ((t = LexCpp(s, LexStrdup)))
									{
										if (StartRd < 0 && !StrcmpW(t, L"("))
											StartRd = a.Length();
										else if (EndRd < 0 && !StrcmpW(t, L")"))
											EndRd = a.Length();

										if (Compare(t, ";", true))
											break;
										a.Add(t);
									}

									if (a.Length())
									{
										auto iName = StartRd > 0 && EndRd > StartRd ? StartRd - 1 : a.Length() - 1;
										auto sName = a[iName];
										Defns.New().Set(DefnTypedef, FileName, sName, LineNum);
										a.DeleteArrays();
									}
								}
								else
								{
									#if 1
									DEBUG_LOG("%s:%i - CLASS/STRUCT no defn: type='%.5S' body='%.5S' name='%s'\n",
												_FL,
												classDef.typeName,
												classDef.body,
												classDef.name.Get());
									// return false;
									#endif
								}
							}
						}
						else if
						(
							TokLen == 4
							&&
							Compare(Start, "enum", false)
						)
						{
							DefineEnum:

							IsEnum = true;
							defnskipws(s);
						
							LAutoWString t(LexCpp(s, LexStrdup));
							if (t && isalpha(*t))
							{
								Defns.New().Set(DefnEnum, FileName, t.Get(), LineNum);
							}
						}
						else if (IsEnum)
						{
							char16 r = *s;
							*s = 0;
							Defns.New().Set(DefnEnumValue, FileName, Start, LineNum);
							*s = r;
							defnskipws(s);
							if (*s == '=')
							{
								s++;
								while (true)
								{
									defnskipws(s);
									defnskipsym(s);
									defnskipws(s);
									if (*s == 0 || *s == ',' || *s == '}')
										break;
									LAssert(*s != '\n');
									s++;
								}
							}
						}
					
						if (s[-1] == ':')
						{
							LastDecl = s;
						}
					}
					else
					{
						s++;
					}

					break;
				}
			}
		}
	
		if (Debug)
		{
			for (unsigned i=0; i<Defns.Length(); i++)
			{
				DefnInfo *def = &Defns[i];
				LgiTrace("    %s: %s:%i %s\n", toString(def->Type), def->File.Get(), def->Line, def->Name.Get());
			}
		}
	
		return true;
	}

	CppContext(const char *fileName, int limitTo, bool debug)
		: FileName(fileName)
		, LimitTo(limitTo)
		, Debug(debug)
	{
	}
};

bool BuildCppDefnList(const char *FileName, char16 *Cpp, LArray<DefnInfo> &Defns, int LimitTo, LError &err, bool Debug)
{
	if (!Cpp)
		// An empty file isn't an error, but it has no symbols.
		return true;

	CppContext context(FileName, LimitTo, Debug);
	return context.Parse(Cpp, Defns, err);
}

