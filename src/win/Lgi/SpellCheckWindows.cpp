#include "lgi/common/Lgi.h"
#include "lgi/common/SpellCheck.h"
#if defined(_MSC_VER) && _MSC_VER >= _MSC_VER_VS2013
#include "Spellcheck.h"
#endif

#ifdef __spellcheck_h__

#include <initguid.h>
DEFINE_GUID(CLSID_SpellCheckerFactory,0x7AB36653,0x1796,0x484B,0xBD,0xFA,0xE7,0x4F,0x1D,0xB7,0xC1,0xDC);
DEFINE_GUID(IID_ISpellCheckerFactory,0x8E018A9D,0x2415,0x4677,0xBF,0x08,0x79,0x4E,0xA6,0x1F,0x94,0xBB);

#include "lgi/common/HashTable.h"

int LangCmp(LSpellCheck::LanguageId *a, LSpellCheck::LanguageId *b)
{
	return Stricmp(a->LangCode.Get(), b->LangCode.Get());
}

int DictionaryCmp(LSpellCheck::DictionaryId *a, LSpellCheck::DictionaryId *b)
{
	return Stricmp(a->Dict.Get(), b->Dict.Get());
}

class WindowsSpellCheck : public LSpellCheck
{
	bool Ok;
	ISpellCheckerFactory *Factory;
	ISpellChecker *Sc;

	struct Lang
	{
		LString Id;
		LString English;
		LString Native;
		LString::Array Dictionaries;
	};

	LHashTbl<ConstStrKey<char,false>, Lang*> Languages;

	bool ReadCsv()
	{
		LString File = __FILE__;
		File = File.Replace(".cpp", ".csv");
		if (!LFileExists(File))
			File = LFindFile(LGetLeaf(File));

		LFile f;
		if (!f.Open(File, O_READ))
		{
			LAssert(!"Csv file missing?");
			return false;
		}

		LString::Array Lines = f.Read().SplitDelimit("\r\n");
		for (auto Ln: Lines)
		{
			LString::Array v = Ln.Split(",");
			if (v.Length() > 1)
			{
				Lang *l = Languages.Find(v[0].Strip());
				if (l)
				{
					l->English = v[1].Strip();
					if (v.Length() > 2)
						l->Native = v[2].Strip();
				}
			}
		}

		return true;
	}

public:
	WindowsSpellCheck() :
		LSpellCheck("LSpellCheck")
	{
		Ok = false;
		Sc = NULL;
		Factory = NULL;
	}

	~WindowsSpellCheck()
	{
		EndThread();
		if (Sc)
		{
			Sc->Release();
			Sc = NULL;
		}
		if (Factory)
		{
			Factory->Release();
			Factory = NULL;
		}
	}

	ISpellCheckerFactory *GetFactory()
	{
		if (!Factory)
		{
			HRESULT r = CoInitialize(NULL);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - CoInitialize failed.\n", _FL);
				return NULL;
			}

			r = CoCreateInstance(CLSID_SpellCheckerFactory, NULL, CLSCTX_INPROC_SERVER, IID_ISpellCheckerFactory, (LPVOID*)&Factory);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - CoCreateInstance(CLSID_SpellCheckerFactory) failed.\n", _FL);
				return NULL;
			}

