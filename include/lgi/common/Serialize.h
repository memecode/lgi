#pragma once

#include "lgi/common/StringClass.h"

class LSerialize
{
	enum Types
	{
		// Primitive types (no array size)
		LInt,
		LFloat,

		// Array types (has array size)
		LStr,
		LObject,
		LBinary,
	};

	union Field
	{
		/*
		Data format:
			uint8_t ObjectType;	 // LSTypes
			uint8_t ElementSize; // in bytes
			uint16_t Id;
			if ObjectType == LStr || LObject || LBinary
				uint32_t ArraySize
			...PayloadBytes
		*/
		
		uint8_t u8[8];
		uint16_t u16[4];
		uint32_t u32[2]; // At least have enough data for the headers and the array size...

	public:
		// Field type
		Types Type() { return (Types) u8[0]; }
		void Type(Types t) { u8[0] = (uint8_t)t; }

		// Size of individual element
		uint8_t Size() { return u8[1]; }
		void Size(uint8_t sz) { u8[1] = sz; }

		// Field ID
		uint16_t Id() { return u16[1]; }
		void Id(uint16_t f) { u16[1] = f; }

		// For atomic types... int, float etc
		uint8_t  *Payload8()  { return (uint8_t*)  (u32 + 1); }
		uint16_t *Payload16() { return (uint16_t*) (u32 + 1); }
		uint32_t *Payload32() { return (uint32_t*) (u32 + 1); }
		uint64_t *Payload64() { return (uint64_t*) (u32 + 1); }
		float    *PayloadF()  { return (float*)    (u32 + 1); }
		double   *PayloadD()  { return (double*)   (u32 + 1); }

		// For array based string, object and binary types...
		uint32_t &ArraySize() { return u32[1]; }
		uint8_t  *Array8()    { return (uint8_t*)  (u32 + 2); }
		uint16_t *Array16()   { return (uint16_t*) (u32 + 2); }
		uint32_t *Array32()   { return (uint32_t*) (u32 + 2); }

		bool IsValid()
		{
			switch (Type())
			{
				case LInt:
					switch (Size())
					{
						case 1:
						case 2:
						case 4:
						case 8:
							return true;
					}
					return false;
				case LStr:
					if (Size() == 1)
						return Array8()[ArraySize()] == 0;
					else if (Size() == 2)
						return Array16()[ArraySize()] == 0;
					else if (Size() == 4)
						return Array32()[ArraySize()] == 0;
					return false;
				case LBinary:
					if (Size() != 1)
						return false;
					return true;
				case LFloat:
					if (Size() == sizeof(float))
						return true;
					if (Size() == sizeof(double))
						return true;
					return false;
				case LObject:
					return true;
			}

			return false;
		}

		uint32_t Sizeof()
		{
			switch (Type())
			{
				case LInt:
				case LFloat:
					return 4 + Size();
				case LStr:
					return 8 + (Size() * (ArraySize() + 1));
				case LObject:
					return 4 + Size();
				case LBinary:
					return 8 + ArraySize();
			}

			return sizeof(*this);
		}
	};

	uint16 ObjectId;
	bool ToStream;
	LStreamI *Stream;
	LArray<char> FieldMem;
	LHashTbl<IntKey<int>, ssize_t> Fields;
	uint32_t Bytes;
	LAutoWString wideCache;

protected:
	Field *GetField(int Id)
	{
		ssize_t o = Fields.Find(Id);
		if (o >= 0)
			return (Field*)FieldMem.AddressOf(o);
		return NULL;
	}

	template<typename T>
	bool Int(uint16 Id, T &i)
	{
		if (ToStream)
		{
			Field f;
			f.Type(LInt);
			f.Id(Id);
			f.Size(sizeof(i));
			memcpy(f.Payload8(), &i, sizeof(i));

			int Bytes = f.Sizeof();
			if (Stream->Write(&f, Bytes) != Bytes)
				return false;
		}
		else
		{
			Field *f = GetField(Id);
            if (!f)
                return false;

			switch (f->Size())
			{
				case 1: i = (T)*f->Payload8();  break;
				case 2: i = (T)*f->Payload16(); break;
				case 4: i = (T)*f->Payload32(); break;
				case 8: i = (T)*f->Payload64(); break;
				default: return false;
			}
		}

		return true;
	}

