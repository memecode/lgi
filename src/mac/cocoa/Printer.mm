#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/Printer.h"

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	LString Printer;
	LString Err;
	
	LPrinterPrivate()
	{
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

bool LPrinter::Serialize(LString &Str, bool Write)
{
	if (Write)
	{
		Str = d->Printer;
	}
	else
	{
		d->Printer = Str;
	}
	
	return true;
}

LString LPrinter::GetErrorMsg()
{
	return d->Err;
}

#define ErrCheck(fn) \
if (e != noErr) \
{ \
	d->Err.Printf("%s:%i - %s failed with %i\n", _FL, fn, e); \
	LgiTrace(d->Err); \
	goto OnError; \
}

void LPrinter::Print(LPrintEvents *Events,
					std::function<void(int)> callback,
					const char *PrintJobName,
					int MaxPages,
					LView *Parent)
{
	int Status = LPrintEvents::OnBeginPrintError;

	if (!Events)
	{
		LAssert(0);
		if (callback)
			callback(Status);
		return;
	}

	#if LGI_COCOA

		#warning "No Cocoa Printing Impl."

	#elif LGI_CARBON // Carbon printing code?

		PMPrintSession ps = NULL;
		PMPageFormat PageFmt = NULL;
		PMPrintSettings PrintSettings = NULL;
		auto Wnd = Parent ? Parent->GetWindow() : NULL;
		Boolean Accepted = false;
		Boolean Changed = false;
		LAutoPtr<LPrintDC> dc;
		int Pages;
		LPrintDcParams Params;
		double paperWidth, paperHeight;
		PMPaper Paper = NULL;
		UInt32 ResCount;
		PMPrinter CurrentPrinter = NULL;
		
		OSStatus e = PMCreateSession(&ps);
		ErrCheck("PMCreateSession");
		
		e = PMCreatePageFormat(&PageFmt);
		ErrCheck("PMCreatePageFormat");
		
		e = PMSessionDefaultPageFormat(ps, PageFmt);
		ErrCheck("PMSessionDefaultPageFormat");
		
		e = PMCreatePrintSettings(&PrintSettings);
		ErrCheck("PMCreatePrintSettings");
		
	#if 0
		e = PMSessionUseSheets(ps, Wnd ? Wnd->WindowHandle() : NULL, NULL /*PMSheetDoneUPP sheetDoneProc*/);
		ErrCheck("PMSessionUseSheets");
	#endif
		
		e = PMSessionPrintDialog(ps, PrintSettings, PageFmt, &Accepted);
		ErrCheck("PMSessionPrintDialog");
		
		e = PMSessionValidatePrintSettings(ps, PrintSettings, &Changed);
		e = PMSessionValidatePageFormat(ps, PageFmt, &Changed);
		
		e = PMSessionBeginCGDocumentNoDialog(ps, PrintSettings, PageFmt);
		ErrCheck("PMSessionBeginCGDocumentNoDialog");
		
		e = PMSessionBeginPageNoDialog(ps, PageFmt, NULL);
		ErrCheck("PMSessionBeginPageNoDialog");
		
		e = PMGetAdjustedPaperRect(PageFmt, &Params.Page); //PMGetUnadjustedPageRect
		ErrCheck("PMGetAdjustedPaperRect");
		
		e = PMSessionGetCGGraphicsContext(ps, &Params.Ctx);
		ErrCheck("PMSessionGetCGGraphicsContext");
		
		e = PMSessionGetCurrentPrinter(ps, &CurrentPrinter);
		ErrCheck("PMSessionGetCurrentPrinter");
		
		e = PMGetPageFormatPaper(PageFmt, &Paper);
		e = PMPaperGetWidth(Paper, &paperWidth);
		e = PMPaperGetHeight(Paper, &paperHeight);
		
		e = PMPrinterGetPrinterResolutionCount(CurrentPrinter, &ResCount);
		ErrCheck("PMPrinterGetPrinterResolutionCount");
		
		for (unsigned i=0; i<ResCount; i++)
		{
			e = PMPrinterGetIndexedPrinterResolution(CurrentPrinter, i, &Params.Dpi);
		}
		
		e = PMPrinterSetOutputResolution(CurrentPrinter, PrintSettings, &Params.Dpi);
		ErrCheck("PMPrinterSetOutputResolution");
		
		dc.Reset(new GPrintDC(&Params, PrintJobName));
		Pages = Events->OnBeginPrint(dc);
		for (int Page = 0; Page < Pages; Page++)
		{
			if (Page > 0)
			{
				e = PMSessionBeginPage(ps, PageFmt, NULL);
				ErrCheck("PMSessionBeginPage");
				
				e = PMSessionGetCGGraphicsContext(ps, &Params.Ctx);
				ErrCheck("PMSessionGetCGGraphicsContext");
				
				dc.Reset(new GPrintDC(&Params, PrintJobName));
			}
			
			Status |= Events->OnPrintPage(dc, Page);
			PMSessionEndPage(ps);
		}
		
		e = PMSessionEndDocumentNoDialog(ps);
		ErrCheck("PMSessionEndDocumentNoDialog");
		
		return Status;
		
	OnError:
		PMRelease(PrintSettings);
		PMRelease(ps);
		
	#endif
	
	if (callback)
		callback(Status);
}
