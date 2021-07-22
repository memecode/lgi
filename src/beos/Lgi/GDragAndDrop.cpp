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

class GDndSourcePriv
{
public:
	LAutoString CurrentFormat;
};

/////////////////////////////////////////////////////////////////////////////////////////
LDragDropSource::LDragDropSource()
{
	d = new GDndSourcePriv;
	OnRegister(true);
}

LDragDropSource::~LDragDropSource()
{
	DeleteObj(d);
}

bool LDragDropSource::SetIcon(LSurface *Img, LRect *SubRgn)
{
	return false;
}

bool LDragDropSource::GetData(LArray<LDragData> &DragData)
{
	if (DragData.Length() == 0)
		return false;

	// Call the deprecated version of 'GetData'
	LVariant *v = &DragData[0].Data[0];
	char *fmt = DragData[0].Format;
	if (!v || !fmt)
		return false;

	return GetData(v, fmt);
}

bool LDragDropSource::CreateFileDrop(LDragData *OutputData, LMouse &m, LString::Array &Files)
{
	if (OutputData && Files.First())
	{
	}

	return false;
}

int LDragDropSource::Drag(LView *SourceWnd, int Effect)
{
	LAssert(SourceWnd);
	if (!SourceWnd)
		return -1;

	if (SourceWnd)
	{
		BMessage Msg(M_DRAG_DROP);
		Msg.AddPointer("LDragDropSource", this);

		LMouse m;
		SourceWnd->GetMouse(m);
		BRect r(m.x, m.y, m.x+14, m.y+14);

		SourceWnd->Handle()->DragMessage(&Msg, r);

		return DROPEFFECT_COPY;
	}

	return DROPEFFECT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////
LDragDropTarget::LDragDropTarget()
{
	To = 0;
}

LDragDropTarget::~LDragDropTarget()
{
	Formats.DeleteArrays();
}

void LDragDropTarget::SetWindow(LView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		// To->DropTargetPtr() = this;
		Status = To->DropTarget(true);
		if (To->Handle())
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", _FL);
		}
	}
}

int LDragDropTarget::OnDrop(LArray<LDragData> &DropData,
							LPoint Pt,
							int KeyState)
{
	if (DropData.Length() == 0 ||
		DropData[0].Data.Length() == 0)
		return DROPEFFECT_NONE;
	
	char *Fmt = DropData[0].Format;
	LVariant *Var = &DropData[0].Data[0];
	return OnDrop(Fmt, Var, Pt, KeyState);
}