	bool String(uint16 Id, LString &s)
	{
		if (ToStream)
		{
			Field f;
			f.Type(LStr);
			f.Size(1);
			f.Id(Id);
			f.ArraySize() = (uint32_t)s.Length();
			
			ssize_t hdrBytes = 8;
			ssize_t payloadBytes = s.Length() + 1/*null*/;
			if (Stream->Write(&f, hdrBytes) != hdrBytes)
				return false;
			if (Stream->Write(s.Get(), payloadBytes) != payloadBytes)
				return false;
		}
		else
		{
			auto f = GetField(Id);
			if (!f || f->Type() != LStr)
				return false;
			
			if (f->Size() != 1)
			{
				// FIXME: add conversion here?
				LAssert(!"wrong char size");
				return false;
			}
			
			return s.Set((char*)f->Payload8(), f->ArraySize());
		}

		return false;
	}

	Field *Alloc(int Id, size_t Sz)
	{
		auto f = GetField(Id);
		if (f && f->Sizeof() >= Sz)
			return f;

		size_t Off = FieldMem.Length();
		FieldMem.Length(Off + Sz);

		f = (Field*) FieldMem.AddressOf(Off);
		f->Id(Id);
		Fields.Add(Id, Off);
		return f;		
	}

public:
	LSerialize(uint16 ObjId = 0) : Fields(64,-1)
	{
		ObjectId = ObjId;
		ToStream = true;
		Stream = NULL;
	}

	virtual ~LSerialize()
	{
	}

	uint16 GetObjectId()
	{
		return ObjectId;
	}

	size_t GetSize()
	{
		size_t Sz = 0;
		
		for (auto o : Fields)
		{
			auto i = (Field*)FieldMem.AddressOf(o.value);
			Sz += i->Sizeof();
		}

		return Sz;
	}

	void Empty()
	{
		ObjectId = 0;
		Fields.Empty();
		FieldMem.Length(0);
	}

	int64 GetInt(int Id, int Default = -1)
	{
		auto f = GetField(Id);
		if (!f || f->Type() == LObject)
			return Default;

		switch (f->Type())
		{
			case LInt:
				switch (f->Size())
				{
					case 1:
						return *f->Payload8();
					case 2:
						return *f->Payload16();
					case 4:
						return *f->Payload32();
					case 8:
						return *f->Payload64();
				}
			case LFloat:
				return (int)*f->PayloadF();
			case LStr:
				if (f->Size() == 1)
					return Atoi(f->Array8());
				else if (f->Size() == 2)
					return Atoi(f->Array16());
				else if (f->Size() == 4)
					return Atoi(f->Array32());
				else
					LAssert(!"Invalid string size");
				break;
			default:
				LAssert(!"Invalid type.");
		}

		return Default;
	}

	float GetFloat(int Id, float Default = 0.0)
	{
		Field *f = GetField(Id);
		if (!f || f->Type() == LObject)
			return Default;

		switch (f->Type())
		{
			case LInt:
				switch (f->Size())
				{
					case 1:
						return (float)*f->Payload8();
					case 2:
						return (float)*f->Payload16();
					case 4:
						return (float)*f->Payload32();
					case 8:
						return (float)*f->Payload64();
				}
			case LFloat:
				if (f->Size() == sizeof(float))
					return *f->PayloadF();
				else if (f->Size() == sizeof(double))
					return (float)*f->PayloadD();
				else
					LAssert(!"Invalid size.");
				break;
			case LStr:
				if (f->Size() == 1)
				{
					return (float)atof((const char*)f->Array8());
				}
				else if (f->Size() == 2)
				{
					LString s( (LString::Char16*) f->Array16());
					return (float)s.Float();
				}
				else if (f->Size() == 4)
				{
					LString s( (LString::Char32*) f->Array32());
					return (float)s.Float();
				}
				else
					LAssert(!"Invalid string size");
				break;
			default:
				LAssert(!"Invalid type.");
				break;
		}

		return Default;
	}


