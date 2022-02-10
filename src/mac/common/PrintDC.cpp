#include "lgi/common/Lgi.h"
#include "lgi/common/GdiLeak.h"

class GPrintDCPrivate
{
public:
	int x, y;
	LPoint Dpi;
	bool PageOpen;
	bool DocOpen;
	CGContextRef Ctx;
	LString PrintJobName;
	LString PrinterName;

	GPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
		x = y = 0;
		Ctx = NULL;
	}
};

LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName) :
	LScreenDC((LPrintDcParams*)Handle)
{
	d = new GPrintDCPrivate;
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
	
	LPrintDcParams *Params = (LPrintDcParams*)Handle;
	if (Params)
	{
		#if LGI_COCOA
		#else
		int x = Params->Page.right - Params->Page.left;
		int y = Params->Page.bottom - Params->Page.top;
		
		d->Dpi.x = Params->Dpi.hRes;
		d->Dpi.y = Params->Dpi.vRes;

		d->Ctx = Params->Ctx;

		#if 0
		d->x = x * d->Dpi.x;
		d->y = y * d->Dpi.y;
		#else
		d->x = x;
		d->y = y;
		#endif

		// Invert the co-ordinate space so 0,0 is the top left corner.
		CGContextTranslateCTM(d->Ctx, 0, y);
		CGContextScaleCTM(d->Ctx, 1.0, -1.0);
		#endif
	}
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

int LPrintDC::DpiX()
{
	return d->Dpi.x;
}

int LPrintDC::DpiY()
{
	return d->Dpi.y;
}
