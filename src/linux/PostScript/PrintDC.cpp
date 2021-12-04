#include "Lgi.h"
// #include <cups/ipp.h>
// #include <cups/cups.h>

#define PS_SCALE			10

///////////////////////////////////////////////////////////////////////////////////////
#include "GLibraryUtils.h"

/*
class GCups : public GLibrary
{
public:
	GCups() : GLibrary("libcups")
	{
		if (!IsLoaded())
		{
			printf("%s,%i - CUPS (Common Unix Printing System) not found.\n", __FILE__, __LINE__);
		}
	}
	
	~GCups() {}

	DynFunc5(int, cupsPrintFile,
			const char *, printer,
			const char *, filename,
			const char *, title,
			int, num_options,
			cups_option_t *, options);
	DynFunc0(int, cupsLastError);
	DynFunc1(int, cupsGetDests, cups_dest_t **, dests);
	DynFunc4(cups_dest_t *, cupsGetDest, const char *, name,
			const char *, instance, int, num_dests, cups_dest_t *, dests);
	DynFunc4(int, cupsAddOption, const char *, name, const char *, value, int, num_options, cups_option_t **, options);
};
*/

// Standard Postscript fonts
char *StandardPsFonts[] =
{
	"AvantGarde-Book",				// 0
	"AvantGarde-BookOblique",
	"AvantGarde-Demi",
	"AvantGarde-DemiOblique",
	"Bookman-Demi",
	"Bookman-DemiItalic",			// 5
	"Bookman-Light",
	"Bookman-LightItalic",
	"Courier-Bold",
	"Courier-BoldOblique",
	"Courier",						// 10
	"Courier-Oblique",
	"Helvetica-Bold",
	"Helvetica-BoldOblique",
	"Helvetica-NarrowBold",
	"Helvetica-NarrowBoldOblique",	// 15
	"Helvetica",
	"Helvetica-Oblique",
	"Helvetica-Narrow",
	"Helvetica-NarrowOblique",
	"NewCenturySchlbk-Bold",		// 20
	"NewCenturySchlbk-BoldItalic",
	"NewCenturySchlbk-Italic",
	"NewCenturySchlbk-Roman",
	"Palatino-Bold",
	"Palatino-BoldItalic",			// 25
	"Palatino-Italic",
	"Palatino-Roman",
	"Symbol",
	"Times-Bold",
	"Times-BoldItalic",				// 30
	"Times-Italic",
	"Times-Roman",
	"ZapfChancery-MediumItalic",
	"ZapfDingbats",					// 34
	0
};

#define PS_AVANTGARDE		0
#define PS_BOOKMAN			4
#define PS_COURIER			10
#define PS_HELVETICA		16
#define PS_NEWCENTURY		23
#define PS_PALATINO			27
#define PS_SYMBOL			28
#define PS_TIMES			32
#define PS_ZAPF				33

typedef struct
{
	char *NativeFont;
	int PsFont;
} PsFontMapping;

PsFontMapping PsFontMap[] = 
{
#if defined LINUX
	{"Sans", PS_HELVETICA},
	{"Luxi Sans", PS_HELVETICA},
#endif
	{"Verdana", PS_HELVETICA},
	{0, 0}
};

///////////////////////////////////////////////////////////////////////////////////////
class GPrintDCPrivate // : public GCups
{
public:
	class PrintPainter *p;
	void *Handle;
	char *PrintJobName;
	bool DocOpen;
	bool PageOpen;
	int Pages;
	COLOUR c;
	
	char *FileName;
	LFile Ps;
	
	GPrintDCPrivate()
	{
		p = 0;
		c = -1;
		Pages = 0;
		FileName = 0;
		Handle = 0;
		PrintJobName = 0;
		PageOpen = false;
		DocOpen = false;
	}
	
	~GPrintDCPrivate()
	{
		if (DocOpen)
		{
		}
		
		DeleteArray(FileName);
		DeleteArray(PrintJobName);
	}
	
	bool IsOk()
	{
		return	this != 0; // && IsLoaded();
	}
	
