#include "lgi/common/Lgi.h"
#include "lgi/common/RichTextEdit.h"
#include "lgi/common/GdcTools.h"
#include "lgi/common/Menu.h"
#include "lgi/common/Net.h"
#include "lgi/common/Uri.h"

#include "RichTextEditPriv.h"

#define LOADER_THREAD_LOGGING		1
#if LOADER_THREAD_LOGGING
#define LOADER_LOG(...)				LgiTrace(__VA_ARGS__)
#else
#define LOADER_LOG(...)				;
#endif

#define TIMEOUT_LOAD_PROGRESS		100 // ms

int ImgScales[] = { 15, 25, 50, 75, 100 };

class ImageLoader : public LEventTargetThread, public Progress
{
	LString File;
	LEventSinkI *Sink = NULL;
	LSurface *Img = NULL;
	LAutoPtr<LFilter> Filter;
	bool SurfaceSent = false;
	int64 Ts = 0;
	LAutoPtr<LStream> In;

public:
	ImageLoader(LEventSinkI *s) :
		LEventTargetThread("ImageLoader"),
		Sink(s)
	{
	}

	~ImageLoader()
	{
		Progress::Cancel(true);
		LOADER_LOG("%s:%i - ~ImageLoader\n", _FL);
	}
	
	const char *GetClass() override { return "ImageLoader"; }

	void Value(int64 v) override
	{
		Progress::Value(v);

		if (!SurfaceSent)
		{
			SurfaceSent = true;
			PostSink(M_IMAGE_SET_SURFACE, (LMessage::Param)Img, (LMessage::Param)In.Release());
		}

		int64 Now = LCurrentTime();
		if (Now - Ts > TIMEOUT_LOAD_PROGRESS)
		{
			Ts = Now;
			PostSink(M_IMAGE_PROGRESS, (LMessage::Param)v);
		}
	}

	bool PostSink(int Cmd, LMessage::Param a = 0, LMessage::Param b = 0)
	{
		for (int i=0; i<50; i++)
		{
			if (Sink->PostEvent(Cmd, a, b))
				return true;
			LSleep(1);
		}

		LAssert(!"PostSink failed.");
		return false;
	}

