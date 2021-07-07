#include "lgi/common/Lgi.h"
#include "lgi/common/GdiLeak.h"

class GPrintDCPrivate
{
public:
	bool PageOpen;
	bool DocOpen;
	LString OutputFileName;

	GPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
	}
};

BOOL CALLBACK LgiAbortProc(HDC hdc, int iError)
{
	LgiYield();
	return true;
}

LPrintDC::LPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new GPrintDCPrivate;
	hDC = (HDC) Handle;

	SetAbortProc(hDC, LgiAbortProc);
	
	// Start document
	if (hDC)
	{
		DOCINFO Info;
		LAutoWString OutName;
		LAutoWString DocName(Utf8ToWide(PrintJobName ? PrintJobName : "Lgi Print Job"));

		ZeroObj(Info);
		Info.cbSize = sizeof(DOCINFO); 
		Info.lpszDocName = DocName;
		
		if (PrinterName &&
			stristr(PrinterName, "XPS"))
		{
			LFile::Path p(LSP_USER_DOCUMENTS);
			LString FileName;
			FileName.Printf("%s.xps", PrintJobName);
			p += FileName;
			if (LFileExists(p.GetFull()))
			{
				for (unsigned i=1; i<1000; i++)
				{
					p--;
					FileName.Printf("%s%i.xps", PrintJobName, i);
					p += FileName;
					if (!LFileExists(p.GetFull()))
						break;
				}
			}
			
			d->OutputFileName = p.GetFull();
			OutName.Reset(Utf8ToWide(d->OutputFileName));
			Info.lpszOutput = OutName;
		}

		d->DocOpen = StartDoc(hDC, &Info) > 0;
		if (d->DocOpen)
		{
			SetSize(X(), Y());
		}
		else
		{
			DeleteDC(hDC);
			hDC = 0;
		}
	}
}

LPrintDC::~LPrintDC()
{
	// EndPage();

    if (hDC)
    {
		if (d->DocOpen)
		{
			EndDoc(hDC);
		}

	    DeleteDC(hDC);
		hDC = 0;
    }
}

const char *LPrintDC::GetOutputFileName()
{
	return d->OutputFileName;
}

int LPrintDC::X()
{
	return GetDeviceCaps(hDC, HORZRES);
}

int LPrintDC::Y()
{
	return GetDeviceCaps(hDC, VERTRES);
}

int LPrintDC::GetBits()
{
	return GetDeviceCaps(hDC, BITSPIXEL);
}

int LPrintDC::DpiX()
{
	return GetDeviceCaps(hDC, LOGPIXELSX); 
}

int LPrintDC::DpiY()
{
	return GetDeviceCaps(hDC, LOGPIXELSY);
}

/*
bool LPrintDC::StartPage()
{
	bool Status = false;
	if (hDC && d->DocOpen)
	{
		EndPage();
		d->PageOpen = Status = ::StartPage(hDC) > 0;
	}

	return Status;
}

void LPrintDC::EndPage()
{
	if (hDC && d->PageOpen)
	{
		::EndPage(hDC);
		d->PageOpen = false;
	}
}
*/

