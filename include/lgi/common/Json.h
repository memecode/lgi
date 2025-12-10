#ifndef _LJSON_H_
#define _LJSON_H_

#include "lgi/common/Array.h"
#include "lgi/common/StringClass.h"

#define SkipWs(s) while (*s && strchr(" \t\r\n", *s)) s++;

class LJson
{
	constexpr static const char *EscapeChars = "\"\r\n\t\b";

	struct Key
	{
		// Option name for value
		LString Name;

		// One of these should be valid:			
			// 1) A string value
			LString Str;
			// 2) An object definition
			LArray<Key> Obj;
			// 3) An array of something
			LArray<Key> Array;
		// end

		Key *Get(const char *name, bool create = false)
		{
			if (Name.Equals(name))
				return this;
			for (unsigned i=0; i<Obj.Length(); i++)
			{
				if (Obj[i].Get(name))
					return Obj.AddressOf(i);
			}
			if (create)
			{
				char *ArrStart = strchr(name, '[');
				char *ArrEnd = ArrStart ? strchr(ArrStart+1, ']') : NULL;
				if (ArrStart && ArrEnd)
				{
					int Idx = atoi(ArrStart + 1);
					if (Idx < 0)
						return NULL;

					LString n;
					n.Set(name, ArrStart - name);
					if (n.IsEmpty())
					{
						return &Array[Idx];
					}

					for (unsigned i=0; i<Obj.Length(); i++)
					{
						if (Obj[i].Get(n))
							return &(Obj[i].Array[Idx]);
					}

					Key &k = Obj.New();
					k.Name = n;
					return &k.Array[Idx];
				}

				Key &k = Obj.New();
				k.Name = name;
				return &k;
			}
			return NULL;
		}

		void Empty()
		{
			Name.Empty();
			Str.Empty();
			Obj.Empty();
			Array.Empty();
		}

		LString MakeIndent(int d)
		{
			return LString(" ") * (d<<1);
		}
		
		LString Print(int Depth = 0)
		{
			LStringPipe r(512);
			LString indent = MakeIndent(Depth);

			if (Name)
			{
				r.Print("%s\"%s\": ", indent.Get(), Name.Get());
			}
			if (Str)
			{
				auto q = LString::Escape(Str, Str.Length(), EscapeChars, 'u');
				r.Print("\"%s\"", q.Get());
			}

			if (Obj.Length())
			{
				r.Print("%s{\n", indent.Get());
				Depth++;
				
				for (size_t i=0; i<Obj.Length(); i++)
				{
					if (i)
						r.Print(",\n");

					auto &e = Obj[i];
					r.Write(e.Print(Depth));
				}
				
				r.Print("\n%s}", indent.Get());
				Depth--;
			}
			else if (!Str)
			{
				// bool isObjArray = Array.Length() ? Array[0].Obj.Length() > 0 : false;

				if (Array.Length() == 0)
				{
					r.Write("[]", 2);
				}
				else if (Array[0].Obj.Length())
				{
					// Print array of objects
					r.Print("[\n");
					for (size_t i=0; i<Array.Length(); i++)
					{
						if (i) r.Print(",\n");
						r.Write(Array[i].Print(Depth+1));
					}
					r.Print("\n%s]", indent.Get());
				}
				else if (Array.Length() == 1)
				{
					// Print single string
					r.Print("[ ");
					r.Write(Array[0].Print(0));
					r.Print(" ]");
				}
				else
				{
					// Pring multiple strings, each on their own line.
 					r.Write("[", 1);
					auto arrIndent = MakeIndent(Depth + 1);
					for (unsigned i=0; i<Array.Length(); i++)
					{
						r.Print("\n%s", arrIndent.Get());
						auto &e = Array[i];
						r.Write(e.Print(Depth));
					}
					r.Print("\n%s]", indent.Get());
				}
			}

			return r.NewLStr();
		}

		Key *Deref(LString Addr, bool Create)
		{
			LString::Array p = Addr.SplitDelimit(".");
			Key *k = this;
			for (unsigned i=0; k && i<p.Length(); i++)
			{
				auto n = p[i];
				k = k->Get(n, Create);
			}
			return k;
		}
	};

	Key Root;
	const char *Start;

	bool ParseString(LString &s, const char *&c)
	{
		SkipWs(c);

		if (*c != '\"')
			return false;

		c++;
		const char *e = c;
		while (*e)
		{
			if (*e == '\\')
				e += 2;
			else if (*e == '\"')
				break;
			else
				e++;
		}
		if (*e != '\"')
			return false;
		
		s = LString::UnEscape(c, e - c);
				
		c = e + 1;
		return true;
	}