	LMessage::Result OnEvent(LMessage *Msg) override
	{
		switch (Msg->Msg())
		{
			case M_IMAGE_LOAD_FILE:
			{
				LAutoPtr<LString> Str((LString*)Msg->A());
				File = *Str;

				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_LOAD_FILE): '%s'\n", _FL, File.Get());
				
				Filter = LFilterFactory::New(File, O_READ, NULL);
				if (!Filter)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): no filter\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				if (!In.Reset(new LFile) ||
					!In->Open(File, O_READ))
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): can't read\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				if (!(Img = new LMemDC(_FL)))
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): alloc err\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				Filter->SetProgress(this);

				Ts = LCurrentTime();
				LFilter::IoStatus Status = Filter->ReadImage(Img, In);
				if (Status != LFilter::IoSuccess)
				{
					if (Status == LFilter::IoComponentMissing)
					{
						LString *s = new LString(Filter->GetComponentName());
						LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_COMPONENT_MISSING)\n", _FL);
						return PostSink(M_IMAGE_COMPONENT_MISSING, (LMessage::Param)s);
					}

					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): Filter::ReadImage err\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				if (!SurfaceSent)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_SET_SURFACE)\n", _FL);
					PostSink(M_IMAGE_SET_SURFACE, (LMessage::Param)Img, (LMessage::Param)In.Release());
				}

				LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_FINISHED)\n", _FL);
				PostSink(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_LOAD_STREAM:
			{
				LAutoPtr<LStreamI> Stream((LStreamI*)Msg->A());
				LAutoPtr<LString> FileName((LString*)Msg->B());
				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_LOAD_STREAM)\n", _FL);
				if (!Stream)
				{
					LAssert(!"No stream.");
					return PostSink(M_IMAGE_ERROR);
				}
				
				LMemStream *Mem = new LMemStream(Stream, 0, -1);
				In.Reset(Mem);
				Filter = LFilterFactory::New(FileName ? *FileName : 0, O_READ, (const uchar*)Mem->GetBasePtr());
				if (!Filter)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): no filter\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				if (!(Img = new LMemDC(_FL)))
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): alloc err\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				Filter->SetProgress(this);

				Ts = LCurrentTime();
				LFilter::IoStatus Status = Filter->ReadImage(Img, Mem);
				if (Status != LFilter::IoSuccess)
				{
					if (Status == LFilter::IoComponentMissing)
					{
						LString *s = new LString(Filter->GetComponentName());
						LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_COMPONENT_MISSING)\n", _FL);
						return PostSink(M_IMAGE_COMPONENT_MISSING, (LMessage::Param)s);
					}

					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): Filter::ReadImage err\n", _FL);
					return PostSink(M_IMAGE_ERROR);
				}

				if (!SurfaceSent)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_SET_SURFACE)\n", _FL);
					PostSink(M_IMAGE_SET_SURFACE, (LMessage::Param)Img, (LMessage::Param)In.Release());
				}

				LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_FINISHED)\n", _FL);
				PostSink(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_RESAMPLE:
			{
				auto Dst = (LSurface*) Msg->A();
				auto Src = (LSurface*) Msg->B();
				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_RESAMPLE)\n", _FL);
				if (Src && Dst)
				{
					ResampleDC(Dst, Src);
					if (PostSink(M_IMAGE_RESAMPLE))
						LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_RESAMPLE)\n", _FL);
					else
						LgiTrace("%s:%i - Error sending re-sample msg.\n", _FL);
				}
				else
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): ptr err %p %p\n", _FL, Src, Dst);
					return PostSink(M_IMAGE_ERROR);
				}
				break;
			}
			case M_IMAGE_COMPRESS:
			{
				LSurface *img = (LSurface*)Msg->A();
				auto si = (LRichTextPriv::ImageBlock::ScaleInf*)Msg->B();
				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_COMPRESS) si: %s\n", _FL, si->ToString().Get());
				if (!img || !si)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): invalid ptr\n", _FL);
					PostSink(M_IMAGE_ERROR, (LMessage::Param) new LString("Invalid pointer."));
					break;
				}
				
				auto f = LFilterFactory::New("a.jpg", O_READ, NULL);
				if (!f)
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): No JPEG filter available\n", _FL);
					PostSink(M_IMAGE_ERROR, (LMessage::Param) new LString("No JPEG filter available."));
					break;
				}

				LAutoPtr<LSurface> scaled;
				if (img->X() != si->Sz.x ||
					img->Y() != si->Sz.y)
				{
					if (!scaled.Reset(new LMemDC(_FL, si->Sz.x, si->Sz.y, img->GetColourSpace())))
						break;
					ResampleDC(scaled, img, NULL, NULL);
					img = scaled;
				}

				LXmlTag Props;
				f->Props = &Props;
				Props.SetAttr(LGI_FILTER_QUALITY, RICH_TEXT_RESIZED_JPEG_QUALITY);
				
				LAutoPtr<LMemStream> jpg(new LMemStream(1024));
				if (!f->WriteImage(jpg, img))
				{
					LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ERROR): Image compression failed\n", _FL);
					PostSink(M_IMAGE_ERROR, (LMessage::Param) new LString("Image compression failed."));
					break;
				}

				LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_COMPRESS)\n", _FL);
				PostSink(M_IMAGE_COMPRESS, (LMessage::Param)jpg.Release(), (LMessage::Param)si);
				break;
			}
			case M_IMAGE_ROTATE:
			{
				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_ROTATE)\n", _FL);
				LSurface *Img = (LSurface*)Msg->A();
				if (!Img)
				{
					LAssert(!"No image.");
					break;
				}

				RotateDC(Img, Msg->B() == 1 ? 90 : 270);
				LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_ROTATE)\n", _FL);
				PostSink(M_IMAGE_ROTATE);
				break;
			}
			case M_IMAGE_FLIP:
			{
				LOADER_LOG("%s:%i - Thread.Receive(M_IMAGE_FLIP)\n", _FL);
				auto Img = (LSurface*)Msg->A();
				if (!Img)
				{
					LAssert(!"No image.");
					break;
				}

				if (Msg->B() == 1)
					FlipXDC(Img);
				else
					FlipYDC(Img);
				LOADER_LOG("%s:%i - Thread.Send(M_IMAGE_FLIP)\n", _FL);
				PostSink(M_IMAGE_FLIP);
				break;
			}
			case M_CLOSE:
			{
				LOADER_LOG("%s:%i - Thread.Receive(M_CLOSE)\n", _FL);
				EndThread();
				break;
			}
		}

		return 0;
	}
};

LRichTextPriv::ImageBlock::ImageBlock(LRichTextPriv *priv) : Block(priv)
{
	Pos.ZOff(-1, -1);
	Size.x = 200;
	Size.y = 64;
	SourceValid.ZOff(-1, -1);

	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

LRichTextPriv::ImageBlock::ImageBlock(const ImageBlock *Copy) : Block(Copy->d)
{
	LayoutDirty = true;
	SourceImg.Reset(new LMemDC(_FL, Copy->SourceImg));
	Size = Copy->Size;

	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;
}

LRichTextPriv::ImageBlock::~ImageBlock()
{
	LAssert(ThreadBusy == 0);
	if (ThreadHnd)
		PostThreadEvent(ThreadHnd, M_CLOSE);
	LAssert(Cursors == 0);
}

bool LRichTextPriv::ImageBlock::IsValid()
{
	return true;
}

bool LRichTextPriv::ImageBlock::IsBusy(bool Stop)
{
	return ThreadBusy != 0;
}

void LRichTextPriv::ImageBlock::OnDimensions()
{
	Scales.Length(CountOf(ImgScales));
	for (int i=0; i<CountOf(ImgScales); i++)
	{
		ScaleInf &si = Scales[i];
		si.Sz.x = SourceImg->X() * ImgScales[i] / 100;
		si.Sz.y = SourceImg->Y() * ImgScales[i] / 100;
		si.Percent = ImgScales[i];
	
		if (si.Sz.x == SourceImg->X() &&
			si.Sz.y == SourceImg->Y() &&
			ResizeIdx < 0)
		{
			ResizeIdx = i;
			ResizeSrc = SourceDefault;
		}
	}
}

void LRichTextPriv::ImageBlock::GetCompressedSize()
{
	// Also create a JPG for the current scale (needed before 
	// we save to HTML).
	if (Scales.IdxCheck(ResizeIdx))
	{
		ScaleInf &si = Scales[ResizeIdx];
		if (!si.Compressing && !si.Compressed)
		{
			si.Compressing = true;
			LOADER_LOG("%s:%i post M_IMAGE_COMPRESS %s\n", _FL, si.ToString().Get());
			if (PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (LMessage::Param)SourceImg.Get(), (LMessage::Param)&si))
				UpdateThreadBusy(_FL, 1);
		}
	}
	else LAssert(!"ResizeIdx should be valid.");
}