	const char *GetStr(int Id, const char *Default = NULL)
	{
		auto f = GetField(Id);
		if (!f || f->Type() != LStr)
			return Default;
		
		if (f->Size() == 1)
			return (const char*) f->Array8();
		else
			LAssert(!"Request for wrong string width");

		return Default;
	}

	template<typename T>
	const T *GetStrT(int Id, const T *Default = NULL)
	{
		auto f = GetField(Id);
		if (!f || f->Type() != LStr)
			return Default;
		
		switch (f->Size())
		{
			case 1:
			{
				if (sizeof(T) == 1)
					return (const T*) f->Array8();
				else
					LAssert(!"FIXME: Convert utf-8");
				break;
			}
			case 2:
			{
				if (sizeof(T) == 2)
					return (const T*) f->Array16();
				else
					LAssert(!"FIXME: Convert utf-16");
				break;
			}
			case 4:
			{
				if (sizeof(T) == 4)
					return (const T*) f->Array32();
				else
					LAssert(!"FIXME: Convert utf-32");
				break;
			}
			default:
			{
				LAssert(!"Unknown word size");
				break;
			}
		}

		return Default;
	}

	bool SetBinary(int Id, void *Ptr, ssize_t Size)
	{
		auto f = Alloc(Id, 8 + Size);
		if (!f)
			return false;
		
		f->Type(LBinary);
		f->Size(1);
		f->ArraySize() = (uint32_t) Size;
		if (Ptr && Size > 0)
			memcpy(f->Array8(), Ptr, Size);
		
		return true;
	}

	bool GetBinary(int Id, uint8_t *&Ptr, uint32_t &Size)
	{
		auto f = GetField(Id);
		if (!f || f->Type() != LBinary)
			return false;
		
		Ptr = (uint8_t*) f->Array8();
		Size = f->ArraySize();

		#ifdef _DEBUG
		uint8_t *End = Ptr + Size;
		uint8_t *Flds = (uint8_t*)FieldMem.AddressOf();
		LAssert(Ptr >= Flds && End <= Flds + FieldMem.Length());
		#endif
		
		return true;
	}

	template<typename T>
	bool SetInt(int Id, T i)
	{
		auto f = Alloc(Id, 4 + sizeof(i));
		if (!f)
			return false;

		f->Type(LInt);
		f->Size(sizeof(i));
		memcpy(f->Payload8(), &i, sizeof(i));
		return true;
	}

	template<typename T>
	bool SetFloat(int Id, T i)
	{
		auto f = Alloc(Id, 4 + sizeof(i));
		if (!f)
			return false;

		f->Type(LFloat);
		f->Size(sizeof(i));
		memcpy(f->Payload8(), &i, sizeof(i));
		return true;
	}

	template<typename T>
	bool SetStr(int Id, const T *w)
	{
		if (!w)
			return false;

		ssize_t chars = Strlen(w);
		ssize_t bytes = (chars + 1) * sizeof(*w);
		Field *f = Alloc(Id, 8 + bytes);
		if (!f)
			return false;

		f->Type(LStr);
		f->Size(sizeof(*w));
		f->ArraySize() = (uint32_t)chars;
		memcpy(f->Array8(), w, bytes);
		return true;
	}
	
	bool SetStr(int id, LString s)
	{
		auto f = Alloc(id, 8 + s.Length() + 1);
		if (!f)
			return false;

		f->Type(LStr);
		f->Size(1);
		f->ArraySize() = (uint32_t)s.Length();
		memcpy(f->Array8(), s.Get(), s.Length() + 1);
		return true;
	}

	bool CanRead(void *ptr, size_t size)
	{
		if (!ptr || size < 8)
			return false;

		auto f = (Field*)ptr;
		if (f->Type() != LObject)
			return false;

		if (f->Sizeof() > size)
			return false;

		return true;
	}