	bool ParseChar(char ch, const char *&c)
	{
		SkipWs(c);
		if (*c != ch)
			return false;
		c++;
		SkipWs(c);
		return true;
	}

	bool IsNumeric(char s)
	{
		return IsDigit(s) || strchr("-.e", s) != NULL;
	}

	bool ParseArray(LArray<Key> &Array, const char *&c)
	{
		if (*c != '[')
		{
			LAssert(0);
			return false;
		}

		c++;
		SkipWs(c);
		while (*c != ']')
		{
			auto Idx = Array.Length();
			Key &a = Array[Idx];
			if (!Parse(a, c))
			{
				Array.Length(Idx); // Delete failed object
				return false;
			}

			SkipWs(c);
			if (*c == ',')
			{
				c++;
				SkipWs(c);
			}
		}
		if (*c != ']')
		{
			LAssert(!"Unexpected token.");
			return false;
		}

		c++;
		return true;
	}

	// Value parser: can be one of:
	// - Object
	// - Array
	// - String
	// - Number
	// - Boolean (true or false)
	// - NULL
	bool Parse(Key &k, const char *&c)
	{
		SkipWs(c);

		if (*c == '\"')
		{
			// String
			return ParseString(k.Str, c);
		}
		else if (*c == '{')
		{
			// Objects
			c++;
			SkipWs(c);
			while (*c && *c != '}')
			{
				LString n;
				if (!ParseString(n, c))
					return false;
				if (!ParseChar(':', c))
					return false;
				
				auto Idx = k.Obj.Length();
				auto &Member = k.Obj[Idx];
				Member.Name = n;
				if (!Parse(Member, c))
				{
					k.Obj.Length(Idx); // Delete incomplete obj.
					return false;
				}

				SkipWs(c);
				if (*c == ',')
				{
					c++;
					SkipWs(c);
				}
			}

			if (*c == '}')
			{
				c++;
				return true;
			}
		}
		else if (*c == '[')
		{
			// Parse array
			return ParseArray(k.Array, c);
		}
		else if (IsNumeric(*c))
		{
			const char *e = c;
			while (*e && IsNumeric(*e))
				e++;
			k.Str.Set(c, e - c);
			c = e;
			return true;
		}
		else if (IsAlpha(*c))
		{
			// Boolean?
			const char *e = c;
			while (*e && IsAlpha(*e))
				e++;
			k.Str.Set(c, e - c);
			c = e;
			return true;
		}
		else if (!*c)
			return true;

		return false;
	}

	Key *Deref(LString Addr, bool Create)
	{
		return Root.Deref(Addr, Create);
	}

public:
	LJson(const char *c = NULL)
	{
		Start = NULL;
		if (c)
			SetJson(c);
	}

	void Empty()
	{
		Root.Empty();
	}

	bool SetJson(const char *c)
	{
		Empty();
		if (!c)
			return false;

		Start = c;
		bool b = Parse(Root, c);
		if (!b)
			LgiTrace("%s:%i - Error at char " LPrintfSizeT "\n", _FL, c - Start);
		return b;
	}

	LString GetJson()
	{
		LString s = Root.Print(0);
		#ifdef _DEBUG
		LAssert(LIsUtf8(s));
		#endif
		return s;
	}

	LString Get(LString Addr)
	{
		Key *k = Deref(Addr, false);
		return k ? k->Str : LString();
	}

	struct Pair
	{
		LString key;
		LString value;

		Pair() { }
		Pair(LString k, LString v) : key(k), value(v) { }
	};

	class Iter
	{
		LJson *j;
		LArray<Key> *a;

	public:
		struct IterPos
		{
			Iter *It;
			size_t Pos;

			IterPos(Iter *i, size_t p)
			{
				It = i;
				Pos = p;
			}

			bool operator !=(const IterPos &i) const
			{
				bool Eq = It == i.It && Pos == i.Pos;
				return !Eq;
			}

			IterPos &operator *()
			{
				return *this;
			}

			operator LString()
			{
				return (*It->a)[Pos].Str;
			}

			IterPos &operator++()
			{
				Pos++;
				return *this;
			}

			Key *Deref(LString &addr, bool create)
			{
				if (!It->a->IdxCheck(Pos))
					return nullptr;
				return (*It->a)[Pos].Deref(addr, create);
			}

			LString GetJson()
			{
				if (!It->a->IdxCheck(Pos))
					return LString();
				return (*It->a)[Pos].Print(0);
			}

			LString Get(LString Addr)
			{
				if (auto k = Deref(Addr, false))
					return k->Str;
				return LString();
			}

