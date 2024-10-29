#pragma once

// Pseudo LArray type interface. LArray is too slow for large
// blocks of straight bytes (like hundreds of MiB), the destructor 
// loop for every element kills cleanup performance.
template<typename T>
class LMallocArray
{
	size_t Len = 0;
	T *Data = NULL;

public:
	LMallocArray(size_t defSize = 0)
	{
		if (defSize)
			Length(defSize);
	}

	~LMallocArray()
	{
		Length(0);
	}

	size_t Length()
	{
		return Len;
	}

	bool Length(size_t s)
	{
		if (!s)
		{
			free(Data);
			Data = nullptr;
			Len = s;
			return true;
		}
		
		if (Data)
			Data = (T*)realloc(Data, s);
		else
			Data = (T*)malloc(s);

		if (!Data) return false;
		Len = s;
		
		return true;
	}

	T *AddressOf(size_t off = 0)
	{
		if (!Data)
		{
			LAssert(!"No data.");
			return NULL;
		}

		return Data + off;
	}

	T &operator[](size_t i)
	{
		if (!Data)
		{
			LAssert(!"No data");
			static T empty = 0;
			return empty;
		}

		return Data[i];
	}
};

