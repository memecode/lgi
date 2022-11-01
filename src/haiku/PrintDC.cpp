#include "lgi/common/Lgi.h"

#define PS_SCALE			10

///////////////////////////////////////////////////////////////////////////////////////
class GPrintDCPrivate // : public GCups
{
public:
	class PrintPainter *p;
	LString PrintJobName;
	LString PrinterName;
	int Pages;
	LColour c;
	LRect Clip;
	
	GPrintDCPrivate()
	{
		p = 0;
		Pages = 0;
	}
	
	~GPrintDCPrivate()
	{
	}
	
	bool IsOk()
	{
		#ifndef __clang__
		return	this != 0;
		#else
		return true;
		#endif
	}
};

/////////////////////////////////////////////////////////////////////////////////////
LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new GPrintDCPrivate();
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
	ColourSpace = CsRgb24;
	d->Clip = Bounds();
}

LPrintDC::~LPrintDC()
{
	DeleteObj(d);
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
	return 24;
}

LPoint LPrintDC::GetDpi()
{
	return LPoint(100, 100);
}
