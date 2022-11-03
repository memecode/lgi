#include "Lgi.h"
#include "GdiLeak.h"

class LPrintDCPrivate
{
public:
	bool PageOpen;
	bool DocOpen;

	LPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
	}
};

LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new LPrintDCPrivate;
}

LPrintDC::~LPrintDC()
{
}

int LPrintDC::X()
{
	return 0;
}

int LPrintDC::Y()
{
	return 0;
}

int LPrintDC::GetBits()
{
	return 0;
}

int LPrintDC::DpiX()
{
	return 600;
}

int LPrintDC::DpiY()
{
	return 600;
}

