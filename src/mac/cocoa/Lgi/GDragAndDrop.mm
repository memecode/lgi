/*
**	FILE:			GDragAndDrop.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/98
**	DESCRIPTION:	Drag and drop support
**
**	Copyright (C) 1998-2003, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GDisplayString.h"
#include "INet.h"
#include "GViewPriv.h"
#include "GClipBoard.h"

// #define DND_DEBUG_TRACE
class GDndSourcePriv;

const char *LMimeToUti(const char *Mime)
{
	if (!Mime) return NULL;
	
	#define _(m, u) if (!Stricmp(Mime, m)) return u;
	_("message/rfc822", "public.email-message")
	_("text/vcard", "public.contact")
	_("text/vcalendar", "public.calendar")
	_("text/html", "public.html")
	_("text/xml", "public.xml")
	
	LgiAssert(!"Impl me");
	return "public.item";
}

@interface LDragSource : NSObject<NSDraggingSource>
{
}
@property GDndSourcePriv *d;
- (id)init:(GDndSourcePriv*)view;
- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
@end

@interface LFilePromiseProviderDelegate : NSObject<NSFilePromiseProviderDelegate>
{
	GString filename;
	GStreamI *stream;
}
- (id)init:(GString)fn stream:(GStreamI*)stream;
- (NSString *)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider fileNameForType:(NSString *)fileType;
- (void)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider writePromiseToURL:(NSURL *)url completionHandler:(void (^)(NSError * __nullable errorOrNil))completionHandler;
@end

@implementation LFilePromiseProviderDelegate
- (id)init:(GString)fn stream:(GStreamI*)s
{
	if ((self = [super init]) != nil)
	{
		self->filename = fn;
		self->stream = s;
	}
	
	return self;
}

- (NSString *)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider fileNameForType:(NSString *)fileType
{
	return self->filename.NsStr();
}

- (void)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider
		writePromiseToURL:(NSURL *)url
		completionHandler:(void (^)(NSError * __nullable errorOrNil))completionHandler
{
	GFile out;
	if (!out.Open([url.path UTF8String], O_WRITE))
		return;
	out.SetSize(0);
	auto len = self->stream->GetSize();
	int64 written = 0;
	for (size_t i=0; i<len; )
	{
		char buf[1024];
		auto rd = self->stream->Read(buf, sizeof(buf));
		if (rd <= 0) break;
		auto wr = out.Write(buf, rd);
		if (wr < rd) break;
		written += wr;
	}
	
	if (written < len)
		LgiTrace("%s:%i - write failed.\n", _FL);
}
@end

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	GSurface *ExternImg;
	GRect ExternSubRgn;
	LDragSource *Wrapper;
	int Effect;
	GMemDC Icon;
	List<char> Formats;

	GDndSourcePriv()
	{
		Effect = 0;
		ExternImg = NULL;
		Wrapper = NULL;
		ExternSubRgn.ZOff(-1, -1);
	}

	~GDndSourcePriv()
	{
		Formats.DeleteArrays();
		if (Wrapper)
			[Wrapper release];
	}
};

@implementation LDragSource

- (id)init:(GDndSourcePriv*)d
{
	if ((self = [super init]) != nil)
	{
		self.d = d;
	}
	
	return self;
}

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session
		sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
	NSDragOperation op = 0;
	auto Effect = self.d->Effect;

	if (Effect & DROPEFFECT_COPY)
		op |= NSDragOperationCopy;
	if (Effect & DROPEFFECT_MOVE)
		op |= NSDragOperationMove;
	if (Effect & DROPEFFECT_LINK)
		op |= NSDragOperationLink;

	return NSDragOperationNone;
}

@end


/////////////////////////////////////////////////////////////////////////////////////////
GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	DeleteObj(d);
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	d->ExternImg = Img;
	if (SubRgn)
		d->ExternSubRgn = *SubRgn;
	else
		d->ExternSubRgn.ZOff(-1, -1);

	return true;
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, GString::Array &Files)
{
	if (OutputData && Files.First())
	{
		for (auto f : Files)
			OutputData->Data.New().OwnStr(NewStr(f));

		OutputData->Format = LGI_FileDropFormat;
		return true;
	}

	return false;
}

int GDragDropSource::Drag(GView *SourceWnd, OsEvent Event, int Effect, GSurface *Icon)
{
	LgiAssert(SourceWnd);
	if (!SourceWnd || !Event)
	{
		LgiAssert(!"Missing param");
		return DROPEFFECT_NONE;
	}

	if (!GetFormats(d->Formats))
	{
		LgiAssert(!"No formats");
		return DROPEFFECT_NONE;
	}

	auto Wnd = SourceWnd->GetWindow();
	if (!Wnd) return DROPEFFECT_NONE;
	auto h = Wnd->WindowHandle();
	if (!h) return DROPEFFECT_NONE;

	NSImage *img = nil;
	GMemDC *Mem = dynamic_cast<GMemDC*>(Icon);
	if (Mem)
	{
		img = Mem->NsImage();
	}
	else
	{
		// Synthesis an image..
		if (!d->Icon.X())
			d->Icon.Create(32, 32, System32BitColourSpace);
		Mem = &d->Icon;
		Mem->Colour(0, 32);
		Mem->Rectangle();
		
		for (int i=0; i<3; i++)
		{
			GRect r(0, 0, 11, 15);
			r.Offset(10 + (i*3), 8 + (i*3));
			Mem->Colour(L_BLACK);
			Mem->Box(&r);
			r.Size(1, 1);
			Mem->Colour(L_WHITE);
			Mem->Rectangle(&r);
		}
		
		img = Mem->NsImage();
	}
	
	d->Effect = Effect;
	if (!d->Wrapper)
		d->Wrapper = [[LDragSource alloc] init:d];
	
	auto pt = Event.p.locationInWindow;
	pt.y -= Mem->Y();
	
	NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];

	List<char> Formats;
	if (!GetFormats(Formats))
		return DROPEFFECT_NONE;

	GArray<GDragData> Data;
	for (auto f: Formats)
		Data.New().Format = f;
	Formats.DeleteArrays();

	if (!GetData(Data))
		return DROPEFFECT_NONE;
	
	[pboard clearContents];
	for (auto &dd: Data)
	{
		if (dd.IsFileStream())
		{
			for (int i=0; i<dd.Data.Length()-2; i+=3)
			{
				auto File = dd.Data[i].Str();
				auto MimeType = dd.Data[i+1].Str();
				auto &v = dd.Data[i+2];
				auto Stream = v.Type == GV_STREAM ? v.Value.Stream.Ptr : NULL;
				
				if (File && MimeType && Stream)
				{
					GString Uti = LMimeToUti(MimeType);
					auto NsUti = Uti.NsStr();
					NSString *utType = (__bridge_transfer NSString*)UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (__bridge CFStringRef)NsUti, kUTTypeData);
					if (utType)
					{
						auto delegate = [[LFilePromiseProviderDelegate alloc] init:File stream:Stream];
						auto prov = [[NSFilePromiseProvider alloc] initWithFileType:utType delegate:delegate];
						NSArray *array = [NSArray arrayWithObject:prov];
						auto wr = [pboard writeObjects:array];
						if (wr)
							printf("Adding file promise '%s' (%s) to drag...\n", File, Uti.Get());
						else
							printf("Error: Adding file promise '%s' (%s) to drag...\n", File, Uti.Get());
						[utType release];
					}
				}
			}
		}
		else if (dd.Data.Length() == 1)
		{
			GVariant &v = dd.Data[0];
			switch (v.Type)
			{
				case GV_STRING:
				{
					NSData *s = [[NSData alloc] initWithBytes:v.Value.String length:strlen(v.Value.String)];
					auto r = [pboard setData:s forType:dd.Format.NsStr()];
					if (!r)
						printf("%s:%i - setData failed.\n", _FL);
					else
						printf("Adding string '%s' to drag...\n", dd.Format.Get());
					break;
				}
				case GV_BINARY:
				{
					// Lgi specific type for moving binary data around...
					auto data = [[LBinaryData alloc] init:dd.Format ptr:(uchar*)v.Value.Binary.Data len:v.Value.Binary.Length];
					NSArray *array = [NSArray arrayWithObject:data];
					[pboard writeObjects:array];
					
					printf("Adding binary '%s' to drag...\n", dd.Format.Get());
					break;
				}
				default:
				{
					printf("%s:%i - Unsupported type '%s' for format '%s'.\n", _FL, GVariant::TypeToString(v.Type), dd.Format.Get());
					break;
				}
			}
		}
		else printf("%s:%i - Impl multiple data handling for %s.\n", _FL, dd.Format.Get());
	}
	
	[h.p dragImage:img at:pt offset:NSZeroSize event:Event.p pasteboard:pboard source:d->Wrapper slideBack:YES ];

	return DROPEFFECT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	To = 0;
}

GDragDropTarget::~GDragDropTarget()
{
	Formats.DeleteArrays();
}

void GDragDropTarget::SetWindow(GView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		To->DropTarget(this);
		Status = To->DropTarget(true);
		if (To->WindowHandle())
		{
			OnDragInit(Status);
		}
		else
		{
			LgiTrace("%s:%i - Error\n", _FL);
		}
	}
}
