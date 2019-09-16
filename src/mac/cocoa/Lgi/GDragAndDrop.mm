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

// #define DND_DEBUG_TRACE
class GDndSourcePriv;

@interface LDragSource : NSObject<NSDraggingSource>
{
}
@property GDndSourcePriv *d;
- (id)init:(GDndSourcePriv*)view;
- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
@end

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	GSurface *ExternImg;
	GRect ExternSubRgn;
	LDragSource *Wrapper;
	
	GDndSourcePriv()
	{
		ExternImg = NULL;
		Wrapper = NULL;
		ExternSubRgn.ZOff(-1, -1);
	}

	~GDndSourcePriv()
	{
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

- (NSDragOperation)draggingSession:(nonnull NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
	LgiAssert(0);
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
		#if 1
		for (auto f : Files)
		{
			// GString ef = LgiEscapeString(" ", f, -1);
			OutputData->Data.New().OwnStr(NewStr(f));
		}
		#else
		if (Files.Length() == 1)
		{
			GUri u;
			u.Protocol = NewStr("file");
			u.Host = NewStr("localhost");
			u.Path = NewStr(Files.First());
			GAutoString Uri = u.GetUri();
			OutputData->Data.New().OwnStr(LgiDecodeUri(Uri));
		}
		#endif

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
		img = [[NSImage alloc] initWithSize:NSMakeSize(32, 32)];
	}
	
	if (!d->Wrapper)
		d->Wrapper = [[LDragSource alloc] init:d];
	
	NSPasteboard *pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
	[h.p dragImage:img at:NSZeroPoint offset:NSZeroSize event:Event.p pasteboard:pboard source:d->Wrapper slideBack:YES ];

	return DROPEFFECT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////
struct DropItemFlavor
{
	int					Index;
	PasteboardItemID	ItemId;
	CFStringRef			Flavor;
	GString				FlavorStr;
};

struct DragParams
{
	GdcPt2 Pt;
	List<char> Formats;
	GArray<GDragData> Data;
	int KeyState;
	
	#if 0
	DragParams(GViewI *v, DragRef Drag, const char *DropFormat)
	{
		KeyState = 0;
		
		// Get the mouse position
		Point mouse;
		Point globalPinnedMouse;
		OSErr err = GetDragMouse(Drag, &mouse, &globalPinnedMouse);
		if (err)
		{
			printf("%s:%i - GetDragMouse failed with %i\n", _FL, err);
		}
		else
		{
			Pt.x = mouse.h;
			Pt.y = mouse.v;
			v->PointToView(Pt);
		}
		
		// Get the keyboard state
		SInt16 modifiers = 0;
		SInt16 mouseDownModifiers = 0;
		SInt16 mouseUpModifiers = 0;
		err = GetDragModifiers(Drag, &modifiers, &mouseDownModifiers, &mouseUpModifiers);
		if (err)
		{
			printf("%s:%i - GetDragModifiers failed with %i\n", _FL, err);
		}
		else
		{
			if (modifiers & cmdKey)
				KeyState |= LGI_EF_SYSTEM;
			if (modifiers & shiftKey)
				KeyState |= LGI_EF_SHIFT;
			if (modifiers & optionKey)
				KeyState |= LGI_EF_ALT;
			if (modifiers & controlKey)
				KeyState |= LGI_EF_CTRL;
		}

		// Get the data formats
		PasteboardRef Pb;
		OSStatus e = GetDragPasteboard(Drag, &Pb);
		GArray<DropItemFlavor> ItemFlavors;
		if (e)
		{
			printf("%s:%i - GetDragPasteboard failed with %li\n", _FL, e);
		}
		else
		{
			GHashTbl<char*, int> Map(32, false, NULL, -1);
			ItemCount Items = 0;
			PasteboardGetItemCount(Pb, &Items);
			
			if (DropFormat)
				printf("Items=%li\n", Items);
			
			for (CFIndex i=1; i<=Items; i++)
			{
				PasteboardItemID Item;
				e = PasteboardGetItemIdentifier(Pb, i, &Item);
				if (e)
				{
					printf("%s:%i - PasteboardGetItemIdentifier[%li]=%li\n", _FL, i-1, e);
					continue;
				}
				
				CFArrayRef FlavorTypes;
				e = PasteboardCopyItemFlavors(Pb, Item, &FlavorTypes);
				if (e)
				{
					printf("%s:%i - PasteboardCopyItemFlavors[%li]=%li\n", _FL, i-1, e);
					continue;
				}
				
				CFIndex Types = CFArrayGetCount(FlavorTypes);
				if (DropFormat)
					printf("[%li] FlavorTypes=%li\n", i, Types);
				for (CFIndex t=0; t<Types; t++)
				{
					CFStringRef flavor = (CFStringRef)CFArrayGetValueAtIndex(FlavorTypes, t);
					if (flavor)
					{
						GString n = flavor;

						if (DropFormat)
						{
							int CurIdx = Map.Find(n);
							if (CurIdx < 0)
							{
								CurIdx = Map.Length();
								Map.Add(n, CurIdx);
							}
							
							printf("[%li][%li]='%s' = %i\n", i, t, n.Get(), CurIdx);
							DropItemFlavor &Fl = ItemFlavors.New();
							Fl.Index = CurIdx;
							Fl.ItemId = Item;
							Fl.Flavor = flavor;
							Fl.FlavorStr = n;
						}
						else
						{
							Formats.Insert(NewStr(n));
						}
					}
				}
				
				if (ItemFlavors.Length())
				{
					for (unsigned i=0; i<ItemFlavors.Length(); i++)
					{
						CFDataRef Ref;
						DropItemFlavor &Fl = ItemFlavors[i];
						e = PasteboardCopyItemFlavorData(Pb, Fl.ItemId, Fl.Flavor, &Ref);
						if (e)
						{
							printf("%s:%i - PasteboardCopyItemFlavorData failed with %lu.\n", _FL, e);
						}
						else
						{
							GDragData &dd = Data[Fl.Index];
							if (!dd.Format)
								dd.Format = Fl.FlavorStr;
							
							CFIndex Len = CFDataGetLength(Ref);
							const UInt8 *Ptr = CFDataGetBytePtr(Ref);
							if (Len > 0 && Ptr != NULL)
							{
								uint8 *Cp = new uint8[Len+1];
								if (Cp)
								{
									memcpy(Cp, Ptr, Len);
									Cp[Len] = 0;
									
									GVariant *v = &dd.Data[i];
									if (!_stricmp(LGI_LgiDropFormat, DropFormat))
									{
										GDragDropSource *Src = NULL;
										if (Len == sizeof(Src))
										{
											Src = *((GDragDropSource**)Ptr);
											v->Empty();
											v->Type = GV_VOID_PTR;
											v->Value.Ptr = Src;
										}
										else LgiAssert(!"Wrong pointer size");
									}
									else if (!_stricmp(DropFormat, "public.file-url"))
									{
										GUri u((char*) Cp);
										Boolean ret = false;
										if (u.Protocol &&
											!_stricmp(u.Protocol, "file") &&
											u.Path &&
											!_strnicmp(u.Path, "/.file/", 7))
										{
											// Decode File reference URL
											CFURLRef url = CFURLCreateWithBytes(NULL, Cp, Len, kCFStringEncodingUTF8, NULL);
											if (url)
											{
												UInt8 buffer[MAX_PATH];
												ret = CFURLGetFileSystemRepresentation(url, true, buffer, sizeof(buffer));
												if (ret)
												{
													*v = (char*)buffer;
												}
											}
										}
										
										if (!ret) // Otherwise just pass the string along...
											*v = (char*) Cp;
									}
									else
									{
										v->SetBinary(Len, Cp, true);
									}
								}
							}
							else
							{
								printf("%s:%i - Pasteboard data error: %p %li.\n", _FL, Ptr, Len);
							}
							
							CFRelease(Ref);
						}
					}
				}
				ItemFlavors.Length(0);
				CFRelease(FlavorTypes);
			}
		}
	}
	
	~DragParams()
	{
		Formats.DeleteArrays();
	}
	
	DragActions Map(int Accept)
	{
		DragActions a = 0;
		if (Accept & DROPEFFECT_COPY)
			a |= kDragActionCopy;
		if (Accept & DROPEFFECT_MOVE)
			a |= kDragActionMove;
		if (Accept & DROPEFFECT_LINK)
			a |= kDragActionAlias;
		return a;
	}
	#endif
};

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
			printf("%s:%i - Error\n", _FL);
		}
	}
}

