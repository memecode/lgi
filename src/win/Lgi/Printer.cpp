#include "lgi/common/Lgi.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Printer.h"

////////////////////////////////////////////////////////////////////
class LPrinterPrivate
{
public:
	PRINTDLGW Info;
	LString Err;
	bool NeedsDC = false;

	LPrinterPrivate()
	{
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

void LPrinter::Print(LPrinter::Context *Events, std::function<void(int)> callback, const char *PrintJobName, int Pages, LView *Parent)
{
	if (!Events)
		PrintStatus(Context::OnBeginPrintError);
		
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
		bool r = Browse(Parent, Events->GetOrientation());
		d->NeedsDC = false;
		if (!r)
			PrintStatus(Context::OnBeginPrintCancel);
	}
	else
	{
		d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
		d->Info.Flags = PD_RETURNDC;
		d->Info.hDC = 0;
		if (!PrintDlg(&d->Info))
			PrintStatus(Context::OnBeginPrintError);
	}

	if (!d->Info.hDC)
		PrintStatus(Context::OnBeginPrintError);
	
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

	auto dc = new LPrintDC(d->Info.hDC, PrintJobName, PrinterName);
	if (!dc || !dc->Handle())
	{
		if (dc)
			delete dc;
		d->Err.Printf("%s:%i - StartDoc failed.\n", _FL);
		PrintStatus(Context::OnBeginPrintError);
	}	
	
	Events->OnBeginPrint(dc,
		[this, dc, Events, callback, PrintJobName=LString(PrintJobName), Pages](auto JobPages)
		{
			LAutoPtr<LPrintDC> ownDc(dc); // Make sure we free the dc

			if (JobPages <= Context::OnBeginPrintCancel)
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
					if (StartPage(dc->Handle()) > 0)
					{
						Status |= Events->OnPrintPage(dc, i);
						EndPage(dc->Handle());
					}
					else
					{
						d->Err.Printf("%s:%i - StartPage failed.", _FL);
						JobPages = Context::OnBeginPrintError;
						break;
					}
				}
			}
	
			LString OutputFile = dc->GetOutputFileName();
			if (LFileExists(OutputFile))
				LBrowseToFile(OutputFile);

			PrintStatus(JobPages);
		});	
}

bool LPrinter::Browse(LView *Parent, PageOrientation Po)
{
	d->Info.hwndOwner = Parent ? Parent->Handle() : 0;
	d->Info.Flags = PD_PRINTSETUP | PD_PAGENUMS;

	if (Po != PoDefault)
	{
		if (!d->Info.hDevMode)
		{
			d->Info.Flags |= PD_RETURNDEFAULT;
			PrintDlg(&d->Info);
			d->Info.Flags &= ~PD_RETURNDEFAULT;
		}
		if (d->Info.hDevMode)
		{
			// This setting is ignored in my experience (windows 10):
			LGlobalMem DevMode(d->Info.hDevMode);
			if (auto pDevMode = (DEVMODEW*)DevMode.Lock())
			{
				switch (Po)
				{
					case PoLandscape:
						pDevMode->dmFields |= DM_ORIENTATION;
						pDevMode->dmOrientation = DMORIENT_LANDSCAPE;
						break;
					case PoPortrait:
						pDevMode->dmFields |= DM_ORIENTATION;
						pDevMode->dmOrientation = DMORIENT_PORTRAIT;
						break;
				}
				DevMode.UnLock();
			}
		}

		// So instead we have to do this:
		// Catch the WM_INITDIALOG message and simulate a "click" on the right orientation.
		// I mean what could go wrong?
		d->Info.Flags |= PD_ENABLESETUPHOOK;
		d->Info.lCustData = Po;
		d->Info.lpfnSetupHook = [](auto hwnd, auto msg, auto wparam, auto lparam)
			{
				switch (msg)
				{
					case WM_INITDIALOG:
					{
						enum CtrlIds {
							ID_PORTRAIT  = 0x420,
							ID_LANDSCAPE = 0x421,
							ID_GROUP     = 0x430,
						};
						if (auto pd = (PRINTDLGW*)lparam)
						{
							HWND ctrls[] = { GetDlgItem(hwnd, ID_PORTRAIT),
											 GetDlgItem(hwnd, ID_LANDSCAPE) };
							
							auto setVal = [&](int idx)
								{
									#if 1
										if (ctrls[idx])
										{
											LPARAM mouseLoc = MAKELONG(3, 3);
											SendMessage(ctrls[idx], WM_LBUTTONDOWN, MK_LBUTTON, mouseLoc);
											SendMessage(ctrls[idx], WM_LBUTTONUP, 0, mouseLoc);
										}
										else LAssert(!"no ctrl?");
									#else
	.									// This doesn't work, because of course it doesn't...
										for (int i=0; i<CountOf(ctrls); i++)
											if (ctrls[i])
												SendMessage(ctrls[i], BM_SETCHECK, i == idx ? BST_CHECKED : BST_UNCHECKED, 0);
									#endif
								};

							switch ((PageOrientation) pd->lCustData)
							{
								case LPrinter::PoPortrait:
								{
									setVal(0);
									break;
								}
								case LPrinter::PoLandscape:
								{
									setVal(1);
									break;
								}
								default:
									break;
							}
						}
						break;
					}
					default:
						// LgiTrace("lpfnSetupHook %p, %i, %i, %i\n", hwnd, msg, wparam, lparam);
						break;
				}
				return (UINT_PTR)FALSE; // let the print dialog handle the message
			};
	}

	if (d->NeedsDC)
		d->Info.Flags |= PD_RETURNDC;

	return PrintDlgW(&d->Info) != 0;
}

#define MAGIC_PRINTDLG					0xAAFF0100
#define MAGIC_DEVMODE					0xAAFF0101
#define MAGIC_DEVNAMES					0xAAFF0102

bool LPrinter::Serialize(LString &Str, bool Write)
{
	DWORD Size = sizeof(d->Info);
	if (Write)
	{
		LGlobalMem DevMode(d->Info.hDevMode);
		LGlobalMem DevNames(d->Info.hDevNames);
		auto Mode = (DEVMODE*) DevMode.Lock();
		auto Names = (DEVNAMES*) DevNames.Lock();
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
								LGlobalMem DevMode(Size);
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
								LGlobalMem DevNames(Size);
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