			IEnumString *Value = NULL;
			r = Factory->get_SupportedLanguages(&Value);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - get_SupportedLanguages failed: %i\n", _FL, r);
			}
			else
			{
				LPOLESTR Str;
				while (SUCCEEDED((r = Value->Next(1, &Str, NULL))) && Str)
				{
					LString s = Str;			
					LString::Array a = s.Split("-", 1);
					if (a.Length() == 2)
					{
						Lang *l = Languages.Find(a[0]);
						if (!l)
						{
							Languages.Add(a[0], l = new Lang);
							l->Id = a[0];
						}
						if (l)
						{
							l->Dictionaries.New() = s;
						}
					}

					CoTaskMemFree(Str);
				}

				Value->Release();

				ReadCsv();
			}
		}

		return Factory;
	}

	ISpellChecker *GetSpellCheck(const char *Lang)
	{
		if (!GetFactory())
		{
			LgiTrace("%s:%i - No factory.\n", _FL);
			return NULL;
		}
		
		LAutoWString WLang(Utf8ToWide(Lang));
		HRESULT r = Factory->CreateSpellChecker(WLang, &Sc);
		if(FAILED(r))
		{
			auto l = FindLanguage(Lang);
			if (l && WLang.Reset(Utf8ToWide(l->Id)))
				r = Factory->CreateSpellChecker(WLang, &Sc);
		}

		if(FAILED(r))
		{
			LgiTrace("%s:%i - CreateSpellChecker(%s) failed: 0x%x.\n", _FL, Lang, r);
			return NULL;
		}

		return Sc;
	}

	Lang *FindLanguage(LString str)
	{
		Lang* l = Languages.Find(str);
		if (l)
			return l;

		// Search by full name?
		for (auto i : Languages)
		{
			if (i.value->English.Equals(str) ||
				i.value->Native.Equals(str))
			{
				return i.value;
			}
		}

		return NULL;
	}

	LMessage::Result OnEvent(LMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_SET_PARAMS:
			{
				LAutoPtr<LSpellCheck::Params> Params((LSpellCheck::Params*)Msg->A());
				if (Params)
				{
					auto l = GFindLang("", Params->Lang);
					if (l)
						GetSpellCheck(l->Id);
				}
				break;
			}
			case M_ENUMERATE_LANGUAGES:
			{
				if (!GetFactory())
				{
					LgiTrace("%s:%i - No factory.\n", _FL);
					break;
				}

				int ResponseHnd = (int)Msg->A();
				LAutoPtr< LArray<LanguageId> > Langs(new LArray<LanguageId>);

				for (auto l : Languages)
				{
					LanguageId &i = Langs->New();
					i.LangCode = l.value->Id;
					i.EnglishName = l.value->English ? l.value->English : l.value->Id;
					i.NativeName = l.value->Native;
				}

				if (Langs && Langs->Length() > 0)
				{
					Langs->Sort(LangCmp);
					PostObject(ResponseHnd, Msg->Msg(), Langs);
				}
				break;
			}
			case M_ENUMERATE_DICTIONARIES:
			{
				int ResponseHnd = (int)Msg->A();
				LAutoPtr<LString> Param((LString*)Msg->B());

				if (!GetFactory())
				{
					LgiTrace("%s:%i - No factory.\n", _FL);
					break;
				}
				if (!Param)
				{
					LAssert(!"No param.");
					break;
				}

				LAutoPtr< LArray<DictionaryId> > Out(new LArray<DictionaryId>);
				Lang *l = FindLanguage(*Param);
				if (!l)
				{
					LgiTrace("%s:%i - Language '%s' not found.\n", _FL, Param->Get());
					break;
				}
				
				for (size_t i=0; i<l->Dictionaries.Length(); i++)
				{
					LString &d = l->Dictionaries[i];
					LString::Array a = d.Split("-", 1);
					if (a.Length() == 2)
					{
						DictionaryId &id = Out->New();
						id.Lang = l->Id.Get();
						id.Dict = a[1].Get();
					}
				}

				if (Out && Out->Length() > 0)
				{
					Out->Sort(DictionaryCmp);
					PostObject(ResponseHnd, Msg->Msg(), Out);
				}
				break;
			}
			case M_SET_DICTIONARY:
			{
				int ResponseHnd = (int)Msg->A();
				LAutoPtr<DictionaryId> Dict((DictionaryId*)Msg->B());
				bool Success = false;
				if (Dict)
				{
					LString Lang;
					if (Dict->Dict)
						Lang.Printf("%s-%s", Dict->Lang.Get(), Dict->Dict.Get());
					else
						Lang = Dict->Lang;
					Success = GetSpellCheck(Lang) != NULL;
				}

				PostThreadEvent(ResponseHnd, M_SET_DICTIONARY, (LMessage::Param)Success);
				break;
			}
			case M_CHECK_TEXT:
			{
				int ResponseHnd = (int)Msg->A();
				LAutoPtr<CheckText> Ct((CheckText*)Msg->B());
				if (!Ct)
				{
					LgiTrace("%s:%i - No text specified.\n", _FL);
					break;
				}
				if (!Sc)
				{
					LgiTrace("%s:%i - No spell check loaded.\n", _FL);
					break;
				}

				LAutoWString WTxt(Utf8ToWide(Ct->Text));
				IEnumSpellingError *ErrEnum = NULL;
				HRESULT r = Sc->Check(WTxt, &ErrEnum);
				if (FAILED(r))
				{
					LgiTrace("%s:%i - Check failed: %i.\n", _FL, r);
					break;
				}

				ISpellingError *Err = NULL;
				while (SUCCEEDED((r = ErrEnum->Next(&Err))) && Err)
				{
					ULONG Start, Len;
					CORRECTIVE_ACTION Action;
					LPWSTR Replace = NULL;

					r = Err->get_StartIndex(&Start);
					r = Err->get_Length(&Len);
					r = Err->get_CorrectiveAction(&Action);
					r = Err->get_Replacement(&Replace);

					Err->Release();

					auto ww = WTxt.Get() + Start;

					SpellingError &Se = Ct->Errors.New();
					Se.Start = Start;
					Se.Len = Len;
					if (Action == CORRECTIVE_ACTION_GET_SUGGESTIONS)
					{
						LAutoWString Word(NewStrW(WTxt + Start, Len));
						IEnumString *Strs = NULL;
						r = Sc->Suggest(Word, &Strs);
						if (SUCCEEDED(r))
						{
							LPOLESTR Str;
							while (SUCCEEDED((r = Strs->Next(1, &Str, NULL))) && Str)
							{
								Se.Suggestions.Add(Str);
								CoTaskMemFree(Str);
							}
							Strs->Release();
						}
					}
				}

				ErrEnum->Release();

				PostObject(ResponseHnd, Msg->Msg(), Ct);
				break;
			}
		}

		return 0;
	}
};

#endif

LAutoPtr<LSpellCheck> CreateWindowsSpellCheck()
{
	LAutoPtr<LSpellCheck> p;
	#ifdef __spellcheck_h__
	p.Reset(new WindowsSpellCheck);
	#endif
	return p;
}