void LRichTextPriv::ImageBlock::MaxImageFilter()
{
	LgiTrace("MaxImageFilter: ResizeIdx=%i ResizeSrc=%s\n", ResizeIdx, ToString(ResizeSrc));
	for (int i=0; i<CountOf(ImgScales); i++)
	{
		ScaleInf &si = Scales[i];
		LgiTrace("[%i]: %s\n", i, si.ToString().Get());
	}

	if (Scales.IdxCheck(ResizeIdx))
	{
		ScaleInf &si = Scales[ResizeIdx];
		if (!si.Compressed)
		{
			LAssert(!"No compressed image?");
			LOADER_LOG("%s:%i - no compressed image?\n", _FL);
			return;
		}

		LRichTextEdit::ImgParams params;
		params.Sz = si.Sz;
		params.Bytes = si.Compressed->GetSize();
		if (d->View->MaxImageFilter(params))
		{
			if (ResizeIdx <= 0)
				LOADER_LOG("%s:%i no more resize steps available?\n", _FL);
			else if (ResizeSrc == SourceUser)
				LOADER_LOG("%s:%i we won't override the user's request.\n", _FL);
			else
			{
				ResizeIdx--;
				LOADER_LOG("%s:%i dec ResizeIdx=%i\n", _FL, ResizeIdx);
				GetCompressedSize();
			}
		}
	}
}

bool LRichTextPriv::ImageBlock::SetImage(LAutoPtr<LSurface> Img)
{
	SourceImg = Img;
	if (!SourceImg)
		return false;

	OnDimensions();
	LayoutDirty = true;
	UpdateDisplayImg();
	if (DisplayImg)
	{
		// Update the display image by scaling it from the source...
		if (PostThreadEvent(GetThreadHandle(),
							M_IMAGE_RESAMPLE,
							(LMessage::Param) DisplayImg.Get(),
							(LMessage::Param) SourceImg.Get()))
			UpdateThreadBusy(_FL, 1);

	}
	else LayoutDirty = true;

	GetCompressedSize();
	
	return true;
}

bool LRichTextPriv::ImageBlock::Load(const char *Src)
{
	if (Src)
		Source = Src;

	LAutoPtr<LStreamI> Stream;
	
	auto a = Source.Strip().Split(":", 1);
	if (a.Length() > 1 &&
		a[0].Equals("cid"))
	{
		auto Env = d->View->GetEnv();
		if (!Env)
			return false;
		
		auto j = Env->NewJob();
		if (!j)
			return false;
		
		j->Uri.Reset(NewStr(Source));
		j->Env = Env;
		j->Pref = LDocumentEnv::LoadJob::FmtStream;
		j->UserUid = d->View->GetDocumentUid();

		auto Result = Env->GetContent(j);
		if (Result == LDocumentEnv::LoadImmediate)
		{
			StreamMimeType = j->MimeType;
			ContentId = j->ContentId.Strip("<>");
			FileName = j->Filename;

			if (j->Stream)
			{
				Stream = j->Stream;
			}
			else if (j->pDC)
			{
				SourceImg = j->pDC;
				return true;
			}
		}
		else if (Result == LDocumentEnv::LoadDeferred)
		{
			LAssert(!"Impl me?");
		}
	}
	else if (LFileExists(Source))
	{
		FileName = Source;
		FileMimeType = LAppInst->GetFileMimeType(Source);
	}
	else
		return false;

	if (!FileName && !Stream)
		return false;

	if (Stream)
	{
		LOADER_LOG("%s:%i - Posting M_IMAGE_LOAD_STREAM\n", _FL);
		if (PostThreadEvent(GetThreadHandle(), M_IMAGE_LOAD_STREAM, (LMessage::Param)Stream.Release(), (LMessage::Param) (FileName ? new LString(FileName) : NULL)))
		{
			UpdateThreadBusy(_FL, 1);
			return true;
		}
	}
	
	if (FileName)
	{
		LOADER_LOG("%s:%i - Posting M_IMAGE_LOAD_FILE\n", _FL);
		if (PostThreadEvent(GetThreadHandle(), M_IMAGE_LOAD_FILE, (LMessage::Param)new LString(FileName)))
		{
			UpdateThreadBusy(_FL, 1);
			return true;
		}
	}
	
	return false;
}

int LRichTextPriv::ImageBlock::GetLines() const
{
	return 1;
}

bool LRichTextPriv::ImageBlock::OffsetToLine(ssize_t Offset, int *ColX, LArray<int> *LineY)
{
	if (ColX)
		*ColX = Offset > 0;
	if (LineY)
		LineY->Add(0);
	return true;
}

