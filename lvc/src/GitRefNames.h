#pragma once

#include "lgi/common/Lgi.h"
#include "lgi/common/HashTable.h"

class GitRefNames
{
	LHashTbl<ConstStrKey<char, false>, LString> Names;
	LArray<size_t> PrefixLengths;
	LHashTbl<ConstStrKey<char, false>, LString*> Cache;

public:
	GitRefNames() = default;
	GitRefNames(const GitRefNames &) = delete;
	GitRefNames &operator=(const GitRefNames &) = delete;
	~GitRefNames() { Cache.DeleteObjects(); }

	void Empty()
	{
		Cache.DeleteObjects();
		Names.Empty();
		PrefixLengths.Length(0);
	}

	void Add(LString hash, LString name)
	{
		Cache.DeleteObjects();
		auto existing = Names.Find(hash);
		Names.Add(hash, existing ? existing + "," + name : name);
		if (!PrefixLengths.HasItem(hash.Length()))
			PrefixLengths.Add(hash.Length());
	}

	LString Find(LString hash)
	{
		if (!hash)
			return LString();
		if (auto cached = Cache.Find(hash))
			return *cached;

		LString names;
		// Git may use different abbreviation lengths for different refs.
		for (auto length: PrefixLengths)
		{
			if (length > hash.Length())
				continue;
			auto match = Names.Find(hash(0, length));
			if (match)
			{
				if (names)
					names += ",";
				names += match;
			}
		}

		// Store empty results too: most commits have no refs pointing at them.
		LAutoPtr<LString> cached(new LString(names));
		if (Cache.Add(hash, cached.Get()))
			cached.Release();
		else
			LgiTrace("%s:%i - Failed to cache Git ref names.\n", _FL);
		return names;
	}
};
