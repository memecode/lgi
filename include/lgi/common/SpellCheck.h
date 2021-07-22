#ifndef _LGI_SPELL_CHECK_H_
#define _LGI_SPELL_CHECK_H_

#include "lgi/common/EventTargetThread.h"
#include "lgi/common/OptionsFile.h"

enum SPELL_MSGS
{
	M_CHECK_TEXT = M_USER + 100,
	M_ENUMERATE_LANGUAGES,
	M_ENUMERATE_DICTIONARIES,
	M_SET_DICTIONARY,
	M_SET_PARAMS,
	M_ADD_WORD,
	M_INSTALL_DICTIONARY,
};

enum SpellCheckParams
{
	SpellBlockPtr,
};

#define SPELL_CHK_VALID_HND(hnd) \
	if (hnd < 0)				\
	{							\
		LAssert(0);			\
		return false;			\
	}

// Spell check interface
class LSpellCheck : public LEventTargetThread
{
public:
	static const char Delimiters[];

	struct LanguageId
	{
		LString LangCode;
		LString EnglishName;
		LString NativeName;
	};
	
	struct DictionaryId
	{
		LString Lang;
		LString Dict;
	};

	struct Params
	{
		LOptionsFile::PortableType IsPortable;
		LString OptionsPath;
		LString Lang, Dict;
		LCapabilityTarget *CapTarget;
		
		Params()
		{
			CapTarget = NULL;
		}
	};
	
	struct SpellingError : public LRange
	{
		LString::Array Suggestions;
	};
	
	struct CheckText : public LRange
	{
		LString Text;
		LArray<SpellingError> Errors;

		// Application specific data
		LArray<LVariant> User;
		
		CheckText()
		{
		}
	};


	LSpellCheck(LString Name) : LEventTargetThread(Name) {}
	virtual ~LSpellCheck() {}

	// Impl OnEvent in your subclass:
	// GMessage::Result OnEvent(GMessage *Msg);

	/// Sends a M_ENUMERATE_LANGUAGES event to 'ResponseHnd' with a heap
	/// allocated LArray<LanguageId>.
	bool EnumLanguages(int ResponseHnd)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_ENUMERATE_LANGUAGES,
						(GMessage::Param)ResponseHnd);
	}

	/// Sends a M_ENUMERATE_DICTIONARIES event to 'ResponseHnd' with a heap
	/// allocated LArray<DictionaryId>.
	bool EnumDictionaries(int ResponseHnd, const char *Lang)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_ENUMERATE_DICTIONARIES,
						(GMessage::Param)ResponseHnd,
						(GMessage::Param)new LString(Lang));
	}

	bool SetParams(LAutoPtr<Params> p)
	{
		return PostObject(GetHandle(), M_SET_PARAMS, p);
	}

	bool SetDictionary(int ResponseHnd, const char *Lang, const char *Dictionary = NULL)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		
		LAutoPtr<DictionaryId> i(new DictionaryId);
		if (!i)
			return false;
		i->Lang = Lang;
		i->Dict = Dictionary;
		
		return PostObject(	GetHandle(),
							M_SET_DICTIONARY,
							(GMessage::Param)ResponseHnd,
							i);
	}

	bool Check(	int ResponseHnd,
				LString s,
				ssize_t Start, ssize_t Len,
				LArray<LVariant> *User = NULL /* see 'SpellCheckParams' */)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		
		if (s.Length() == 0)
			return false;

		LAutoPtr<CheckText> c(new CheckText);
		c->Text = s;
		c->Start = Start;
		c->Len = Len;
		if (User)
			c->User = *User;
		
		return PostObject(GetHandle(), M_CHECK_TEXT, (GMessage::Param)ResponseHnd, c);
	}
	
	bool InstallDictionary()
	{
		return PostEvent(M_INSTALL_DICTIONARY);
	}
};

// These are the various implementations of the this object. You have to include the
// correct C++ source to get this to link.
extern LAutoPtr<LSpellCheck> CreateWindowsSpellCheck();		// Available on Windows 8.0 and greater
extern LAutoPtr<LSpellCheck> CreateAppleSpellCheck();		// Available on Mac OS X
extern LAutoPtr<LSpellCheck> CreateAspellObject();			// Available anywhere.

#endif