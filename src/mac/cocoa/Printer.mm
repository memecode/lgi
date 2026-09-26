#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/Printer.h"
#include "lgi/common/Json.h"

#include <algorithm>

#import <Cocoa/Cocoa.h>

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	LString Printer;
	LString JobName;
	LString Err;
	NSPrintInfo *PrintInfo = nil;
	
	LPrinterPrivate()
	{
		PrintInfo = [[NSPrintInfo sharedPrintInfo] retain];
	}
	
	~LPrinterPrivate()
	{
		if (PrintInfo)
			[PrintInfo release];
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

bool LPrinter::Browse(LView *Parent, PageOrientation Po)
{
	NSPrintInfo *info = d->PrintInfo ? d->PrintInfo : [NSPrintInfo sharedPrintInfo];
	if (Po == PoLandscape)
		[info setOrientation:NSPaperOrientationLandscape];
	else if (Po == PoPortrait)
		[info setOrientation:NSPaperOrientationPortrait];
	else
		[info setOrientation:NSPaperOrientationPortrait];
	
	NSPrintPanel *panel = [NSPrintPanel printPanel];
	NSInteger result = [panel runModalWithPrintInfo:info];
	if (result == NSModalResponseOK)
	{
		d->PrintInfo = info;
		if (auto p = [info printer])
			d->Printer = [[p name] UTF8String];
		return true;
	}
	
	return false;
}

bool LPrinter::Serialize(LString &Str, bool Write)
{
	if (Write)
	{
		LJson j;
		j.Set("printer", d->Printer);
		if (d->PrintInfo)
		{
			if (!d->JobName.IsEmpty())
				j.Set("jobName", d->JobName);

			NSPaperOrientation orient = [d->PrintInfo orientation];
			const char *orientName = "default";
			if (orient == NSPaperOrientationLandscape)
				orientName = "landscape";
			else if (orient == NSPaperOrientationPortrait)
				orientName = "portrait";
			j.Set("orientation", orientName);

			NSSize paper = [d->PrintInfo paperSize];
			j.Set("paperWidth", (int64_t)paper.width);
			j.Set("paperHeight", (int64_t)paper.height);
			j.Set("scalingFactor", [d->PrintInfo scalingFactor]);
			NSNumber *copies = [[d->PrintInfo dictionary] objectForKey:NSPrintCopies];
			if (copies)
				j.Set("copies", (int64_t)[copies integerValue]);
		}
		Str = j.GetJson();
		return true;
	}
	else
	{
		LJson j(Str);
		d->Printer = j.Get("printer");
		if (!d->PrintInfo)
			d->PrintInfo = [[NSPrintInfo sharedPrintInfo] retain];

		LString orient = j.Get("orientation");
		if (orient == "landscape")
			[d->PrintInfo setOrientation:NSPaperOrientationLandscape];
		else if (orient == "portrait")
			[d->PrintInfo setOrientation:NSPaperOrientationPortrait];
		else
			[d->PrintInfo setOrientation:NSPaperOrientationPortrait];

		LString jobName = j.Get("jobName");
		if (!jobName.IsEmpty())
			d->JobName = jobName;

		LString w = j.Get("paperWidth");
		if (!w.IsEmpty())
			[d->PrintInfo setPaperSize:NSMakeSize(w.Float(), j.Get("paperHeight").Float())];

		LString copies = j.Get("copies");
		if (!copies.IsEmpty())
			[[d->PrintInfo dictionary] setObject:[NSNumber numberWithInteger:(NSInteger)copies.Int()] forKey:NSPrintCopies];

		LString scale = j.Get("scalingFactor");
		if (!scale.IsEmpty())
			[d->PrintInfo setScalingFactor:scale.Float()];

		if (auto p = [d->PrintInfo printer])
			d->Printer = [[p name] UTF8String];
		return true;
	}
}

LString LPrinter::GetErrorMsg()
{
	return d->Err;
}

void LPrinter::Print(
		/// The event callback for pagination and printing of pages
		Context *Events,
		/// The status callback
		std::function<void(int)> callback,
		/// [Optional] The name of the print job
		const char *PrintJobName,
		/// [Optional] The maximum number of pages to print
		int Pages,
		/// [Optional] The parent window for the printer selection dialog
		LView *Parent
		)
{
	int Status = LPrinter::Context::OnBeginPrintError;

	if (!Events)
	{
		LAssert(0);
		if (callback)
			callback(Status);
		return;
	}

	NSPrintInfo *info = d->PrintInfo ? d->PrintInfo : [[NSPrintInfo sharedPrintInfo] retain];
	if (PrintJobName && *PrintJobName)
		d->JobName = PrintJobName;
	if (!d->PrintInfo)
		d->PrintInfo = info;
	
	switch (Events->GetOrientation())
	{
		case PoPortrait:
			[info setOrientation:NSPaperOrientationPortrait];
			break;
		case PoLandscape:
			[info setOrientation:NSPaperOrientationLandscape];
			break;
		default:
			break;
	}

	// Create a minimal print DC object for the callback-based LPrintDC lifecycle.
	// The actual Cocoa page context is produced by NSPrintOperation during the
	// print run, and a real CGContext-backed LPrintDC implementation would need
	// to be added to the mac/ Cocoa path as a follow-up.
	auto *dc = new LPrintDC(nullptr, PrintJobName ? PrintJobName : "Lgi Print Job", d->Printer ? d->Printer : "");
	if (!dc)
	{
		if (callback)
			callback(Status);
		return;
	}

	Events->OnBeginPrint(dc,
		[this, Events, callback, dc, Pages](int JobPages)
		{
			if (JobPages <= LPrinter::Context::OnBeginPrintCancel)
			{
				if (callback)
					callback(JobPages);
				delete dc;
				return;
			}
		
			int PageCount = (Pages > 0) ? std::min(JobPages, Pages) : JobPages;
			auto *Ranges = Events->GetPageRanges();
			for (int i = 0; i < PageCount; ++i)
			{
				if (Ranges && !Ranges->InRanges(i + 1))
					continue;
				Events->OnPrintPage(dc, i);
			}
		
			if (callback)
				callback(JobPages);
			delete dc;
		});
}
