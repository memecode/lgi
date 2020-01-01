/*

Useful info:
	https://github.com/alexey-lysiuk/ncexp/blob/966bce79e7385754883201a613432b4189e1d918/NimbleCommander/States/FilePanels/DragSender.mm
	@interface FilesDraggingSource : NSObject<NSDraggingSource, NSPasteboardItemDataProvider>
	@interface PanelDraggingItem : NSPasteboardItem


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

@interface LDragItem : NSPasteboardItem
@property (nonatomic, readonly) GString path;
@property (nonatomic, weak) NSImage *icon;
@property (nonatomic) GAutoPtr<GStreamI> src;
- (LDragItem*) initWithItem:(GString)item source:(GStreamI*)src;
- (void)dealloc;
@end

@implementation LDragItem
{
}

- (LDragItem*) initWithItem:(GString)item source:(GStreamI*)src
{
	if ((self = [super init]) != nil)
	{
		self->_path = item;
		self->_src.Reset(src);
		self->_icon = NULL;
		
		// for File URL Promise. need to check if this is necessary
        [self setString:(NSString*)kUTTypeData
                forType:(NSString *)kPasteboardTypeFilePromiseContent];
	}
	
	return self;
}

- (void)dealloc
{
	self->_src.Reset();
	[super dealloc];
}

@end

@interface LDragSource : NSObject<NSDraggingSource, NSPasteboardItemDataProvider>
{
	GArray<LDragItem*> Items;
}

@property GDndSourcePriv *d;

- (id)init:(GDndSourcePriv*)view;
- (void)addItem:(LDragItem*)i;

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type;
- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard;

@end

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	GSurface *ExternImg;
	GRect ExternSubRgn;
	int Effect;
	GMemDC Icon;
	List<char> Formats;

	GDndSourcePriv()
	{
		Effect = 0;
		ExternImg = NULL;
		ExternSubRgn.ZOff(-1, -1);
	}

	~GDndSourcePriv()
	{
		Formats.DeleteArrays();
	}
};

static NSURL *ExtractPromiseDropLocation(NSPasteboard *_pasteboard)
{
    NSURL *result = nil;
    PasteboardRef pboardRef = nullptr;
    PasteboardCreate((__bridge CFStringRef)_pasteboard.name, &pboardRef);
    if( pboardRef ) {
        PasteboardSynchronize(pboardRef);
        CFURLRef urlRef = nullptr;
        PasteboardCopyPasteLocation(pboardRef, &urlRef);
        if( urlRef )
            result = (NSURL*) CFBridgingRelease(urlRef);
        CFRelease(pboardRef);
    }
    return result;
}

@implementation LDragSource

- (id)init:(GDndSourcePriv*)d
{
	if ((self = [super init]) != nil)
	{
		self.d = d;
	}
	
	return self;
}

- (void)addItem:(LDragItem*)i
{
	self->Items.Add(i);
}

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session
		sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    switch (context)
    {
        case NSDraggingContextOutsideApplication:
			return NSDragOperationCopy;
			
        case NSDraggingContextWithinApplication:
			return NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationMove;
			
        default:
            return NSDragOperationNone;
    }

	/*
	NSDragOperation op = 0;
	auto Effect = self.d->Effect;

	if (Effect & DROPEFFECT_COPY)
		op |= NSDragOperationCopy;
	if (Effect & DROPEFFECT_MOVE)
		op |= NSDragOperationMove;
	if (Effect & DROPEFFECT_LINK)
		op |= NSDragOperationLink;

	return NSDragOperationNone;
	*/
}

- (void)pasteboard:(nullable NSPasteboard *)sender item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type
{
    if (![type isEqualToString:(NSString*)kPasteboardTypeFileURLPromise])
    	return;

	if (auto drop_url = ExtractPromiseDropLocation(sender))
	{
		GFile::Path path(drop_url.path.fileSystemRepresentation);
		auto di = objc_dynamic_cast(LDragItem, item);
		if (!di)
		{
			LgiTrace("%s:%i - not a LDragItem object.\n", _FL);
			return;
		}
		
		path += di.path;
		auto s = di.src;

		GFile out;
		if (!out.Open(path, O_WRITE))
		{
			LgiTrace("%s:%i - can't open '%s' for writing.\n", _FL, path.GetFull().Get());
			return;
		}
		out.SetSize(0);
		auto len = s->GetSize();
		int64 written = 0;
		for (size_t i=0; i<len; )
		{
			char buf[1024];
			auto rd = s->Read(buf, sizeof(buf));
			if (rd <= 0) break;
			auto wr = out.Write(buf, rd);
			if (wr < rd) break;
			written += wr;
		}
		
		if (written < len)
		{
			LgiTrace("%s:%i - write failed.\n", _FL);
			return;
		}
		else
		{
			const auto url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.GetFull()]
										isDirectory:false
									  	relativeToURL:nil];
			if (url)
			{
				// NB! keep this in dumb form!
				// [url writeToPasteboard:sender] doesn't work.
				[sender writeObjects:@[url]];
			}
		}
	}
}

- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard
{
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

static NSArray* BuildImageComponentsForItem(LDragItem* _item)
{
	GMemDC Mem(32, 32, System32BitColourSpace);
	Mem.Colour(0, 32);
	Mem.Rectangle();
	
	for (int i=0; i<3; i++)
	{
		GRect r(0, 0, 11, 15);
		r.Offset(10 + (i*3), 8 + (i*3));
		Mem.Colour(L_BLACK);
		Mem.Box(&r);
		r.Size(1, 1);
		Mem.Colour(L_WHITE);
		Mem.Rectangle(&r);
	}

	NSDraggingImageComponent *ic = [[NSDraggingImageComponent alloc] initWithKey:NSDraggingImageComponentIconKey];
	ic.contents = Mem.NsImage();
	ic.frame = NSMakeRect(0, 0, 32, 32);
	return @[ic];
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
	auto DragSrc = [[LDragSource alloc] init:d];
	
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
	NSMutableArray *drag_items = [[NSMutableArray alloc] init];

	auto pasteboard_types = @[(NSString *)kPasteboardTypeFileURLPromise];
	auto position = [h.p.contentView convertPoint:Event.p.locationInWindow fromView:nil];
    /*position.x -= 16;
    position.y -= 16;*/

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
					auto item = [[LDragItem alloc] initWithItem:File source:Stream];
					v.Value.Stream.Ptr = NULL; // So we take ownership of it.
					[item setDataProvider:DragSrc forTypes:pasteboard_types];
			
					[DragSrc addItem:item];
			
					auto drag_item = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
					drag_item.draggingFrame = NSMakeRect(floor(position.x), floor(position.y), 32, 32);

					[drag_items addObject:drag_item];
			
					#if 1
					__weak LDragItem *weak_pb_item = item;
					drag_item.imageComponentsProvider = ^{
						return BuildImageComponentsForItem((LDragItem *)weak_pb_item);
					};
					#endif
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
	
	if (drag_items.count > 0)
	{
		NSDraggingSession *session = [h.p.contentView	beginDraggingSessionWithItems:drag_items
														event:Event.p
														source:DragSrc];
		if (session)
		{
			//[d->Wrapper writeURLsPBoard:session.draggingPasteboard];
			return DROPEFFECT_COPY;
		}
	}
	else
	{
		[h.p dragImage:img at:pt offset:NSZeroSize event:Event.p pasteboard:pboard source:DragSrc slideBack:YES ];
	}

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
