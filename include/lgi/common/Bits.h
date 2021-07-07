#pragma once

template<typename T = uint8_t, int pBits = sizeof(T)<<3>
struct LBits
{
    T *p;
    int remaining;
	ssize_t bytes;

    LBits(void *ptr, ssize_t size = -1)
    {
        p = (T*)ptr;
        remaining = 8;
		bytes = size;
    }

	ssize_t Length() // In bytes
	{
		return bytes < 0 ? -1 : bytes - (remaining == 0);
	}

	ssize_t BitLength() // Bits remaining
	{
		return bytes < 0 ? -1 : bytes * pBits + remaining;
	}

	void ByteAlign() // Seek to next byte boundary
	{
		if (remaining < pBits)
		{
			remaining = pBits;
			p++;
			if (bytes >= 0)
				bytes--;			
		}
	}

    uint64 Read(int bits)
    {
        uint64 v = 0;
		assert(bits <= 64);
		if (bytes == 0) // End of buffer?
			return v;

        while (bits > 0)
        {
            if (remaining == 0)
            {
				// If we're at the end, exit loop safely.
				if (bytes >= 0 && --bytes == 0)
					return v;
                p++;
                remaining = pBits;
            }

            auto rd = MIN(remaining, bits);
			v <<= rd; // make space
            v |= (*p >> (remaining-rd)) & ((1<<rd)-1); // copy bits
            
			bits -= rd;
            remaining -= rd;
        }

        return v;
    }
};
