#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "lgi/common/Button.h"
#include "lgi/common/Printer.h"

using namespace Gtk;

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	GtkPrintOperation *Op;
	GtkPrintSettings *Settings;
	LString JobName;
	LString Printer;
	LString Err;
	LString PrinterName;
	LPrinter::Context *Events;
	
	LAutoPtr<LPrintDC> PrintDC;
	
	LPrinterPrivate()
	{
		Settings = NULL;
		Events = NULL;
		Op = NULL;
	}
	
	~LPrinterPrivate()
	{
		if (Op)
			g_object_unref(Op);
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

bool LPrinter::Browse(LView *Parent, PageOrientation ori)
{
	if (d->Settings != NULL)
		Gtk::gtk_print_operation_set_print_settings(d->Op, d->Settings);
	
	return false;
}

bool LPrinter::Serialize(LString &Str, bool Write)
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

static void
GtkPrintBegin(	GtkPrintOperation	*operation,
				GtkPrintContext		*context,
				LPrinterPrivate		*d)
{
	if (auto settings = gtk_print_operation_get_print_settings(operation))
	{
		d->PrinterName = gtk_print_settings_get_printer(settings);
	}

	if (!d->PrintDC.Reset(new LPrintDC(context, d->JobName, d->PrinterName)))
		return;
		
	d->Events->OnBeginPrint(d->PrintDC, [&](auto Pages)
	{
		if (Pages > 0)
			gtk_print_operation_set_n_pages(d->Op, Pages);
		else
			gtk_print_operation_cancel(d->Op);
	});
}
	
static void
GtkPrintDrawPage(	GtkPrintOperation	*operation,
					GtkPrintContext		*context,
					gint				page_number,
					LPrinterPrivate		*d)
{
	cairo_t *ct = gtk_print_context_get_cairo_context(context);
	if (ct &&
		d->PrintDC &&
		d->PrintDC->Handle() != ct)
	{
		// Update to the new handle?
		d->PrintDC->SetHandle(ct);
	}
	
	d->Events->OnPrintPage(d->PrintDC, page_number);
}

void LPrinter::Print(	LPrinter::Context *Events,
						std::function<void(int)> Callback,
						const char *PrintJobName,
						int Pages,
						LView *Parent)
{
	if (!Events)
	{
		LgiTrace("%s:%i - Error: missing param.\n", _FL);
		return;
	}
	
	d->Op = gtk_print_operation_new();
	if (!d->Op)
	{
		LgiTrace("%s:%i - Error: gtk_print_operation_new failed.\n", _FL);
		return;
	}
		
	GError *Error = nullptr;
	GtkPrintOperationResult Result;
	GtkWindow *Wnd = nullptr;
	
	if (Parent)
	{
		if (auto w = Parent->GetWindow())
			Wnd = w->WindowHandle();
	}
	
	d->Events = Events;
	d->JobName = PrintJobName;

	g_signal_connect(G_OBJECT(d->Op), "begin-print", G_CALLBACK(GtkPrintBegin), d);
	g_signal_connect(G_OBJECT(d->Op), "draw-page", G_CALLBACK(GtkPrintDrawPage), d);
	
	gtk_print_operation_set_job_name(d->Op, PrintJobName);
	gtk_print_operation_set_embed_page_setup(d->Op, true); // allow customization of page setup
	
	if (Events)
	{
		auto orientation = Events->GetOrientation();
		if (orientation != PoDefault)
		{
			auto pageSetup = gtk_print_operation_get_default_page_setup(d->Op);
			if (!pageSetup)
			{
				pageSetup = gtk_page_setup_new();
			}			
			auto settings = gtk_print_operation_get_print_settings(d->Op);
			if (!settings)
			{
				settings = gtk_print_settings_new();
			}
			if (settings || pageSetup)
			{
				switch (orientation)
				{
					case PoPortrait:
						if (pageSetup)
							gtk_page_setup_set_orientation(pageSetup, GTK_PAGE_ORIENTATION_PORTRAIT);
						if (settings)
							gtk_print_settings_set_orientation(settings, GTK_PAGE_ORIENTATION_PORTRAIT);
						break;
					case PoLandscape:
						if (pageSetup)
							gtk_page_setup_set_orientation(pageSetup, GTK_PAGE_ORIENTATION_LANDSCAPE);
						if (settings)
							gtk_print_settings_set_orientation(settings,  GTK_PAGE_ORIENTATION_LANDSCAPE);
						break;
				}
				
				if (pageSetup)
				{
					gtk_print_operation_set_default_page_setup(d->Op, pageSetup);
					g_object_unref(pageSetup);
				}
				if (settings)
				{
					gtk_print_operation_set_print_settings(d->Op, settings);
					g_object_unref(settings);
				}
			}
		}
	}
	
	Result = gtk_print_operation_run(d->Op,
									GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
									Wnd,
									&Error);    
    
    bool Status = Result != GTK_PRINT_OPERATION_RESULT_ERROR;
    if (!Status)
    {
		if (Error)
		{
			LgiTrace("%s:%i - gtk_print_operation_run failed with: %i - %s\n",
					_FL,
					Error->code,
					Error->message);
		}
		else
		{
			LgiTrace("%s:%i - gtk_print_operation_run failed with: unknown error\n", _FL);
		}
    }

	if (d->Op)
	{
		g_object_unref(d->Op);
		d->Op = nullptr;
	}
    
    return;
}
