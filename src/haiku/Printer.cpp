#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/Printer.h"

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	LString JobName;
	LString Printer;
	LString Err;
	LString PrinterName;
	LPrintEvents *Events;
	
	LAutoPtr<LPrintDC> PrintDC;
	
	LPrinterPrivate()
	{
		Events = NULL;
	}
	
	~LPrinterPrivate()
	{
	}
};

////////////////////////////////////////////////////////////////////
LPrinter::LPrinter()
{
	d = new LPrinterPrivate;
}

LPrinter::~LPrinter()
{
	DeleteObj(d);
}

bool LPrinter::Browse(LView *Parent)
{
	return false;
}

bool LPrinter::Serialize(::LString &Str, bool Write)
{
	if (Write)
		Str = d->Printer;
	else
		d->Printer = Str;

	return true;
}

	
LString LPrinter::GetErrorMsg()
{
	return d->Err;
}
	
void LPrinter::Print(LPrintEvents *Events,
					std::function<void(int)> callback,
					const char *PrintJobName,
					int Pages,
					LView *Parent)
{
	if (!Events)
		LgiTrace("%s:%i - Error: missing param.\n", _FL);

	LAssert(!"Impl me.");
}
