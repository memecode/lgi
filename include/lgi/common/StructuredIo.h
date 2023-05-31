#ifndef _STRUCTURED_IO_H_
#define _STRUCTURED_IO_H_

#include <functional>
#include "lgi/common/Variant.h"
#include "lgi/common/LMallocArray.h"

#define DEBUG_STRUCT_IO			0

/*
	Generic base data field:
	(where size_t == EncSize/DecSize field)

		uint8_t type; // LVariantType
		size_t name_len;
		char name[name_len];
		size_t data_size;
		uint8_t data[data_size];

	Objects are wrapped with generic fields of the type

		GV_CUSTOM // start of object...
			...array of member fields....
		GV_VOID_PTR // end of object

*/

class LStructuredIo : public LMallocArray<uint8_t>
{
	bool Write = true;
	size_t Pos = 0;

protected:
	bool CheckSpace(size_t bytes)
	{
		auto l = Pos + bytes;
		if (l > Length())
		{
			if (!Length((l + 255) & ~0xff))
				return false;
			LAssert(Length() >= l);
		}
		return true;
	}

	constexpr static int SevenMask = 0x7f;

	template<typename T>
	void EncSize(LPointer &p, T sz)
	{
		if (!sz)
			*p.u8++ = 0;
		else
			while (sz)
			{
				uint8_t bits = sz & SevenMask;
				sz >>= 7;
				*p.u8++ = bits | (sz ? 0x80 : 0);
			}
	}

	template<typename T>
	void DecSize(LPointer &p, T &sz)
	{
		sz = 0;
		uint8_t bits, shift = 0;
		do
		{
			bits = *p.u8++;
			sz |= ((T)bits & SevenMask) << shift;
			shift += 7;
		}
		while (bits & 0x80);
	}

	bool Encode(uint8_t type, const void *obj = NULL, size_t sz = 0, const char *name = NULL)
	{
		LPointer p;
		LAssert(Write);

		auto name_len = Strlen(name);
		if (!CheckSpace(1 + 8 + name_len + 8 + sz))
			return false;
		p.u8 = AddressOf(Pos);
#if DEBUG_STRUCT_IO
auto type_addr = p.u8 - AddressOf();
#endif
		*p.u8++ = type;
		EncSize(p, name_len);
		if (name)
		{
			memcpy(p.u8, name, name_len);
			p.u8 += name_len;
		}
		EncSize(p, sz);
#if DEBUG_STRUCT_IO
auto data_addr = p.u8 - AddressOf();
#endif
		if (obj)
		{
			memcpy(p.u8, obj, sz);
			p.u8 += sz;
		}
		else LAssert(sz == 0);
		Pos = p.u8 - AddressOf();

#if DEBUG_STRUCT_IO
LgiTrace("Encode(%i @ %i,%i sz=%i) after=%i\n", type, (int)type_addr, (int)data_addr, (int)sz, (int)Pos);
#endif
		return true;
	}

public:
	constexpr static LVariantType StartObject	= GV_CUSTOM;
	constexpr static LVariantType EndObject		= GV_VOID_PTR;
	constexpr static LVariantType EndRow		= GV_NULL;

	LStructuredIo(bool write) :
		Write(write)
	{
	}

	bool GetWrite() { return Write; }
	size_t GetPos() { return Pos; }

	void Bool(bool &b, const char *name = NULL)
	{
		Encode(GV_BOOL, &b, sizeof(b), name);
	}

	template<typename T>
	void Int(T &i, const char *name = NULL)
	{
		Encode(GV_INT64, &i, sizeof(i), name);
	}
	
	template<typename T>
	void Float(T &f, const char *name = NULL)
	{
		Encode(GV_DOUBLE, &f, sizeof(f), name);
	}

	template<typename T>
	void String(T *str, ssize_t sz = -1, const char *name = NULL)
	{
		if (sz < 0)
			sz = Strlen(str);
		Encode(sizeof(*str) > 1 ? GV_WSTRING : GV_STRING, str, sz * sizeof(T), name);
	}

	void String(LString &str, const char *name = NULL)
	{
		Encode(GV_STRING, str.Get(), str.Length(), name);
	}

	template<typename T>
	void Binary(T *data, size_t elements, const char *name = NULL)
	{
		if (!data || elements == 0)
			return;
		Encode(GV_BINARY, data, sizeof(*data)*elements, name);
	}

	struct ObjRef
	{
		LStructuredIo *io;