#if 0
int GDragDropTarget::OnDrop(GArray<GDragData> &DropData,
							GdcPt2 Pt,
							int KeyState)
{
	if (DropData.Length() == 0 ||
		DropData[0].Data.Length() == 0)
		return DROPEFFECT_NONE;
	
	char *Fmt = DropData[0].Format;
	GVariant *Var = &DropData[0].Data[0];
	return OnDrop(Fmt, Var, Pt, KeyState);
}

OSStatus GDragDropTarget::OnDragWithin(GView *v, DragRef Drag)
{
	GDragDropTarget *Target = this;
	GAutoPtr<DragParams> param(new DragParams(v, Drag, NULL));

	// Call the handler
	int Accept = Target->WillAccept(param->Formats, param->Pt, param->KeyState);
	for (GViewI *p = v->GetParent(); param && !Accept && p; p = p->GetParent())
	{
		GDragDropTarget *pt = p->DropTarget();
		if (pt)
		{
			param.Reset(new DragParams(p, Drag, NULL));
			Accept = pt->WillAccept(param->Formats, param->Pt, param->KeyState);
			if (Accept)
			{
				v = p->GetGView();
				Target = pt;
				break;
			}
		}
	}
	if (Accept)
	{
		v->d->AcceptedDropFormat.Reset(NewStr(param->Formats.First()));
		LgiAssert(v->d->AcceptedDropFormat.Get());
	}

	#if 0
	printf("kEventControlDragWithin %ix%i accept=%i class=%s (",
		param->Pt.x, param->Pt.y,
		Accept,
		v->GetClass());
	for (char *f = param->Formats.First(); f; f = param->Formats.Next())
		printf("%s ", f);
	printf(")\n");
	#endif

	SetDragDropAction(Drag, param->Map(Accept));

	return noErr;
}

OSStatus GDragDropTarget::OnDragReceive(GView *v, DragRef Drag)
{
	int Accept = 0;
	GView *DropView = NULL;
	for (GView *p = v; p; p = p->GetParent() ? p->GetParent()->GetGView() : NULL)
	{
		if (p->d->AcceptedDropFormat)
		{
			DropView = p;
			break;
		}
	}
	if (DropView &&
		DropView->d->AcceptedDropFormat)
	{
		GDragDropTarget *pt = DropView->DropTarget();
		if (pt)
		{
			DragParams p(DropView, Drag, DropView->d->AcceptedDropFormat);
			int Accept = pt->OnDrop(p.Data,
									p.Pt,
									p.KeyState);
			SetDragDropAction(Drag, p.Map(Accept));
		}
	}
	else
	{
		printf("%s:%i - No accepted drop format. (view=%s, DropView=%p)\n",
			_FL,
			v->GetClass(),
			DropView);
		SetDragDropAction(Drag, kDragActionNothing);
	}
	
	return noErr;
}
#endif

