/*
Known bugs:


*/
#include "Lgi.h"
#include "ParserCommon.h"

#if 0
// #define DEBUG_FILE		"\\ape-apcp.c"
#define DEBUG_FILE		"apcp\\apcp\\apcp.h"
#define DEBUG_LINE		550
#endif

#define IsWhiteSpace(c) \
	(strchr(WhiteSpace, c) != NULL)
#define IsBracket(c) \
	((c) == '{' || (c) == '}')
#define SkipWs(c) \
	while (*c && strchr(WhiteSpace, *c)) \
	{ \
		if (*c == '\n') \
			Line++; \
		c++; \
	}
#define SkipSymbol(c) \
	while (*c && (IsAlpha(*c) || IsDigit(*c) || strchr("_",*c))) \
		c++;

bool BuildJsDefnList(char *FileName, char16 *Source, GArray<DefnInfo> &Defns, int LimitTo, bool Debug)
{
	if (!Source)
		return false;

	int Depth = 0;
	int Line = 1;
	GString CurClass;

	for (auto *c = Source; *c; c++)
	{
		switch (*c)
		{
			case '/':
				if (c[1] == '/')
				{
					// Single line comment
					c++;
					while (c[1] && c[1] != '\n')
						c++;
				}
				else if (c[1] == '*')
				{
					// Multi-line comment..
					c += 2;
					while (*c)
					{
						if (*c == '\n')
							Line++;
						if (c[0] == '*' && c[1] == '/')
						{
							c++;
							break;
						}
						c++;
					}
				}
				break;
			case '\'':
			case '\"':
			case '`':
				// Skip over string literal
				for (char delim = *c++; *c; c++)
				{
					if (*c == '\n')
						Line++;
					if (*c == '\\')
						c++;
					else if (*c == delim)
						break;
				}				
				break;
			case '{':
				if (Depth == 1 && CurClass)
				{
					auto *Start = c - 1;
					int LocalLine = Line;
					while (Start > Source && !IsBracket(*Start))
					{
						if (*Start == '\n') LocalLine--;
						Start--;
					}
					if (IsBracket(*Start))
					{
						Start++;

						while (*Start && strchr(WhiteSpace, *Start))
						{
							if (*Start == '\n') LocalLine++;
							Start++;
						}

						GString Fn(Start, c - Start);

						auto &d = Defns.New();
						d.Type = DefnFunc;
						d.Name = CurClass + "." + Fn.Strip();
						d.File = FileName;
						d.Line = LocalLine;
					}
				}
				Depth++;
				break;			
			case '}':
				Depth--;
				if (Depth <= 0 && CurClass)
					CurClass.Empty();
				break;
			case 'c':
				if (!Strncmp(c, L"class", 5) &&
					IsWhiteSpace(c[5]))
				{
					c += 5;
					SkipWs(c);
					auto *Start = c;
					SkipSymbol(c);

					auto &d = Defns.New();
					d.Type = DefnClass;
					CurClass = GString(Start, c - Start);
					d.Name = CurClass;
					d.File = FileName;
					d.Line = Line;
				}
				break;
			case 'f':
				if (!Strncmp(c, L"function", 8) &&
					IsWhiteSpace(c[8]))
				{
					c += 8;

					auto &d = Defns.New();
					d.Type = DefnFunc;

					SkipWs(c);
					auto *Start = c;
					SkipSymbol(c);
					d.FnName.Start = 0;
					d.FnName.Len = c - Start;

					int Bracket = 0;
					while (*c)
					{
						if (*c == '\n')
							Line++;
						if (*c == '(') Bracket++;
						else if (*c == ')')
						{
							if (--Bracket <= 0)
							{
								c++;
								break;
							}
						}
						c++;
					}

					GString FnDecl = GString(Start, c - Start);
					d.Name = FnDecl;
					d.File = FileName;
					d.Line = Line;
					c--;
				}
				break;
			case '\n':
				Line++;
				break;
			default:
				break;
		}
	}

	return Defns.Length() > 0;
}