		ObjRef(ObjRef &&r) : io(NULL)
		{			
			LSwap(io, r.io);
		}

		ObjRef(LStructuredIo *parent) : io(parent)
		{
		}

		~ObjRef()
		{
			if (io)
				io->Encode(EndObject);
		}
	};

	ObjRef StartObj(const char *name)
	{
		ObjRef r(this);
		Encode(StartObject, NULL, 0, name);
		return r;
	}

	bool Decode(std::function<void(LVariantType, size_t, void*, const char*)> callback, Progress *prog = NULL)
	{
		if (Length() == 0)
			return false;

		LPointer p;
		auto end = AddressOf()+Length();
		p.u8 = AddressOf(Pos);
		#define CHECK_EOF(sz) if (p.u8 + sz > end) return false
		CHECK_EOF(1);
#if DEBUG_STRUCT_IO
auto type_addr = p.u8 - AddressOf();
#endif
		LVariantType type = (LVariantType)(*p.u8++);
		if (type >= GV_MAX)
			return false;

		if (type == EndRow)
		{
			if (callback)
				callback(type, 0, NULL, NULL);
			Pos = p.u8 - AddressOf();
			return true;
		}

		size_t name_len, data_size;
		
		DecSize(p, name_len);
		CHECK_EOF(name_len);
		LString name(p.c, name_len);
		p.s8 += name_len;

		DecSize(p, data_size);
		CHECK_EOF(data_size);
#if DEBUG_STRUCT_IO
auto data_addr = p.u8 - AddressOf();
#endif
		if (callback)
			callback(type, data_size, p.u8, name);
		p.u8 += data_size;

		Pos = p.u8 - AddressOf();
#if DEBUG_STRUCT_IO
LgiTrace("Decode(%i @ %i,%i sz=%i) after=%i\n", type, (int)type_addr, (int)data_addr, (int)data_size, (int)Pos);
#endif
		return true;
	}

	bool Flush(LStream *s)
	{
		if (!s || !Write || Length() == 0)
			return false;

		(*this)[Pos++] = EndRow;
		bool Status = s->Write(AddressOf(), Pos) == Pos;

		Pos = 0;
		
		return Status;
	}
};

#define IntIo(type) inline void StructIo(LStructuredIo &io, type i) { io.Int(i); }
#define StrIo(type) inline void StructIo(LStructuredIo &io, type i) { io.String(i); }

IntIo(char)
IntIo(unsigned char)
IntIo(short)
IntIo(unsigned short)
IntIo(int)
IntIo(unsigned int)
IntIo(int64_t)
IntIo(uint64_t)

StrIo(char*);
StrIo(const char*);
StrIo(wchar_t*);
StrIo(const wchar_t*);

inline void StructIo(LStructuredIo &io, LString &s)
{
	if (io.GetWrite())
		io.String(s.Get(), s.Length());
	else
		io.Decode([&s](auto type, auto sz, auto ptr, auto name)
		{
			if (type == GV_STRING && ptr && sz > 0)
				s.Set((char*)ptr, sz);
		});
}

inline void StructIo(LStructuredIo &io, LStringPipe &p)
{
	// auto obj = io.StartObj("LStringPipe");
	if (io.GetWrite())
	{
		p.Iterate([&io](auto ptr, auto bytes)
		{
			io.String(ptr, bytes);
			return true;
		});
	}
	else
	{
		io.Decode([&p](auto type, auto sz, auto ptr, auto name)
		{
			if (type == GV_STRING && ptr && sz > 0)
				p.Write(ptr, sz);
		});
	}
}

inline void StructIo(LStructuredIo &io, LRect &r)
{
	auto obj = io.StartObj("LRect");
	io.Int(r.x1, "x1");
	io.Int(r.y1, "y1");
	io.Int(r.x2, "x2");
	io.Int(r.y2, "y2");
}

inline void StructIo(LStructuredIo &io, LColour &c)
{
	auto obj = io.StartObj("LColour");
	LString cs;
	uint8_t r, g, b, a;

	if (io.GetWrite())
	{
		cs = LColourSpaceToString(c.GetColourSpace());
		r = c.r(); g = c.g(); b = c.b(); a = c.a();
	}

	io.String(cs, "colourSpace");
	io.Int(r, "r");
	io.Int(g, "g");
	io.Int(b, "b");
	io.Int(a, "a");

	if (!io.GetWrite())
	{
		c.SetColourSpace(LStringToColourSpace(cs));
		c.r(r); c.g(g); c.b(b); c.a(a);
	}
}

#endif