			Pair Get()
			{
				auto &a = *It->a;
				if (!a.IdxCheck(Pos))
					return Pair();
				
				Key &k = a[Pos];				
				if (k.Name)
					return Pair(k.Name, k.Str);
				
				if (k.Obj.Length())
					return Pair(k.Obj[0].Name, k.Obj[0].Str);
					
				return Pair();
			}

			Iter GetArray(LString Addr)
			{
				Iter a(It->j);

				auto &Arr = *It->a;
				if (Pos < Arr.Length())
				{
					Key *k = Arr[Pos].Deref(Addr, false);
					if (k)
						a.Set(&k->Array);
				}

				return a;
			}

			bool Set(LString Addr, const char *Val)
			{
				if (auto k = Deref(Addr, true))
				{
					k->Str = Val;
					return true;
				}
				return false;
			}

			bool Set(LString Addr, bool b)
			{
				if (auto k = Deref(Addr, true))
					return k->Str.Printf("%i", b) > 0;
				return false;
			}

			bool Set(LString Addr, int64_t Int)
			{
				if (auto k = Deref(Addr, true))
					return k->Str.Printf(LPrintfInt64, Int) > 0;
				return false;
			}

			bool Set(LString Addr, double Dbl)
			{
				if (auto k = Deref(Addr, true))
					return k->Str.Printf("%f", Dbl) > 0;
				return false;
			}

			bool Set(LString Addr, const LArray<LString> &Array)
			{
				if (auto k = Deref(Addr, true))
				{
					for (size_t i=0; i<Array.Length(); i++)
						k->Array.New().Str = Array.ItemAt(i);
					return true;
				}
				return false;
			}

			bool Set(LString Addr, const LArray<LJson> &Array)
			{
				if (auto k = Deref(Addr, true))
				{
					for (size_t i=0; i<Array.Length(); i++)
					{
						auto &out = k->Array.New();
						out = Array.ItemAt(i).Root;
					}
					return true;
				}
				return false;
			}
		};

		Iter(LJson *J = NULL) : j(J)
		{
			a = NULL;
		}

		void Set(LJson *J)
		{
			j = J;
		}

		void Set(LArray<Key> *arr)
		{
			a = arr;
		}

		size_t Length()
		{
			return a ? a->Length() : 0;
		}

		IterPos begin()
		{
			return IterPos(this, 0);
		}

		IterPos end()
		{
			return IterPos(this, a ? a->Length() : 0);
		}
	};

	Iter GetArray(LString Addr)
	{
		Iter a(this);
		Key *k = Deref(Addr, false);
		if (k)
			a.Set(&k->Array);

		return a;
	}

	bool Set(LString Addr, const char *Val)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		k->Str = Val;
		return true;
	}

	bool Set(LString Addr, bool b)
	{
		char s[32];
		sprintf_s(s, sizeof(s), "%i", b);
		return Set(Addr, s);
	}

	bool Set(LString Addr, int64_t Int)
	{
		char s[32];
		sprintf_s(s, sizeof(s), LPrintfInt64, Int);
		return Set(Addr, s);
	}

	bool Set(LString Addr, uint64_t Int)
	{
		char s[32];
		sprintf_s(s, sizeof(s), LPrintfUInt64, Int);
		return Set(Addr, s);
	}
	
	bool Set(LString Addr, double Dbl)
	{
		char s[32];
		sprintf_s(s, sizeof(s), "%f", Dbl);
		return Set(Addr, s);
	}

	bool Set(LString Addr, const LArray<LString> &Array)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		for (size_t i=0; i<Array.Length(); i++)
			k->Array.New().Str = Array.ItemAt(i);
		return true;
	}

	bool Set(LString Addr, const LArray<LJson> &Array)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		for (size_t i=0; i<Array.Length(); i++)
		{
			auto &out = k->Array.New();
			out = Array.ItemAt(i).Root;
		}
		return true;
	}

	LString::Array GetKeys(const char *Addr = nullptr)
	{
		LString::Array keys;

		if (auto k = Deref(Addr, true))
			for (auto &i: k->Obj)
				keys.Add(i.Name);

		return keys;
	}

	// Testing 
	static LString PrintTest()
	{
		LJson j;

		LArray<LJson> a;
		j.Set("noElem", a);
			a[0] = "one";
		j.Set("oneStr", a);
			a.Add("two");
			a.Add("three");
		j.Set("threeStr", a);

		LArray<LJson> objs;
		auto &first = objs.New();
			first.Set("field", "val");
			first.Set("field2", "val2");
		j.Set("oneObj", objs);
			auto &second = objs.New();
			second.Set("field", "val");
			second.Set("field2", "val2");
		j.Set("twoObj", objs);

		return j.GetJson();
	}
};

#endif
