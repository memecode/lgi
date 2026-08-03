/*

Useful info:
	https://github.com/alexey-lysiuk/ncexp/blob/966bce79e7385754883201a613432b4189e1d918/NimbleCommander/States/FilePanels/DragSender.mm
	@interface FilesDraggingSource : NSObject<NSDraggingSource, NSPasteboardItemDataProvider>
	@interface PanelDraggingItem : NSPasteboardItem


*/

#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DragAndDrop.h"
#include "lgi/common/DisplayString.h"
#include "lgi/common/Net.h"
#include "lgi/common/ClipBoard.h"
#include "lgi/common/ProgressDlg.h"

#include "ViewPriv.h"

// #define DND_DEBUG_TRACE
#define DND_DEBUG_TRACE

#ifdef DND_DEBUG_TRACE
    #define DND_DLOG(...) LgiTrace(__VA_ARGS__)
    static void DndLogPasteboardTypes(NSArray<NSPasteboardType> *types, const char *tag)
    {
        if (!types)
        {
            LgiTrace("%s: (null types)\n", tag ? tag : "DND");
            return;
        }

        LgiTrace("%s: %ld type(s)\n", tag ? tag : "DND", (long)types.count);
        for (NSPasteboardType t in types)
        {
            const char *s = t.UTF8String ? t.UTF8String : "<null>";
            LgiTrace("  - %s\n", s);
        }
    }
#else
    #define DND_DLOG(...)
    static void DndLogPasteboardTypes(NSArray<NSPasteboardType> *, const char *) {}
#endif

const char *LMimeToUti(const char *Mime)
{
    if (!Mime || !*Mime)
        return "public.data";

    // Trim MIME params: "type/subtype; charset=utf-8" -> "type/subtype"
    LString m(Mime);
    auto semi = strchr(m.Get(), ';');
    if (semi)
        *semi = 0;
    m = m.Strip();

    #define _(mm, u) if (!Stricmp(m.Get(), mm)) return u;
    _("message/rfc822", "public.email-message")
    _("text/vcard", "public.contact")
    _("text/vcalendar", "public.calendar")
    _("text/html", "public.html")
    _("text/xml", "public.xml")
    #undef _

    return "public.data";
}

static LString PromiseSafeName(const LString &inName, const LString &uti)
{
    // basename only
    LString name = inName;
    const char *s = name.Get();
    const char *p1 = strrchr(s, '/');
    const char *p2 = strrchr(s, '\\');
    const char *base = p1 > p2 ? p1 : p2;
    if (base && base[0])
        name = base + 1;

    // replace problematic characters
    for (char *p = name.Get(); p && *p; ++p)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
            *p = '_';
    }

    // ensure non-empty
    if (name.IsEmpty())
        name = "Untitled";

    // add extension for known UTIs if missing
    if (!strchr(name.Get(), '.'))
    {
        if (!Stricmp(uti.Get(), "public.email-message")) name += ".eml";
        else if (!Stricmp(uti.Get(), "public.contact"))  name += ".vcf";
        else if (!Stricmp(uti.Get(), "public.calendar")) name += ".ics";
        else if (!Stricmp(uti.Get(), "public.html"))     name += ".html";
        else if (!Stricmp(uti.Get(), "public.xml"))      name += ".xml";
    }

    return name;
}

static bool CopyStreamToFileSync(const LString &dst, LStreamI *src)
{
    if (!src)
        return false;

    LFile out;
    if (!out.Open(dst, O_WRITE))
    {
        LgiTrace("%s:%i - can't open '%s' for writing.\n", _FL, dst.Get());
        return false;
    }

    out.SetSize(0);

    for (;;)
    {
        char buf[64 * 1024];
        auto rd = src->Read(buf, sizeof(buf));
        if (rd <= 0)
            break;

        auto wr = out.Write(buf, rd);
        if (wr < rd)
        {
            LgiTrace("%s:%i - short write to '%s'.\n", _FL, dst.Get());
            return false;
        }
    }

    return true;
}

static NSString *PromiseExtForUti(const LString &uti)
{
    if (!Stricmp(uti.Get(), "public.email-message")) return @"eml";
    if (!Stricmp(uti.Get(), "public.contact"))       return @"vcf";
    if (!Stricmp(uti.Get(), "public.calendar"))      return @"ics";
    if (!Stricmp(uti.Get(), "public.html"))          return @"html";
    if (!Stricmp(uti.Get(), "public.xml"))           return @"xml";
    return @"dat";
}

class LDndSourcePriv
{
public:
	LAutoString CurrentFormat;
	LSurface *ExternImg;
	LRect ExternSubRgn;
	int Effect;
	LMemDC Icon;
	LDragFormats Formats;

