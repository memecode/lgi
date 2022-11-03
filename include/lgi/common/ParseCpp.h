#ifndef _GPARSECPP_H_
#define _GPARSECPP_H_

/// C++ symbol types
enum GSymbolType
{
	SymNone,
	SymDefineValue,
	SymDefineFunction,
	SymTypedef,
	SymUnion,
	SymClass,
	SymStruct,
	SymEnum,
	SymFunction,
	SymVariable,
	SymExternC
};

/// A reference to a matched symbol
struct GSymbolResult
{
	/// The type of the symbol
	GSymbolType Type;
	/// Which file the symbol is in
	const char *File;
	/// The line number in that file
	int Line;
	/// The quality of the match (higher is better).
	int MatchQuality;
};

/// A parser library for C++ code.
class LCppParser
{
	struct GCppParserPriv *d;

public:
	typedef void (*SearchResultsCb)(void *CallbackData, LArray<GSymbolResult*> &Syms);
	
	struct ValuePair
	{
		LAutoString Var, Val;
		
		ValuePair(const char *var = NULL, const char *val = NULL)
		{
			Var.Reset(NewStr(var));
			Val.Reset(NewStr(val));
		}
		
		ValuePair(const ValuePair &p)
		{
			Var.Reset(NewStr(p.Var));
			Val.Reset(NewStr(p.Val));
		}
	};
	
	LCppParser();
	virtual ~LCppParser();
	
	/// Parse a group of source code files.
	void ParseCode
	(
		/// All the include paths for this group of source code files
		LArray<const char*> &IncludePaths,
		/// Any pre-defined values
		LArray<ValuePair*> &PreDefines,
		/// All the source code files to parse
		LArray<char*> &Source
	);
	
	/// Do a search for a symbol
	void Search
	(
		/// The search string, can be multiple parts broken by spaces. Sub-strings will
		/// match longer idenifiers and searches are case sensitive
		const char *Str,
		/// A callback to receive results.
		SearchResultsCb Callback,
		/// A user defined value to pass back to the Callback.
		void *CallbackData
	);
};

#endif