	bool Serialize(LStream *stream, bool write)
	{
		if ((ToStream = write))
		{
			uint32_t Size = 0;
			// for (ssize_t o = Fields.First(); o >= 0; o = Fields.Next())
			for (auto o : Fields)
			{
				Field *i = (Field*)FieldMem.AddressOf(o.value);
				Size += i->Sizeof();
			}

			Field f;
			f.Type(LObject);
			f.Size(1);
			f.Id(GetObjectId());
			f.ArraySize() = Size;
			
			if (stream->Write(&f, 8) != 8)
				return false;

			for (auto o: Fields)
			{
				auto i = (Field*)FieldMem.AddressOf(o.value);
				int bytes = i->Sizeof();
				if (stream->Write(i, bytes) != bytes)
					return false;
			}
		}
		else
		{
			Empty();

			Field f;
			if (stream->Read(&f, 8) != 8)
				return false;

			if (f.Type() != LObject)
				return false;
			ObjectId = f.Id();

			FieldMem.Length(f.ArraySize());
			if (stream->Read(FieldMem.AddressOf(), f.ArraySize()) != f.ArraySize())
				return false;

			// Parse all the fields into a hash table for O(1) access.
			char *s = FieldMem.AddressOf();
			char *e = s + FieldMem.Length();
			while (s < e)
			{
				Field *fld = (Field*)s;
				if (!fld->IsValid())
				{
					fld->IsValid();
					LAssert(!"Invalid field");
					return false;
				}

				Fields.Add(fld->Id(), s - FieldMem.AddressOf(0));
				s += fld->Sizeof();
			}
		}

		return true;
	}


	// Unit test
	static bool UnitTest()
	{
		enum Ids {
			idNone,
			idInt8 = 100,
			idInt16,
			idInt32,
			idInt64,
			idFloat,
			idDouble,
			idStr8,
			idStr16,
			idStr32,
			idBinary
		};
		
		LSerialize wr;
		
		// Write all the different types
		uint8_t i8 = 20;
		wr.SetInt(idInt8, i8);
		
		uint16_t i16 = 21;
		wr.SetInt(idInt16, i16);

		uint32_t i32 = 22;
		wr.SetInt(idInt32, i32);

		uint64_t i64 = 23;
		wr.SetInt(idInt64, i64);
		
		float f = 24.0f;
		wr.SetFloat(idFloat, f);

		double d = 25.0f;
		wr.SetFloat(idDouble, d);
	
		char s8[] = "26";
		wr.SetStr(idStr8, s8);

		LString::Char16 s16[] = { '2', '7', 0 };
		wr.SetStr(idStr16, s16);

		LString::Char32 s32[] = { '2', '8', 0 };
		wr.SetStr(idStr32, s32);

		// Serialize them
		LStringPipe p;
		wr.Serialize(&p, true);
		
		LSerialize rd;
		p.SetPos(0);
		if (!rd.Serialize(&p, false))
			return false;
		
		// Read out the results on the other side and see if they match?
		if (rd.GetInt(idInt8) != i8)
		{
			LAssert(!"int8 failed");
			return false;
		}

		if (rd.GetInt(idInt16) != i16)
		{
			LAssert(!"int16 failed");
			return false;
		}

		if (rd.GetInt(idInt32) != i32)
		{
			LAssert(!"int32 failed");
			return false;
		}

		if (rd.GetInt(idInt64) != i64)
		{
			LAssert(!"int64 failed");
			return false;
		}

		if (rd.GetFloat(idFloat) != f)
		{
			LAssert(!"float failed");
			return false;
		}

		if (rd.GetFloat(idDouble) != d)
		{
			LAssert(!"double failed");
			return false;
		}

		if (Strcmp(rd.GetStr(idStr8), s8))
		{
			LAssert(!"str8 failed");
			return false;
		}

		if (Strcmp(rd.GetStrT<LString::Char16>(idStr16), s16))
		{
			LAssert(!"str16 failed");
			return false;
		}

		if (Strcmp(rd.GetStrT<LString::Char32>(idStr32), s32))
		{
			LAssert(!"str32 failed");
			return false;
		}

		return true;
	}
};