	LDndSourcePriv() :
		Formats(true),
		Icon(_FL)
	{
		Effect = 0;
		ExternImg = NULL;
		ExternSubRgn.ZOff(-1, -1);
	}
};


@interface LDragItem : NSPasteboardItem
@property (nonatomic, readonly) LString path;
@property (nonatomic, readonly) LString uti;
@property (nonatomic, weak) NSImage *icon;
@property (nonatomic) LAutoPtr<LStreamI> src;
- (LDragItem*) initWithItem:(LString)item mime:(const char*)mime source:(LStreamI*)src;
- (void)dealloc;
@end

@implementation LDragItem
{
}

- (LDragItem*) initWithItem:(LString)item mime:(const char*)mime source:(LStreamI*)src
{
    if ((self = [super init]) != nil)
    {
        self->_src.Reset(src);
        self->_icon = NULL;

        const char *mapped = LMimeToUti(mime);
        if (!mapped) mapped = "public.data";
        self->_uti = mapped;
        self->_path = PromiseSafeName(item, self->_uti);

        NSString *utiStr = [NSString stringWithUTF8String:self->_uti.Get()];
        if (utiStr)
            [self setString:utiStr forType:(NSString *)kPasteboardTypeFilePromiseContent];

        // Legacy Finder/Desktop promise hint
        NSString *ext = PromiseExtForUti(self->_uti);
        if (ext)
            [self setPropertyList:@[ext] forType:(NSString *)NSFilesPromisePboardType];
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
	LArray<LDragItem*> Items;
	LView *SourceWnd;
}

@property LDndSourcePriv *d;

- (id)init:(LDndSourcePriv*)view wnd:(LView*)Wnd;
- (void)addItem:(LDragItem*)i;

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type;
- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard;
- (NSArray<NSString*> *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination;
@end

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

class LFileCopy : public LProgressDlg, public LThread
{
	LString Dst;
	LAutoPtr<LStreamI> Src;
	uint64_t StartTs;
	
public:
	LFileCopy(LView *parent, LString dst, LAutoPtr<LStreamI> src) :
		LThread("LFileCopy"), LProgressDlg(parent, 1000)
	{
		SetParent(parent);
		Dst = dst;
		Src = src;
		StartTs = LCurrentTime();
		
		SetDescription("Saving file...");
		SetPulse(400);
		Run();
	}
	
	void OnPulse()
	{
		LProgressDlg::OnPulse();
		if (IsExited())
		{
			SetPulse();
			EndModeless();
			delete this;
		}
		else if (StartTs)
		{
			uint64_t Diff = LCurrentTime() - StartTs;
			if (Diff > 1000)
			{
				StartTs = 0;
				DoModeless();
			}
		}
	}
	
	int Main()
	{
		LFile out;
		if (!out.Open(Dst, O_WRITE))
		{
			LgiTrace("%s:%i - can't open '%s' for writing.\n", _FL, Dst.Get());
			return -1;
		}
		
		out.SetSize(0);
		auto len = Src->GetSize();
		int64 written = 0;
		if (len > 0)
			SetRange(LRange(0, len));
		SetScale(1.0/1024.0/1024.0);
		SetType("MiB");

		for (size_t i=0; len<0 || i<len; )
		{
			char buf[1024];
			auto rd = Src->Read(buf, sizeof(buf));
			if (rd <= 0) break;
			auto wr = out.Write(buf, rd);
			if (wr < rd) break;
			written += wr;
			
			Value(written);
			if (IsCancelled())
				break;
		}
		
		return 0;
	}
};

@implementation LDragSource

- (id)init:(LDndSourcePriv*)d wnd:(LView*)Wnd
{
	if ((self = [super init]) != nil)
	{
		self.d = d;
		self->SourceWnd = Wnd;
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
    DND_DLOG("%s:%i provideDataForType requested='%s'\n", _FL, type.UTF8String ? type.UTF8String : "<null>");
    if (sender)
        DndLogPasteboardTypes(sender.types, "sender.types");

    if (![type isEqualToString:(NSString*)kPasteboardTypeFileURLPromise])
        return;

    auto di = objc_dynamic_cast(LDragItem, item);
    if (!di)
    {
        LgiTrace("%s:%i - not a LDragItem object.\n", _FL);
        return;
    }

    NSURL *drop_url = ExtractPromiseDropLocation(sender);
    DND_DLOG("%s:%i drop_url(initial)='%s'\n", _FL,
             drop_url ? (drop_url.path.UTF8String ? drop_url.path.UTF8String : "<null>") : "<nil>");

    // Fallback when Finder doesn't expose paste location.
    if (!drop_url)
    {
        NSString *tmp = NSTemporaryDirectory();
        if (tmp.length)
            drop_url = [NSURL fileURLWithPath:tmp isDirectory:YES];
        DND_DLOG("%s:%i drop_url(fallback tmp)='%s'\n", _FL,
                 drop_url ? (drop_url.path.UTF8String ? drop_url.path.UTF8String : "<null>") : "<nil>");
    }

    if (!drop_url)
        return;

    LFile::Path path(drop_url.path.fileSystemRepresentation);
    path += di.path;

    DND_DLOG("%s:%i writing promised file to '%s'\n", _FL, path.GetFull().Get());

    if (!CopyStreamToFileSync(path.GetFull(), di.src))
    {
        DND_DLOG("%s:%i CopyStreamToFileSync failed '%s'\n", _FL, path.GetFull().Get());
        return;
    }

    const auto url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.GetFull().Get()]
                                 isDirectory:false
                               relativeToURL:nil];
    if (url)
    {
        DND_DLOG("%s:%i publishing promised URL '%s'\n", _FL, url.path.UTF8String ? url.path.UTF8String : "<null>");
        [sender writeObjects:@[url]];
    }
}

- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard
{
}

@end

/////////////////////////////////////////////////////////////////////////////////////////
LDragDropSource::LDragDropSource()
{
	d = new LDndSourcePriv;
	OnRegister(true);
}

LDragDropSource::~LDragDropSource()
{
	DeleteObj(d);
}

bool LDragDropSource::SetIcon(LSurface *Img, LRect *SubRgn)
{
	d->ExternImg = Img;
	if (SubRgn)
		d->ExternSubRgn = *SubRgn;
	else
		d->ExternSubRgn.ZOff(-1, -1);

	return true;
}

bool LDragDropSource::CreateFileDrop(LDragData *OutputData, LMouse &m, LString::Array &Files)
{
	if (OutputData && Files.First())
	{
		for (auto f : Files)
		{
			LString s;
			s.Printf("file://%s", f.Get());
			OutputData->Data.New().OwnStr(NewStr(s));
		}

		OutputData->Format = LGI_FileDropFormat;
		return true;
	}

	return false;
}

static NSArray* BuildImageComponentsForItem(NSPasteboardItem *_item)
{
	NSDraggingImageComponent *ic = [[NSDraggingImageComponent alloc] initWithKey:NSDraggingImageComponentIconKey];
	NSImage *img = nil;

	#if 0
		img = [NSImage imageNamed:NSImageNameApplicationIcon]; // test it works..
	#else
		LMemDC Mem(_FL, 32, 32, System32BitColourSpace);
		Mem.Colour(0, 32);
		Mem.Rectangle();
	
		for (int i=0; i<3; i++)
		{
			LRect r(0, 0, 11, 15);
			r.Offset(10 + (i*3), 8 + (i*3));
			Mem.Colour(L_BLACK);
			Mem.Box(&r);
			r.Inset(1, 1);
			Mem.Colour(L_WHITE);
			Mem.Rectangle(&r);
		}

		img = Mem.NsImage();
	#endif

	if (img)
	{
		ic.contents = img;
		ic.frame = NSMakeRect(0, 0, img.size.width, img.size.height);
	}

	return @[ic];
}

int LDragDropSource::Drag(LView *SourceWnd, OsEvent Event, int Effect, LSurface *Icon)
{
	LAssert(SourceWnd);
	if (!SourceWnd || !Event)
	{
		LAssert(!"Missing param");
		return DROPEFFECT_NONE;
	}

	d->Formats.SetSource(true);
	if (!GetFormats(d->Formats))
	{
		LAssert(!"No formats");
		return DROPEFFECT_NONE;
	}

	auto Wnd = SourceWnd->GetWindow();
	if (!Wnd)
		return DROPEFFECT_NONE;
	auto h = Wnd->WindowHandle();
	if (!h)
		return DROPEFFECT_NONE;

	NSImage *img = nil;
	auto Mem = dynamic_cast<LMemDC*>(Icon);
	if (Mem)
	{
		img = Mem->NsImage();
	}
	else
	{
		// synthesize an image..
		if (!d->Icon.X())
			d->Icon.Create(32, 32, System32BitColourSpace);
		Mem = &d->Icon;
		Mem->Colour(0, 32);
		Mem->Rectangle();
		
		for (int i=0; i<3; i++)
		{
			LRect r(0, 0, 11, 15);
			r.Offset(10 + (i*3), 8 + (i*3));
			Mem->Colour(L_BLACK);
			Mem->Box(&r);
			r.Inset(1, 1);
			Mem->Colour(L_WHITE);
			Mem->Rectangle(&r);
		}
		
		img = Mem->NsImage();
	}
	
	d->Effect = Effect;
	auto DragSrc = [[LDragSource alloc] init:d wnd:SourceWnd];
	
	auto pt = Event.p.locationInWindow;
	pt.y -= Mem->Y();

	LDragFormats Formats(true);
	if (!GetFormats(Formats))
		return DROPEFFECT_NONE;

	LArray<LDragData> Data;
	for (auto f: Formats.Formats)
		Data.New().Format = f;
	Formats.Empty();

	if (!GetData(Data))
		return DROPEFFECT_NONE;
	
	auto drag_items = [[NSMutableArray alloc] init];

	auto pasteboard_types = @[
        (NSString *)kPasteboardTypeFileURLPromise,
        (NSString *)NSFilesPromisePboardType
    ];
    DndLogPasteboardTypes(pasteboard_types, "provider types");

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
					DND_DLOG("%s:%i queue file-stream drag file='%s' mime='%s'\n",
							 _FL, File, MimeType);
					auto item = [[LDragItem alloc] initWithItem:File mime:MimeType source:Stream];
					v.Value.Stream.Ptr = NULL; // So we take ownership of it.
					[item setDataProvider:DragSrc forTypes:pasteboard_types];
			
					[DragSrc addItem:item];
			
					auto drag_item = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
					drag_item.draggingFrame = NSMakeRect(floor(pt.x), floor(pt.y), 32, 32);

					[drag_items addObject:drag_item];
			
					#if 0
						__weak LDragItem *weak_pb_item = item;
						drag_item.imageComponentsProvider = ^{
							return BuildImageComponentsForItem(weak_pb_item);
						};
					#endif
				}
			}
		}
		else if (dd.Data.Length() == 1)
		{
			LVariant &v = dd.Data[0];
			switch (v.Type)
			{
				case GV_STRING:
				{
					auto item = [[NSPasteboardItem alloc] init];

					LString str = v.Str();
					[item setString:str.NsStr() forType:dd.Format.NsStr()];

					auto drag_item = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
					drag_item.draggingFrame = NSMakeRect(floor(pt.x), floor(pt.y), 32, 32);
					[drag_items addObject:drag_item];

					printf("Adding string '%s' to drag...\n", dd.Format.Get());

					#if 0
						__weak NSPasteboardItem *weak_pb_item = item;
						drag_item.imageComponentsProvider = ^{
							return BuildImageComponentsForItem(weak_pb_item);
						};
					#endif
					break;
				}
				case GV_BINARY:
				{
					auto item = [[NSPasteboardItem alloc] init];
					
					NSData *data = [NSData dataWithBytes:v.Value.Binary.Data length:v.Value.Binary.Length];
					[item setData:data forType:dd.Format.NsStr()];

					auto drag_item = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
					drag_item.draggingFrame = NSMakeRect(floor(pt.x), floor(pt.y), 32, 32);
					[drag_items addObject:drag_item];

					DND_LOG("Adding binary '%s' to drag...\n", dd.Format.Get());
					
					#if 0
						__weak NSPasteboardItem *weak_pb_item = item;
						drag_item.imageComponentsProvider = ^{
							return BuildImageComponentsForItem(weak_pb_item);
						};
					#endif
					break;
				}
				default:
				{
					printf("%s:%i - Unsupported type '%s' for format '%s'.\n", _FL, LVariant::TypeToString(v.Type), dd.Format.Get());
					break;
				}
			}
		}
		else printf("%s:%i - Impl multiple data handling for %s.\n", _FL, dd.Format.Get());
	}
	
	auto session = [h.p.contentView	beginDraggingSessionWithItems:drag_items
									event:Event.p
									source:DragSrc];
	if (session)
	{
		//[d->Wrapper writeURLsPBoard:session.draggingPasteboard];
		DND_LOG("%s:%i beginDraggingSessionWithItems started session\n", _FL);
		return DROPEFFECT_COPY;
	}
	else
	{
		DND_ERROR("%s:%i error: beginDraggingSessionWithItems failed.\n", _FL);
	}

	return DROPEFFECT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////
LDragDropTarget::LDragDropTarget() : Formats(false)
{
	To = 0;
}

LDragDropTarget::~LDragDropTarget()
{
}

void LDragDropTarget::SetWindow(LView *to)
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

////////////////////////////////////////////////////////////////////////////////////////////////

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
bool LDragFormats::CheckUti(const char *uti)
{
	auto nsUri = LString(uti).NsStr();
	// auto t = [UTType typeWithIdentifier:nsUri];

	// [t release];
	[nsUri release];

	return false;
}