ssize_t LRichTextPriv::ImageBlock::LineToOffset(ssize_t Line)
{
	return 0;
}

void LRichTextPriv::ImageBlock::Dump()
{
}

LNamedStyle *LRichTextPriv::ImageBlock::GetStyle(ssize_t At)
{
	return Style;
}

void LRichTextPriv::ImageBlock::SetStyle(LNamedStyle *s)
{
	if ((Style = s))
	{
		LFont *Fnt = d->GetFont(s);
		LayoutDirty = true;
		LAssert(Fnt != NULL);

		Margin.x1 = Style->MarginLeft().ToPx(Pos.X(), Fnt);
		Margin.y1 = Style->MarginTop().ToPx(Pos.Y(), Fnt);
		Margin.x2 = Style->MarginRight().ToPx(Pos.X(), Fnt);
		Margin.y2 = Style->MarginBottom().ToPx(Pos.Y(), Fnt);

		Border.x1 = Style->BorderLeft().ToPx(Pos.X(), Fnt);
		Border.y1 = Style->BorderTop().ToPx(Pos.Y(), Fnt);
		Border.x2 = Style->BorderRight().ToPx(Pos.X(), Fnt);
		Border.y2 = Style->BorderBottom().ToPx(Pos.Y(), Fnt);

		Padding.x1 = Style->PaddingLeft().ToPx(Pos.X(), Fnt);
		Padding.y1 = Style->PaddingTop().ToPx(Pos.Y(), Fnt);
		Padding.x2 = Style->PaddingRight().ToPx(Pos.X(), Fnt);
		Padding.y2 = Style->PaddingBottom().ToPx(Pos.Y(), Fnt);
	}
}

ssize_t LRichTextPriv::ImageBlock::Length()
{
	return IsDeleted ? 0 : 1;
}

bool LRichTextPriv::ImageBlock::ToHtml(LStream &s, LArray<LDocView::ContentMedia> *Media, LRange *Rng)
{
	LUri uri(Source);
	if (uri.IsProtocol("http") ||
		uri.IsProtocol("https") ||
		uri.IsProtocol("ftp"))
	{
		// Nothing to do...?
	}
	else if (Media)
	{
		bool ValidSourceFile = LFileExists(Source);
		LDocView::ContentMedia &Cm = Media->New();
		
		int Idx = LRand() % 10000;
		if (!ContentId)
			ContentId.Printf("%u@memecode.com", Idx);
		Cm.Id = ContentId;

		LString Style;
		auto Si = ResizeIdx >= 0 && ResizeIdx < (int)Scales.Length() ? &Scales[ResizeIdx] : NULL;
		if (Si && Si->Compressed)
		{
			// Attach a copy of the resized JPEG...
			Si->Compressed->SetPos(0);
			Cm.Stream.Reset(new LMemStream(Si->Compressed, 0, -1));
			Cm.MimeType = Si->MimeType;

			if (FileName)
				Cm.FileName = LGetLeaf(FileName);
			else if (Cm.MimeType.Equals("image/jpeg"))
				Cm.FileName.Printf("img%u.jpg", Idx);
			else if (Cm.MimeType.Equals("image/png"))
				Cm.FileName.Printf("img%u.png", Idx);
			else if (Cm.MimeType.Equals("image/tiff"))
				Cm.FileName.Printf("img%u.tiff", Idx);
			else if (Cm.MimeType.Equals("image/gif"))
				Cm.FileName.Printf("img%u.gif", Idx);
			else if (Cm.MimeType.Equals("image/bmp"))
				Cm.FileName.Printf("img%u.bmp", Idx);
			else
			{
				LAssert(!"Unknown image mime type?");
				Cm.FileName.Printf("img%u", Idx);
			}
		}
		else if (ValidSourceFile)
		{
			// Attach the original file...
			Cm.MimeType = LAppInst->GetFileMimeType(Source);
			Cm.FileName = LGetLeaf(Source);

			LAutoPtr<LFile> f(new LFile);
			if (f)
			{
				if (f->Open(Source, O_READ))
					Cm.Stream.Reset(f.Release());
				else
					LgiTrace("%s:%i - Failed to open link image '%s'.\n", _FL, Source.Get());
			}
		}
		else
		{
			LAssert(!"No valid source.");
			return false;
		}

		LAssert(Cm.MimeType != NULL);

		if (DisplayImg &&
			SourceImg &&
			DisplayImg->X() != SourceImg->X())
		{
			int Dx = DisplayImg->X();
			Style.Printf(" style=\"width:%ipx\"", Dx);
		}
		
		if (Cm.Stream)
		{
			s.Print("<img%s src=\"", Style ? Style.Get() : "");
			if (d->HtmlLinkAsCid)
				s.Print("cid:%s", Cm.Id.Get());
			else
				s.Print("%s", Cm.FileName.Get());
			s.Print("\">\n");

			LAssert(Cm.Valid());
			return true;
		}
	}

	s.Print("<img src=\"%s\">\n", Source.Get());
	return true;
}

