#include "lgi/common/Lgi.h"
#include "lgi/common/List.h"
#include "History.h"

//////////////////////////////////////////////////////////////
// LHistoryPopup
class LHistoryPopup : public LPopup
{
public:
	LList *Lst;
	LString Str;
	int64 Index;
	bool Ignore;
	LHistoryPopup() : LPopup(0)
	{
		Lst = 0;
		Index = -1;
		LRect r(0, 0, 300, 300);
		SetPos(r);
		Children.Insert(Lst = new LList(1, 1, 1, 100, 100));
		if (Lst)
		{
			Lst->MultiSelect(false);
			Lst->ColumnHeaders(false);
			Lst->Sunken(false);
			Lst->AddColumn("", 1000);
		}
	}
	
	void OnPaint(LSurface *pDC)
	{
		if (Lst)
		{
			#if LGI_VIEW_HANDLE
			if (!Lst->Handle())
			#endif
				AttachChildren();
			Lst->Focus(true);
		}
		
		pDC->Colour(L_BLACK);
		pDC->Box();
	}
	
	void OnPosChange()
	{
		if (Lst)
		{
			LRect r = GetClient();
			r.Inset(1, 1);
			Lst->SetPos(r);
		}
	}
	
	int OnNotify(LViewI *Ctrl, LNotification &n) override
	{
		if (Lst && Ctrl->GetId() == 1)
		{
			if (n.Type == LNotifyItemSelect)
			{
				LListItem *li = Lst->GetSelected();
				if (li)
				{
					Str = li->GetText(0);
					Index = Lst->Value();
					Visible(false);
					
					// printf("%s:%i - str=%s idx=%i\n", _FL, Str.Get(), (int)Index);
				}
				else printf("%s:%i - No selection.\n", _FL);
			}
			// else printf("%s:%i - Flags=%i\n", _FL, Flags);
		}
		else printf("%s:%i - getid=%i\n", _FL, Ctrl->GetId());
		
		return 0;
	} 
};
/////////////////////////////////////////////////////////////
// LHistory
class LHistoryPrivate
{
public:
	LHistoryPopup *Popup;
	int TargetId;
	int64 Value;
	
	LHistoryPrivate()
	{
		Popup = 0;
		TargetId = 0;
		Value = 0;
	}
};
LHistory::LHistory() :
	LDropDown(-1, 0, 0, 10, 10, 0),
	ResObject(Res_Custom)
{
	d = new LHistoryPrivate;
	SetPopup(d->Popup = new LHistoryPopup);
}
LHistory::~LHistory()
{
	DeleteObj(d);
	DeleteArrays();
}
bool LHistory::OnLayout(LViewLayoutInfo &Inf)
{
	if (!Inf.Width.Min)
		Inf.Width.Min = Inf.Width.Max = 24;
	else
		Inf.Height.Min = Inf.Height.Max = LSysFont->GetHeight() + 6;
	
	return true;
}
int LHistory::GetTargetId()
{
	return d->TargetId;
}
void LHistory::SetTargetId(int id)
{
	d->TargetId = id;
}
int64 LHistory::Value()
{
	if (d && d->Popup)
		return d->Popup->Index;
	
	return -1;
}
void LHistory::Value(int64 i)
{
	if (!d || !d->Popup)
	{
		LgiTrace("%s:%i - Invalid params.\n", _FL);
		return;
	}
	
	d->Popup->Index = i;
	if (d->Popup->Lst)
	{
		d->Popup->Lst->Value(i);
		
		LListItem *li = d->Popup->Lst->GetSelected();
		if (li)
		{
			d->Popup->Str = li->GetText(0);
			GetWindow()->SetCtrlName(d->TargetId, d->Popup->Str);
		}
	}
	else LgiTrace("%s:%i - No list?\n", _FL);
}
int LHistory::Add(const char *Str)
{
	if (!Str)
		return -1;
	int Idx = 0;
	for (auto s: *this)
	{
		if (strcmp(s, Str) == 0)
		{
			return Idx;
		}
		Idx++;
	}
	Idx = (int)Length();
	Insert(NewStr(Str));
	if (d->Popup && d->Popup->Lst)
	{
		LListItem *li = new LListItem;
		if (li)
		{
			li->SetText(Str);
			d->Popup->Lst->Insert(li);
		}
	}
	return Idx;
}
void LHistory::Update()
{
	LList *Lst = d->Popup ? d->Popup->Lst : 0;
	if (Lst)
	{
		Lst->Empty();
		for (auto s: *this)
		{
			LListItem *i = new LListItem;
			if (i)
			{
				i->SetText(s);
				Lst->Insert(i);
			}
		}
	}
}
void LHistory::OnPopupClose()
{
	if (!d->Popup->GetCancelled() &&
		d->TargetId &&
		d->Popup &&
		d->Popup->Str)
	{
		GetWindow()->SetCtrlName(d->TargetId, d->Popup->Str);
	}
}
////////////////////////////////////////////////////////////
// Factory
class LHistoryFactory : public LViewFactory
{
public:
	virtual LView *NewView
	(
		const char *Class,
		LRect *Pos,
		const char *Text
	)
	{
		if (stricmp(Class, "LHistory") == 0)
		{
			return new LHistory;
		}
		return 0;
	}
	
} HistoryFactory;