	bool StartPs()
	{
		char Temp[256];
		char Part[64];
		do
		{
			LgiGetSystemPath(LSP_TEMP, Temp, sizeof(Temp));
			sprintf(Part, "_lgi_printjob_%x.ps", LRand());
			LMakePath(Temp, sizeof(Temp), Temp, Part);
		}
		while (FileExists(Temp));
		
		FileName = NewStr(Temp);
		if (FileName)
		{
			if (Ps.Open(FileName, O_WRITE))
			{
				Ps.Print(	"%!PS-Adobe-3.0\n"
							"%%%%BoundingBox: left bottom right top\n"
							"%%%%Pages: (atend)\n"
							"%%%%EndComments\n"
							"\n"
							"/startpage {\n"
							"	newpath clippath pathbbox\n"
							"	/papery2 exch def\n"
							"	/paperx2 exch def\n"
							"	/papery1 exch def\n"
							"	/paperx1 exch def\n"
							"	paperx1 papery2 translate\n"
							"	1 setlinewidth\n"
							"	0.000000 0.000000 0.000000 setrgbcolor\n"
							"	/Helvetica findfont\n"
							"	8 scalefont\n"
							"	setfont\n"
							"} def\n"
							"\n");
											
				return true;
			}
		}			
		
		return false;
	}
	
	bool StartPage()
	{
		if (Ps.IsOpen())
		{
			Ps.Print(	"%%%%Page: %i %i\n"
						"gsave\n"
						"startpage\n",
						Pages + 1,
						Pages + 1);
			return true;
		}
		
		return false;
	}

	bool EndPage()
	{
		if (Ps.IsOpen())
		{
			Ps.Print(	"grestore\n"
						"showpage\n"
						"%%%%EndPage\n"
						"\n");
			Pages++;
			return true;
		}
		return false;
	}
	