bool LRichTextPriv::ImageBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	if (LayoutDirty)
	{
		Cursor->Pos.ZOff(-1, -1);	// This is valid behaviour... need to 
									// wait for layout before getting cursor
									// position.
		return false;
	}

	Cursor->Pos = ImgPos;
	Cursor->Line = Pos;
	if (Cursor->Offset == 0)
	{
		Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
	}
	else if (Cursor->Offset == 1)
	{
		Cursor->Pos.x1 = Cursor->Pos.x2 - 1;
	}

	return true;
}

bool LRichTextPriv::ImageBlock::HitTest(int blkOffset, HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Near = false;
	htr.LineHint = 0;
	htr.BlkOffset = blkOffset;

	int Cx = ImgPos.x1 + (ImgPos.X() / 2);
	if (htr.In.x < Cx)
		htr.Idx = 0;
	else
		htr.Idx = 1;

	return true;
}

void LRichTextPriv::ImageBlock::OnPaint(PaintContext &Ctx)
{
	bool ImgSelected = Ctx.SelectBeforePaint(this);
			
	// Paint margins, borders and padding...
	LRect r = Pos;

	r.x1 -= Margin.x1;
	r.y1 -= Margin.y1;
	r.x2 -= Margin.x2;
	r.y2 -= Margin.y2;
	LCss::ColorDef BorderStyle;
	if (Style)
		BorderStyle = Style->BorderLeft().Color;
	LColour BorderCol(222, 222, 222);
	if (BorderStyle.Type == LCss::ColorRgb)
		BorderCol.Set(BorderStyle.Rgb32, 32);

	Ctx.DrawBox(r, Margin, Ctx.Colours[Unselected].Back);
	Ctx.DrawBox(r, Border, BorderCol);
	Ctx.DrawBox(r, Padding, Ctx.Colours[Unselected].Back);

	LRegion unpainted(r);

	if (!DisplayImg &&
		SourceImg &&
		SourceImg->X() > r.X())
	{
		UpdateDisplayImg();
	}

	LSurface *Src = DisplayImg ? DisplayImg : SourceImg;
	if (Src)
	{
		if (SourceValid.Valid())
		{
			LRect Bounds(0, 0, Size.x-1, Size.y-1);
			Bounds.Offset(r.x1, r.y1);

			Ctx.pDC->Colour(L_MED);
			Ctx.pDC->Box(&Bounds);
			Bounds.Inset(1, 1);
			Ctx.pDC->Colour(L_WORKSPACE);
			Ctx.pDC->Rectangle(&Bounds);

			LRect rr(0, 0, Src->X()-1, SourceValid.y2 / Scale);
			Ctx.pDC->Blt(ImgPos.x1, ImgPos.y1, Src, &rr);
			rr.Offset(ImgPos.x1, ImgPos.y1);
			unpainted.Subtract(&rr);
		}
		else
		{
			LRect rr;
			if (Ctx.Type == LRichTextPriv::Selected)
			{
				if (!SelectImg &&
					SelectImg.Reset(new LMemDC(_FL, Src->X(), Src->Y(), System32BitColourSpace)))
				{
					SelectImg->Blt(0, 0, Src);

					int Op = SelectImg->Op(GDC_ALPHA);
					LColour c = Ctx.Colours[LRichTextPriv::Selected].Back;
					c.Rgb(c.r(), c.g(), c.b(), 0xa0);
					SelectImg->Colour(c);
					SelectImg->Rectangle();
					SelectImg->Op(Op);
				}

				rr = SelectImg->Bounds();
				Ctx.pDC->Blt(ImgPos.x1, ImgPos.y1, SelectImg);
			}
			else
			{
				rr = Src->Bounds();
				Ctx.pDC->Blt(ImgPos.x1, ImgPos.y1, Src);
			}

			rr.Offset(ImgPos.x1, ImgPos.y1);
			unpainted.Subtract(&rr);
		}
	}
	else
	{
		// Drag missing image...
		r = ImgPos;
		LColour cBack(245, 245, 245);
		Ctx.pDC->Colour(ImgSelected ? cBack.Mix(Ctx.Colours[Selected].Back) : cBack);
		Ctx.pDC->Rectangle(&r);

		Ctx.pDC->Colour(L_LOW);
		uint Ls = Ctx.pDC->LineStyle(LSurface::LineAlternate);
		Ctx.pDC->Box(&r);
		Ctx.pDC->LineStyle(Ls);

		int Cx = r.x1 + (r.X() >> 1);
		int Cy = r.y1 + (r.Y() >> 1);
		Ctx.pDC->Colour(LColour::Red);
		int Sz = 5;
		Ctx.pDC->Line(Cx - Sz, Cy - Sz, Cx + Sz, Cy + Sz);
		Ctx.pDC->Line(Cx - Sz, Cy - Sz + 1, Cx + Sz - 1, Cy + Sz);
		Ctx.pDC->Line(Cx - Sz + 1, Cy - Sz, Cx + Sz, Cy + Sz - 1);

		Ctx.pDC->Line(Cx + Sz, Cy - Sz, Cx - Sz, Cy + Sz);
		Ctx.pDC->Line(Cx + Sz - 1, Cy - Sz, Cx - Sz, Cy + Sz - 1);
		Ctx.pDC->Line(Cx + Sz, Cy - Sz + 1, Cx - Sz + 1, Cy + Sz);

		unpainted.Subtract(&ImgPos);
	}

	ImgSelected = Ctx.SelectAfterPaint(this);

	if (ImgSelected)
	{
		Ctx.pDC->Colour(Ctx.Colours[Selected].Back);
		Ctx.pDC->Rectangle(ImgPos.x2 + 1, ImgPos.y1, ImgPos.x2 + 7, ImgPos.y2);
	}

	if (Ctx.Cursor &&
		Ctx.Cursor->Blk == this &&
		Ctx.Cursor->Blink &&
		d->View->Focus())
	{
		Ctx.pDC->Colour(CursorColour);
		if (Ctx.Cursor->Pos.Valid())
			Ctx.pDC->Rectangle(&Ctx.Cursor->Pos);
		else
			Ctx.pDC->Rectangle(Pos.x1, Pos.y1, Pos.x1, Pos.y2);
	}
}

