/**
	\file
	\author Matthew Allen
*/

#pragma once

class LPrintEvents
{
public:
	virtual ~LPrintEvents() {}

	/*
	/// Get the number of pages in the document
	int GetPages();

	/// Get the range of pages to print that was selected by the user
	bool GetPageRange
	(
		/// You'll get pairs of ints, each pair is the start and end page
		/// number of a range to print. i.e. [5, 10], [14, 16]
		LArray<int> &p
	);
	*/

	virtual int OnBeginPrint(LPrintDC *pDC) { return 1; }
	virtual bool OnPrintPage(LPrintDC *pDC, int PageIndex) = 0;
};

/// Class to connect to a printer and start printing pages.
class LgiClass LPrinter
{
	class LPrinterPrivate *d;

public:
	LPrinter();
	virtual ~LPrinter();

	/// Browse to a printer
	bool Browse(LView *Parent);

	/// Start a print job
	bool Print
	(
		/// The event callback for pagination and printing of pages
		LPrintEvents *Events,
		/// [Optional] The name of the print job
		const char *PrintJobName = NULL,
		/// [Optional] The maximum number of pages to print
		int Pages = -1,
		/// [Optional] The parent window for the printer selection dialog
		LView *Parent = NULL
	);
	
	/// Gets any available error message...
	LString GetErrorMsg();

	/// Write the user selected printer to a string for storage
	bool Serialize(LString &Str, bool Write);
};

