#include "Lgi.h"
#include "UnitTests.h"
#include "LBits.h"

bool LBitsTest::Run()
{
	uint8_t Data[] = {0xf8, 0xcc, 0x1d, 0x00};
	LBits<> Bits(Data, sizeof(Data));

	if (Bits.Read(5) != 0x1f)
		return false;

	if (Bits.Read(5) != 3)
		return false;

	if (Bits.Read(7) != 0x18)
		return false;

	if (Bits.Read(3) != 1)
		return false;

	if (Bits.Read(4) != 0xd)
		return false;

	if (Bits.Length() != 1)
		return false;

	return true;
}
