#include "lgi/common/Lgi.h"

class LPrintDCPrivate
{
public:
	int x = 0, y = 0;
	LPoint Dpi;
	bool PageOpen = false;
	bool DocOpen = false;
	CGContextRef Ctx = nullptr;
	LString PrintJobName;
	LString PrinterName;

};

LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName) :
	LScreenDC((LPrintDcParams*)Handle)
{
	d = new LPrintDCPrivate;
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
}

LPrintDC::~LPrintDC()
{
	DeleteObj(d);
}

int LPrintDC::X()
{
	return d->x;
}

int LPrintDC::Y()
{
	return d->y;
}

int LPrintDC::GetBits()
{
	return 24;
}

LPoint LPrintDC::GetDpi()
{
	return d->Dpi;
}
