#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/Printer.h"

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	::LString JobName;
	::LString Printer;
	::LString Err;
	::LString PrinterName;
	GPrintEvents *Events;
	
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
GPrinter::GPrinter()
{
	d = new LPrinterPrivate;
}

GPrinter::~GPrinter()
{
	DeleteObj(d);
}

bool GPrinter::Browse(LView *Parent)
{
	return false;
}

bool GPrinter::Serialize(::LString &Str, bool Write)
{
	if (Write)
		Str = d->Printer;
	else
		d->Printer = Str;

	return true;
}

	
::LString GPrinter::GetErrorMsg()
{
	return d->Err;
}
	
bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages /* = -1 */, LView *Parent /* = 0 */)
{
	if (!Events)
	{
		LgiTrace("%s:%i - Error: missing param.\n", _FL);
		return false;
	}
	
    return false;
}