	bool EndPs()
	{
		if (Ps.IsOpen())
		{
			Ps.Print(	"%%%%Trailer\n"
						"%%%%Pages: %i\n"
						"%%%%EOF\n",
						Pages);
			Ps.Close();
			return true;
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////////////
class PrintPainter
{
	LPrintDC *pDC;
	int Fore, Back;
	List<LRect> Clip;
	int Ox, Oy;

	char Face[256];
	int PtSize;
	int Ascent;

public:
	PrintPainter(LPrintDC *dc)
	{
		Face[0] = 0;
		pDC = dc;
		Fore = Back = 0;
		Ox = Oy = 0;
		PtSize = 0;
		Ascent = 0;
	}
	
	~PrintPainter()
	{
		EmptyClip();
	}

	// Don't need these
	// bool Begin(XWidget *w) { return pDC != 0; }
	void End() {}
	int X() { return pDC->X(); }
	int Y() { return pDC->Y(); }
	//void setRasterOp(RowOperation i) {}
	//RowOperation rasterOp() { return CopyROP; }
	void drawPoint(int x, int y) {}
	void drawLine(int x1, int y1, int x2, int y2) {}
	void drawRect(int x, int y, int wid, int height) {}
	// void drawImage(int x, int y, XBitmapImage &image, int sx, int sy, int sw, int sh, XBitmapImage::BlitOp op) {}
	
	LFile &File()
	{
		return pDC->d->Ps;
	}

	void GetScale(double &x, double &y)
	{
		x = PS_SCALE;
		y = PS_SCALE;
	}
	
	void setFore(int c)
	{
		Fore = c;
	}
	
	void setBack(int c)
	{
		Back = c;
	}
	
	void setFont(LFont *f)
	{
		if (File().IsOpen() &&
			(stricmp(f->Face(), Face) != 0 || PtSize != f->PointSize()))
		{
			strcpy(Face, f->Face());
			PtSize = f->PointSize();			
			Ascent = f->Ascent();

			char *FontFace = Face;
			for (PsFontMapping *m = PsFontMap; m->NativeFont; m++)
			{
				if (stricmp(m->NativeFont, FontFace) == 0)
				{
					FontFace = StandardPsFonts[m->PsFont];
					break;
				}
			}
			
			char c[256], *o = c;
			for (char *i=FontFace; *i; i++)
			{
				if (*i != ' ') *o++ = *i;
			}
			*o = 0;
			
			File().Print(	"/%s findfont\n"
							"%i scalefont\n"
							"setfont\n",
							c,
							f->PointSize());
		}
	}
	
	void translate(int x, int y)
	{
		Ox += x;
		Oy += y;
	}
	
	void PushClip(int x1, int y1, int x2, int y2)
	{
		LRect *Next = new LRect(Ox + x1, Oy + y1, Ox + x2, Ox + y2);
		if (Next)
		{
			LRect *Last = Clip.Last();
			if (Last)
			{
				Next->Bound(Last);
			}
			else
			{
				LRect r(0, 0, X()-1, Y()-1);
				Next->Bound(&r);
			}
			
			Clip.Insert(Last);
		}
	}
	
	void PopClip()
	{
		LRect *Last = Clip.Last();
		if (Last)
		{
			Clip.Delete(Last);
			DeleteObj(Last);
		}
	}
	
	void EmptyClip()
	{
		Clip.DeleteObjects();
	}
	
	void SetClient(LRect *Client)
	{
	}
	
	void drawText(int x, int y, char16 *text, int len, int *backColour, LRect *clip)
	{
		if (ValidStrW(text) && File().IsOpen())
		{
			if (len < 0)
			{
				len = StrlenW(text);
			}
			
			// Encode the string to protect the '(' and ')' characters which are reserved for the syntax
			// of the postscript language. So we escape them here.
			char *Mem = LgiNewUtf16To8(text);
			LStringPipe p;
			for (char *s = Mem; s && *s; )
			{
				char *e = s;
				while (*e && *e != '(' && *e != ')') e++;
				if (*e)
				{
					p.Push(s, (int)e-(int)s);
					
					char n[32];
					sprintf(n, "\\%c", *e++);
					p.Push(n);
					s = e;
				}
				else
				{
					p.Push(s);
					break;
				}
			}
			
			DeleteArray(Mem);
			Mem = p.NewStr();
			
			File().Print(	"newpath\n"
							"%f %f moveto\n"
							"(%s) show\n",
							pDC->Xc(x), pDC->Yc(y),
							Mem);

			DeleteArray(Mem);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////
class DeleterThread : public LThread
{
	char *File;
	
public:
	DeleterThread(char *f) : LThread("DeleterThread")
	{
		File = NewStr(f);
		DeleteOnExit = true;
		Run();
	}
	
	int Main()
	{
		/*
		LSleep(2 * 60 * 1000);
		FileDev->DeleteFile(File, false);
		DeleteArray(File);
		*/
		
		return 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
LPrintDC::LPrintDC(void *Handle, const char *PrintJobName)
{
	d = new GPrintDCPrivate;
	d->Handle = Handle;
	d->PrintJobName = NewStr(PrintJobName);
}

LPrintDC::~LPrintDC()
{
	EndPage();
	
	if (d->Pages)
	{
		char Copies[32] = "1";
		
		/*
		int Job = 0;
		cups_dest_t *Dests = 0;
		int DestCount = d->cupsGetDests(&Dests);
		if (Dests)
		{
			cups_dest_t *Dest = d->cupsGetDest(NULL, NULL, DestCount, Dests);
			if (Dest)
			{
				int Options = 0;
				cups_option_t *Option = 0;
				Options = d->cupsAddOption("copies", Copies, Options, &Option);				
				Job = d->cupsPrintFile(Dest->name, d->FileName, d->PrintJobName, Options, Option);
			}
		}
		*/

		d->EndPs();
		d->Ps.Close();
		
		/*
		if (!Job)
		{
			int Err = d->cupsLastError();
  			printf("Cups Error: %x\n", (int)Err);
  		}
  		else
  		{
			printf("Successful Job=%i\n", Job);
		}
		*/
		
		char a[256];
		sprintf(a, "-t \"%s\" %s", d->PrintJobName, d->FileName);
		LExecute("kprinter", a);
		new DeleterThread(d->FileName);
	}	
	
	DeleteObj(d);
}

double LPrintDC::Xc(int x)
{
	return (double) x / PS_SCALE;
}

double LPrintDC::Yc(int y)
{
	return (double) -y / PS_SCALE;
}

OsPainter LPrintDC::Handle()
{
	return Cairo;
}

void LPrintDC::Handle(OsPainter Set)
{
	Cairo = Set;
}

int LPrintDC::X()
{
	return (int) (8.26 * (double)DpiX());
}

int LPrintDC::Y()
{
	return (int) (10.27 * (double)DpiY()); // 11.69
}

// This is arbitary, chosen because it's easy to use.
int LPrintDC::GetBits()
{
	return 24;
}

// This resolution is chosen to match postscripts internal
// user space in pt's.
int LPrintDC::DpiX()
{
	return 72 * PS_SCALE;
}

int LPrintDC::DpiY()
{
	return 72 * PS_SCALE;
}

bool LPrintDC::StartPage()
{
	bool Status = false;
	
	if (d->IsOk())
	{
		if (!d->Ps.IsOpen())
		{
			d->StartPs();
		}
		
		Status = d->PageOpen = d->StartPage();
	}

	return Status;
}

void LPrintDC::EndPage()
{
	if (d->IsOk() &&
		d->PageOpen)
	{
		d->EndPage();
		d->PageOpen = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Print primitives
COLOUR LPrintDC::Colour()
{
	return d->c;
}

LColour LPrintDC::Colour(LColour c)
{
	LColour Prev(d->c, 24);
	COLOUR c24 = c.c24();
	if (c24 != d->c)
	{
		d->c = c24;
		if (d->Ps.IsOpen())
		{
			d->Ps.Print("%f %f %f setrgbcolor\n",
				(double) R24(d->c) / 255.0,
				(double) G24(d->c) / 255.0,
				(double) B24(d->c) / 255.0);
		}
	}
	
	return Prev;
}

COLOUR LPrintDC::Colour(COLOUR c, int Bits)
{
	COLOUR c24 = CBit(24, c, Bits?Bits:24);
	if (c24 != d->c)
	{
		d->c = c24;
		if (d->Ps.IsOpen())
		{
			d->Ps.Print("%f %f %f setrgbcolor\n",
				(double) R24(d->c) / 255.0,
				(double) G24(d->c) / 255.0,
				(double) B24(d->c) / 255.0);
		}
	}
	
	return c;
}

void LPrintDC::Set(int x, int y)
{
	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"stroke\n",
					Xc(x), Yc(y),
					Xc(x), Yc(y));
	}
}

COLOUR LPrintDC::Get(int x, int y)
{
    return -1;
}

void LPrintDC::HLine(int x1, int x2, int y)
{
	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"stroke\n",
					Xc(x1), Yc(y),
					Xc(x2), Yc(y));
	}
}

void LPrintDC::VLine(int x, int y1, int y2)
{
	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"stroke\n",
					Xc(x), Yc(y1),
					Xc(x), Yc(y2));
	}
}

void LPrintDC::Line(int x1, int y1, int x2, int y2)
{
	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"stroke\n",
					Xc(x1), Yc(y1),
					Xc(x2), Yc(y2));
	}
}

void LPrintDC::Circle(double cx, double cy, double radius)
{
}

void LPrintDC::FilledCircle(double cx, double cy, double radius)
{
}

void LPrintDC::Arc(double cx, double cy, double radius, double start, double end)
{
}

void LPrintDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
}

void LPrintDC::Ellipse(double cx, double cy, double x, double y)
{
}

void LPrintDC::FilledEllipse(double cx, double cy, double x, double y)
{
}

void LPrintDC::Box(int x1, int y1, int x2, int y2)
{
	LRect a(x1, y1, x2, y2);
	Box(&a);
}

void LPrintDC::Box(LRect *a)
{
	LRect r;
	if (a) r = *a;
	else r.ZOff(X()-1, Y()-1);

	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"%f %f lineto\n"
					"%f %f lineto\n"
					"closepath\n"
					"stroke\n",
					Xc(r.x1), Yc(r.y1),
					Xc(r.x2), Yc(r.y1),
					Xc(r.x2), Yc(r.y2),
					Xc(r.x1), Yc(r.y2));
	}
}

void LPrintDC::Rectangle(int x1, int y1, int x2, int y2)
{
	LRect a(x1, y1, x2, y2);
	Rectangle(&a);
}

void LPrintDC::Rectangle(LRect *a)
{
	LRect r;
	if (a) r = *a;
	else r.ZOff(X()-1, Y()-1);
	
	if (d->Ps.IsOpen())
	{
		d->Ps.Print("newpath\n"
					"%f %f moveto\n"
					"%f %f lineto\n"
					"%f %f lineto\n"
					"%f %f lineto\n"
					"closepath\n"
					"fill\n",
					Xc(r.x1), Yc(r.y1),
					Xc(r.x2), Yc(r.y1),
					Xc(r.x2), Yc(r.y2),
					Xc(r.x1), Yc(r.y2));
	}
}

void LPrintDC::Blt(int x, int y, LSurface *Src, LRect *a)
{
}

void LPrintDC::StretchBlt(LRect *d, LSurface *Src, LRect *s)
{
}

void LPrintDC::Polygon(int Points, LPoint *Data)
{
}

void LPrintDC::Bezier(int Threshold, LPoint *Pt)
{
}

void LPrintDC::FloodFill(int x, int y, int Mode, COLOUR Border, LRect *Bounds)
{
}

