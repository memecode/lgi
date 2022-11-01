#include "lgi/common/Lgi.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Printer.h"

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	PRINTDLG Info;
	LString Err;
	bool NeedsDC;

	LPrinterPrivate()
	{
		NeedsDC = false;
		ZeroObj(Info);
		Info.lStructSize = sizeof(Info);
	}
};

//////////////////////////////////////////////////////////////////////
LPrinter::LPrinter()
{
	d = new LPrinterPrivate;
}

LPrinter::~LPrinter()
{
	DeleteObj(d);
}

LString LPrinter::GetErrorMsg()
{
	return d->Err;
}

#define PrintStatus(val) \
	{ if (callback) callback(val); return; }

void LPrinter::Print(LPrintEvents *Events, std::function<void(int)> callback, const char *PrintJobName, int Pages, LView *Parent)
{
	if (!Events)
		PrintStatus(LPrintEvents::OnBeginPrintError);
		
	if
	(
		Parent
		&&
		(
			!d->Info.hDevMode
			||
			!d->Info.hDevNames
		)
	)
	{
		d->NeedsDC = true;
		bool r = Browse(Parent);
		d->NeedsDC = false;
		if (!r)
			PrintStatus(LPrintEvents::OnBeginPrintError);
	}
	else
	{
		d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
		d->Info.Flags = PD_RETURNDC;
		d->Info.hDC = 0;
		if (!PrintDlg(&d->Info))
			PrintStatus(LPrintEvents::OnBeginPrintError);
	}

	if (!d->Info.hDC)
		PrintStatus(LPrintEvents::OnBeginPrintError);
	
	LString PrinterName;
	if (d->Info.hDevNames)
	{
		DEVNAMES *Name = (DEVNAMES*)GlobalLock(d->Info.hDevNames);
		if (Name)
		{
			// const char *Driver = (const char *)Name + Name->wDriverOffset;
			const char *Device = (const char *)Name + Name->wDeviceOffset;
			// const char *Output = (const char *)Name + Name->wOutputOffset;
			// const char *Default = (const char *)Name + Name->wDefault;			
			PrinterName = Device;			
			GlobalUnlock(Name);
		}
	}

	LPrintDC dc(d->Info.hDC, PrintJobName, PrinterName);
	if (!dc.Handle())
	{
		d->Err.Printf("%s:%i - StartDoc failed.\n", _FL);
		PrintStatus(LPrintEvents::OnBeginPrintError);
	}	
	
	Events->OnBeginPrint(&dc, [&](auto JobPages)
	{
		if (JobPages <= LPrintEvents::OnBeginPrintCancel)
			PrintStatus(JobPages);

		bool Status = false;
		DOCINFO Info;
		LAutoWString DocName(Utf8ToWide(PrintJobName ? PrintJobName : "Lgi Print Job"));

		ZeroObj(Info);
		Info.cbSize = sizeof(DOCINFO); 
		Info.lpszDocName = DocName; 

		if (Pages > 0)
			JobPages = min(JobPages, Pages);
		
		auto PageRanges = Events->GetPageRanges();
		for (int i=0; i<JobPages; i++)
		{
			if (!PageRanges || PageRanges->InRanges(i + 1))
			{
				if (StartPage(dc.Handle()) > 0)
				{
					Status |= Events->OnPrintPage(&dc, i);
					EndPage(dc.Handle());
				}
				else
				{
					d->Err.Printf("%s:%i - StartPage failed.", _FL);
					JobPages = LPrintEvents::OnBeginPrintError;
					break;
				}
			}
		}
	
		LString OutputFile = dc.GetOutputFileName();
		if (LFileExists(OutputFile))
			LBrowseToFile(OutputFile);

		PrintStatus(JobPages);
	});	
}

bool LPrinter::Browse(LView *Parent)
{
	d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
	d->Info.Flags = PD_PRINTSETUP | PD_PAGENUMS;
	if (d->NeedsDC)
		d->Info.Flags |= PD_RETURNDC;
	/*
	if (d->Pages > 1)
	{
		d->Info.nMinPage = 1;
		d->Info.nMaxPage = d->Pages;
	}
	*/

	return PrintDlg(&d->Info) != 0;
}

