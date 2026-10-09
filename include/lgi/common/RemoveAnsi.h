#pragma once

extern size_t RemoveAnsi(char *s, size_t length);
extern void RemoveAnsi(LArray<char> &a);
extern void RemoveAnsi(LString &a);

// If you need stateful parsing of ansi codes across memory blocks, use this:
// It _should_ handle partial ansi escape codes between Write calls.
class LRemoveAnsiStream : public LStream
{
	LStream *out = nullptr;
	struct AnsiParser *d;
	
public:
	LRemoveAnsiStream(LStream *outputStream);	
	~LRemoveAnsiStream();
	
	// Write data with ansi to this stream.. the non-ansi data is written to
	// the 'outputStream' passed to the constructor.
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
};