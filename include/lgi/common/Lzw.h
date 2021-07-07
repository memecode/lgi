#ifndef _LZW_H_
#define _LZW_H_

class Lzw
{
	class LzwPrivate *d;

	int find_match(int hash_prefix, uint hash_character);
	void output_code(LStream *output, uint code);
	int input_code(LStream *input);
	uchar *decode_string(unsigned char *buffer, unsigned int code);
	
public:
	int MeterBlock;
	Progress *Meter;

	Lzw();
	virtual ~Lzw();

	void SetBufSize(int i);
	bool Decompress(LStream *out, LStream *in);
	bool Compress(LStream *out, LStream *in);
};

#endif