/*
bool GPrinter::GetPageRange(LArray<int> &p)
{
	if (d->Info.Flags & PD_PAGENUMS)
	{
		p[0] = d->Info.nFromPage;
		p[1] = d->Info.nToPage;

		return true;
	}

	return false;
}
*/

#define MAGIC_PRINTDLG					0xAAFF0100
#define MAGIC_DEVMODE					0xAAFF0101
#define MAGIC_DEVNAMES					0xAAFF0102

bool LPrinter::Serialize(LString &Str, bool Write)
{
	int Size = sizeof(d->Info);
	if (Write)
	{
		GMem DevMode(d->Info.hDevMode);
		GMem DevNames(d->Info.hDevNames);
		DEVMODE *Mode = (DEVMODE*) DevMode.Lock();
		DEVNAMES *Names = (DEVNAMES*) DevNames.Lock();
		LMemQueue Temp;
		int32 m;

		// Dlg
		m = MAGIC_PRINTDLG;
		Temp.Write(&m, sizeof(m));
		m = sizeof(d->Info);
		Temp.Write(&m, sizeof(m));
		Temp.Write((uchar*)&d->Info, m);

		// Mode
		if (Mode)
		{
			m = MAGIC_DEVMODE;
			Temp.Write(&m, sizeof(m));
			m = (int32) DevMode.GetSize();
			Temp.Write(&m, sizeof(m));
			Temp.Write((uchar*) Mode, m);
		}

		// Names
		if (Names)
		{
			m = MAGIC_DEVNAMES;
			Temp.Write(&m, sizeof(m));
			m = (int32) DevNames.GetSize();
			Temp.Write(&m, sizeof(m));
			Temp.Write((uchar*) Names, m);
		}

		DevMode.Detach();
		DevNames.Detach();

		// Convert to base64
		auto BinLen = Temp.GetSize();
		uchar *Bin = new uchar[BinLen];
		if (Bin && Temp.Read(Bin, BinLen))
		{
			auto Base64Len = BufferLen_BinTo64(BinLen);
			char *Base64 = new char[Base64Len+1];
			if (Base64)
			{
				memset(Base64, 0, Base64Len+1);
				ConvertBinaryToBase64(Base64, Base64Len, Bin, BinLen);
				Str = Base64;
			}
			return true;
		}
	}
	else
	{
		bool Status = false;

		ZeroObj(d->Info);
		d->Info.lStructSize = Size;
		if (Str)
		{
			// Convert to binary
			LMemQueue Temp;

			auto Base64Len = Str.Length();
			auto BinaryLen = BufferLen_64ToBin(Base64Len);
			uchar *Binary = new uchar[BinaryLen];
			if (Binary)
			{
				auto Len = ConvertBase64ToBinary(Binary, BinaryLen, Str, Base64Len);
				Temp.Write(Binary, Len);

				bool Done = false;
				do
				{
					int Type = 0;
					int Size = 0;

					if (Temp.Read(&Type, sizeof(Type)) &&
						Temp.Read(&Size, sizeof(Size)))
					{
						switch (Type)
						{
							case MAGIC_PRINTDLG:
							{
								if (Size == sizeof(d->Info))
								{
									Temp.Read((uchar*) &d->Info, Size);
									d->Info.hDevMode = 0;
									d->Info.hDevNames = 0;
									Status = true;
								}
								else
								{
									// Grrrr: Error
									Done = true;
								}
								break;
							}
							case MAGIC_DEVMODE:
							{
								GMem DevMode(Size);
								DEVMODE *Mode = (DEVMODE*) DevMode.Lock();
								if (Mode)
								{
									Temp.Read((uchar*)Mode, Size);
									d->Info.hDevMode = DevMode.Handle();
									DevMode.Detach();
								}
								break;
							}
							case MAGIC_DEVNAMES:
							{
								GMem DevNames(Size);
								DEVNAMES *Names = (DEVNAMES*) DevNames.Lock();
								if (Names)
								{
									Temp.Read((uchar*)Names, Size);
									d->Info.hDevNames = DevNames.Handle();
									DevNames.Detach();
								}
								break;
							}
							default:
							{
								// Eof
								Done = true;
								break;
							}
						}
					}
					else
					{
						Done = true;
					}
				}
				while (!Done);

				DeleteArray(Binary);
			}
		}

		return Status;
	}

	return false;
}