bool LRichTextPriv::ImageBlock::OnLayout(Flow &flow)
{
	LayoutDirty = false;

	flow.Left += Margin.x1;
	flow.Right -= Margin.x2;
	flow.CurY += Margin.y1;

	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY-1; // Start with a 0px height.

	flow.Left += Border.x1 + Padding.x1;
	flow.Right -= Border.x2 + Padding.x2;
	flow.CurY += Border.y1 + Padding.y1;

	ImgPos.x1 = Pos.x1 + Padding.x1;
	ImgPos.y1 = Pos.y1 + Padding.y1;	

	ImgPos.x2 = ImgPos.x1 + Size.x - 1;
	ImgPos.y2 = ImgPos.y1 + Size.y - 1;

	int Px2 = ImgPos.x2 + Padding.x2;
	if (Px2 < Pos.x2)
		Pos.x2 = ImgPos.x2 + Padding.x2;
	Pos.y2 = ImgPos.y2 + Padding.y2;

	flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
	flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
	flow.Right += Margin.x2 + Border.x2 + Padding.x2;

	return true;
}

ssize_t LRichTextPriv::ImageBlock::GetTextAt(ssize_t Offset, LArray<StyleText*> &t)
{
	// No text to get
	return 0;
}

ssize_t LRichTextPriv::ImageBlock::CopyAt(ssize_t Offset, ssize_t Chars, LArray<uint32_t> *Text)
{
	// No text to copy
	return 0;
}

