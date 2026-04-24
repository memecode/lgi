#include "lgi/common/Lgi.h"
#include "lgi/common/RemoveAnsi.h"

#define FunctionChar(ch) (IsAlpha(ch) || (ch) == '~')

struct AnsiParser
{
	constexpr static int cBELL   = 0x07;
	constexpr static int cESCAPE = 0x1b;
	
	bool echoChars = true;
	uint8_t ansi[256] = "";
	int used = 0;
	
	enum TState {
		TNon,
		TEscape,
		TCsi,
		TCursor,
		TString,
	}	state = TNon;

	void parse(	std::function<void(uint8_t*,size_t)> nonAnsi,
				std::function<void(uint8_t*,size_t)> onAnsi,
				const char *in,
				size_t length)
	{
		auto s = (uint8_t*) in;
		auto *out = in;
		auto e = s + length;
		uint8_t *startNon = nullptr;
		int u = 0;
		
		auto emitNonAnsi = [&]()
			{
				if (startNon)
				{
					if (nonAnsi)
						nonAnsi(startNon, s - startNon);
					startNon = nullptr;
				}
			};
		auto emitAnsi = [&]()
			{
				if (used > 0)
				{
					if (onAnsi)
						onAnsi(ansi, used);
					ansi[used = 0] = 0;
				}
			};
		
		while (s < e)
		{
			switch (state)
			{
				case TNon:
				{
					if (*s == cBELL)
					{
						emitNonAnsi();
					}
					else if (*s == 0x9c && echoChars == false) // I'm not sure if this is correct...
					{
						echoChars = true;
					}
					else if (*s == cESCAPE)
					{
						emitNonAnsi();
						
						// Start the escape sequence:
						state = TEscape;
						used = 0;
						ansi[used++] = *s;
					}
					else if (!startNon && echoChars)
					{
						startNon = s;
					}
					break;
				}
				case TEscape:
				{
					ansi[used++] = *s;
					
					// What type of escape is it?
					if (*s == '[' || *s == 0x9B)
					{
						state = TCsi; // Control Sequence Introducer
					}
					else if (*s == '7' || *s == '8')
					{
						// Save/restore cursor
						state = TCursor;
					}
					else if
					(
						(*s == 'P' || *s == 0x90) // Device Control String
						||
						(*s == ']' || *s == 0x9D) // Operating System Command
						||
						(*s == 'X') // Start of String
						||
						(*s == '^') // Privacy message
						||
						(*s == '_') // App command
					)
					{				
						echoChars = false;
						state = TString;
					}
					else
					{
						// Not a valid ANSI?
						state = TNon;
					}
					break;
				}
				case TCsi: // Control Sequence Introducer
				{
					if (FunctionChar(*s))
					{
						emitAnsi();
						state = TNon;
					}
					else if (used < sizeof(ansi))
					{
						ansi[used++] = *s;
					}
					break;
				}
				case TCursor:
				{
					ansi[used++] = *s;
					emitAnsi();
					state = TNon;
					break;
				}
				case TString:
				{
					if (*s == 0x9C)
					{
						emitAnsi();
						state = TNon;
					}
					break;
				}
			}
			
			s++;
		}
		
		emitNonAnsi();
	}
};

void RemoveAnsi(LArray<char> &a)
{
	auto newSize = RemoveAnsi(a.AddressOf(), a.Length());
	a.Length(newSize);
}

void RemoveAnsi(LString &a)
{
	auto newSize = RemoveAnsi(a.Get(), a.Length());
	a.Length(newSize);
}

size_t RemoveAnsi(char *in, size_t length)
{
	if (!in)
		return 0;

	AnsiParser p;
	char *out = in;
	
	p.parse([&](
		// non-ansi cb:
		auto ptr, auto sz)
		{
			memcpy(out, ptr, sz);
			out += sz;
		},
		// ansi cb:
		nullptr,
		in,
		length);
	
	// null-terminate string
	*out = 0;
	
	return out - in;
}	

////////////////////////////////////////////////////////////////////////////////////////////////////////
LRemoveAnsiStream::LRemoveAnsiStream(LStream *outputStream)
{
	out = outputStream;
	d = new AnsiParser;
}

LRemoveAnsiStream::~LRemoveAnsiStream()
{
	delete d;
}

ssize_t LRemoveAnsiStream::LRemoveAnsiStream::Write(const void *Ptr, ssize_t Size, int Flags)
{
	ssize_t processed = 0;
	
	d->parse(
		// non-ansi cb:
		[&](auto ptr, auto sz)
		{
			if (out)
				out->Write(ptr, sz);
			processed += sz;
		},
		// ansi cb:
		nullptr,
		(char*)Ptr, Size);
		
	return processed;
}