bool LRichTextPriv::ImageBlock::Seek(SeekType To, BlockCursor &Cursor)
{
	switch (To)
	{
		case SkLineStart:
		{
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkLineEnd:
		{
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		case SkLeftChar:
		{
			if (Cursor.Offset != 1)
				return false;
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkRightChar:
		{
			if (Cursor.Offset != 0)
				return false;
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		default:
		{
			return false;
			break;
		}
	}

	return true;
}

ssize_t LRichTextPriv::ImageBlock::FindAt(ssize_t StartIdx, const uint32_t *Str, LFindReplaceCommon *Params)
{
	// No text to find in
	return -1;
}

void LRichTextPriv::ImageBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
}

bool LRichTextPriv::ImageBlock::DoContext(LSubMenu &s, LPoint Doc, ssize_t Offset, bool TopOfMenu)
{
	if (SourceImg && !TopOfMenu)
	{
		s.AppendSeparator();
		
		LSubMenu *c = s.AppendSub("Transform Image");
		if (c)
		{
			c->AppendItem("Rotate Clockwise", ID_CLOCKWISE); 
			c->AppendItem("Rotate Anti-clockwise", ID_ANTI_CLOCKWISE); 
			c->AppendItem("Horizontal Flip", ID_X_FLIP); 
			c->AppendItem("Vertical Flip", ID_Y_FLIP); 
		}

		c = s.AppendSub("Scale Image");
		if (c)
		{
			for (unsigned i=0; i<Scales.Length(); i++)
			{
				ScaleInf &si = Scales[i];
				
				LString m;
				si.Sz.x = SourceImg->X() * ImgScales[i] / 100;
				si.Sz.y = SourceImg->Y() * ImgScales[i] / 100;
				si.Percent = ImgScales[i];
				
				m.Printf("%i x %i, %i%% ", si.Sz.x, si.Sz.y, ImgScales[i]);
				if (si.Compressed)
				{
					char Sz[128];
					LFormatSize(Sz, sizeof(Sz), si.Compressed->GetSize());
					LString s;
					s.Printf(" (%s)", Sz);
					m += s;
				}
				
				auto mi = c->AppendItem(m, ID_SCALE_IMAGE+i, !IsBusy());
				if (mi && ResizeIdx == i)
				{
					mi->Checked(true);
				}
			}
		}

		return true;
	}

	return false;
}

LRichTextPriv::Block *LRichTextPriv::ImageBlock::Clone()
{
	return new ImageBlock(this);
}

void LRichTextPriv::ImageBlock::OnComponentInstall(LString Name)
{
	if (Source && !SourceImg)
	{
		// Retry the load?
		Load(Source);
	}
}

void LRichTextPriv::ImageBlock::UpdateDisplay(int yy)
{
	LRect s;
	if (DisplayImg && !SourceValid.Valid())
	{
		SourceValid = SourceImg->Bounds();
		SourceValid.y2 = yy;
		s = SourceValid;
	}
	else
	{
		s = SourceValid;
		s.y1 = s.y2 + 1;
		s.y2 = SourceValid.y2 = yy;
	}

	/* This seems to be broken...
	if (DisplayImg)
	{
		LRect d(0, s.y1 / Scale, DisplayImg->X()-1, s.y2 / Scale);

		// Do a quick and dirty nearest neighbor scale to 
		// show the user some feed back.
		LSurface *Src = SourceImg;
		LSurface *Dst = DisplayImg;
		for (int y=d.y1; y<=d.y2; y++)
		{
			int sy = y * Scale;
			int sx = d.x1 * Scale;
			for (int x=d.x1; x<=d.x2; x++, sx+=Scale)
			{
				COLOUR c = Src->Get(sx, sy);
				Dst->Colour(c);
				Dst->Set(x, y);
			}
		}
	}
	*/

	LayoutDirty = true;
	this->d->InvalidateDoc(NULL);
}

int LRichTextPriv::ImageBlock::GetThreadHandle()
{
	if (ThreadHnd == 0)
	{
		ImageLoader *il = new ImageLoader(this);
		if (il != NULL)
			ThreadHnd = il->GetHandle();
	}

	return ThreadHnd;
}

void LRichTextPriv::ImageBlock::UpdateDisplayImg()
{
	if (!SourceImg)
		return;

	Size.x = SourceImg->X();
	Size.y = SourceImg->Y();

	int ViewX = d->Areas[LRichTextEdit::ContentArea].X();
	if (ViewX > 0)
	{
		int MaxX = (int) (ViewX * 0.9);
		if (SourceImg->X() > MaxX)
		{
			double Ratio = (double)SourceImg->X() / MAX(1, MaxX);
			Scale = (int)ceil(Ratio);

			Size.x = (int)ceil((double)SourceImg->X() / Scale);
			Size.y = (int)ceil((double)SourceImg->Y() / Scale);
			if (DisplayImg.Reset(new LMemDC(_FL, Size.x, Size.y, SourceImg->GetColourSpace())))
			{
				DisplayImg->Colour(L_MED);
				DisplayImg->Rectangle();

				if (PostThreadEvent(GetThreadHandle(),
									M_IMAGE_RESAMPLE,
									(LMessage::Param)DisplayImg.Get(),
									(LMessage::Param)SourceImg.Get()))
				{
					UpdateThreadBusy(_FL, 1);
				}
			}
		}
	}
}

void LRichTextPriv::ImageBlock::UpdateThreadBusy(const char *File, int Line, int Off)
{
	if (ThreadBusy + Off >= 0)
	{
		ThreadBusy += Off;
		LOADER_LOG("%s:%i - ThreadBusy=%i\n", File, Line, ThreadBusy);
	}
	else
	{
		LOADER_LOG("%s:%i - Error: ThreadBusy=%i\n", File, Line, ThreadBusy, ThreadBusy + Off);
		LAssert(0);
	}
}

LMessage::Result LRichTextPriv::ImageBlock::OnEvent(LMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			if (!SourceImg)
				break;
			if (Msg->A() >= ID_SCALE_IMAGE &&
				Msg->A() <  ID_SCALE_IMAGE + CountOf(ImgScales))
			{
				int i = (int)Msg->A() - ID_SCALE_IMAGE;
				if (i >= 0 && i < (int)Scales.Length())
				{
					ScaleInf &si = Scales[i];
					ResizeIdx = i;
					ResizeSrc = SourceUser;
					LgiTrace("%s:%i - user setting image scale to %i\n", _FL, ResizeIdx);

					LOADER_LOG("%s:%i - Posting M_IMAGE_COMPRESS\n", _FL);
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (LMessage::Param)SourceImg.Get(), (LMessage::Param)&si))
						UpdateThreadBusy(_FL, 1);
					else
						LAssert(!"PostThreadEvent failed.");
				}
			}
			else switch (Msg->A())
			{
				case ID_CLOCKWISE:
					LOADER_LOG("%s:%i - Posting M_IMAGE_ROTATE\n", _FL);
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (LMessage::Param) SourceImg.Get(), 1))
						UpdateThreadBusy(_FL, 1);
					break;
				case ID_ANTI_CLOCKWISE:
					LOADER_LOG("%s:%i - Posting M_IMAGE_ROTATE\n", _FL);
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (LMessage::Param) SourceImg.Get(), -1))
						UpdateThreadBusy(_FL, 1);
					break;
				case ID_X_FLIP:
					LOADER_LOG("%s:%i - Posting M_IMAGE_FLIP\n", _FL);
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (LMessage::Param) SourceImg.Get(), 1))
						UpdateThreadBusy(_FL, 1);
					break;
				case ID_Y_FLIP:
					LOADER_LOG("%s:%i - Posting M_IMAGE_FLIP\n", _FL);
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (LMessage::Param) SourceImg.Get(), 0))
						UpdateThreadBusy(_FL, 1);
					break;
			}
			break;
		}
		case M_IMAGE_COMPRESS:
		{
			auto Jpg = Msg->AutoA<LMemStream>();
			auto Si = (ScaleInf*)Msg->B();
			if (!Jpg || !Si)
			{
				LAssert(0);
				LOADER_LOG("%s:%i - Error: M_IMAGE_COMPRESS bad arg\n", _FL);
				break;
			}

			Si->Compressing = false;
			Si->Compressed.Reset(Jpg.Release());
			Si->MimeType = "image/jpeg";
			LOADER_LOG("%s:%i - Received M_IMAGE_COMPRESS: %s\n", _FL, Si->ToString().Get());
			UpdateThreadBusy(_FL, -1);

			// Change the doc to dirty
			d->Dirty = true;
			d->View->SendNotify(LNotifyDocChanged);

			MaxImageFilter();
			break;
		}
		case M_IMAGE_ERROR:
		{
			LAutoPtr<LString> ErrMsg((LString*) Msg->A());
			LOADER_LOG("%s:%i - Received M_IMAGE_ERROR, posting M_CLOSE\n", _FL);
			UpdateThreadBusy(_FL, -1);
			break;
		}
		case M_IMAGE_COMPONENT_MISSING:
		{
			LAutoPtr<LString> Component((LString*) Msg->A());
			LOADER_LOG("%s:%i - Received M_IMAGE_COMPONENT_MISSING, posting M_CLOSE\n", _FL);
			UpdateThreadBusy(_FL, -1);

			if (Component)
			{
				auto t = LString(*Component).SplitDelimit(",");
				for (int i=0; i<t.Length(); i++)
					d->View->NeedsCapability(t[i]);
			}
			else LAssert(!"Missing component name.");
			break;
		}
		case M_IMAGE_SET_SURFACE:
		{
			auto File = Msg->AutoB<LStream>();
			LOADER_LOG("%s:%i - Received M_IMAGE_SET_SURFACE\n", _FL);			
			if (SourceImg.Reset((LSurface*)Msg->A()))
			{
				OnDimensions();
				for (int i=0; i<CountOf(ImgScales); i++)
				{
					ScaleInf &si = Scales[i];
					if (si.Sz.x == SourceImg->X() &&
						si.Sz.y == SourceImg->Y())
					{
						si.Compressed.Reset(File.Release());
						if (StreamMimeType)
						{
							si.MimeType = StreamMimeType;
						}
						else if (FileMimeType)
						{
							si.MimeType = FileMimeType.Get();
							FileMimeType.Empty();
						}
					}
				}
				LAssert(File.Get() == NULL); // One of the scales should have claimed the data

				UpdateDisplayImg();
				MaxImageFilter();
			}
			break;
		}
		case M_IMAGE_PROGRESS:
		{
			LOADER_LOG("%s:%i - Received M_IMAGE_PROGRESS\n", _FL);
			
			UpdateDisplay((int)Msg->A());
			break;
		}
		case M_IMAGE_FINISHED:
		{
			LOADER_LOG("%s:%i - Received M_IMAGE_FINISHED\n", _FL);
			UpdateThreadBusy(_FL, -1);

			if (SourceImg)
			{
				OnDimensions();
				UpdateDisplay(SourceImg->Y()-1);

				if
				(
					DisplayImg != NULL &&
					PostThreadEvent(GetThreadHandle(),
									M_IMAGE_RESAMPLE,
									(LMessage::Param)DisplayImg.Get(),
									(LMessage::Param)SourceImg.Get())
				)
				{
					UpdateThreadBusy(_FL, 1);
				}

				GetCompressedSize();
			}
			break;
		}
		case M_IMAGE_RESAMPLE:
		{
			LOADER_LOG("%s:%i - Received M_IMAGE_RESAMPLE\n", _FL);
			
			LayoutDirty = true;
			UpdateThreadBusy(_FL, -1);
			d->InvalidateDoc(NULL);
			SourceValid.ZOff(-1, -1);
			break;
		}
		case M_IMAGE_ROTATE:
		case M_IMAGE_FLIP:
		{
			LOADER_LOG("%s:%i - Received %s\n", _FL, Msg->Msg()==M_IMAGE_ROTATE?"M_IMAGE_ROTATE":"M_IMAGE_FLIP");
			
			LAutoPtr<LSurface> Img = SourceImg;
			UpdateThreadBusy(_FL, -1);
			SetImage(Img);
			break;
		}
		default:
			return false;
	}

	return true;
}

bool LRichTextPriv::ImageBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32_t *Str, ssize_t Chars, LNamedStyle *Style)
{
	// Can't add text to image block
	return false;
}

bool LRichTextPriv::ImageBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, LCss *Style, bool Add)
{
	// No styles to change...
	return false;
}

ssize_t LRichTextPriv::ImageBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, LArray<uint32_t> *DeletedText)
{
	// The image is one "character"
	IsDeleted = BlkOffset == 0;
	if (IsDeleted)
		return true;

	return false;
}

bool LRichTextPriv::ImageBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	// No text to change case...
	return false;
}

#ifdef _DEBUG
void LRichTextPriv::ImageBlock::DumpNodes(LTreeItem *Ti)
{
	LString s;
	s.Printf("ImageBlock style=%s", Style?Style->Name.Get():NULL);
	Ti->SetText(s);
}
#endif

