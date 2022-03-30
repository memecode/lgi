#include "lgi/common/Lgi.h"
#include "Lvc.h"

struct DropDownBtnPriv
{
	DropDownBtn *View;
	int EditId;

	DropDownBtnPriv(DropDownBtn *view)
	{
		View = view;
		EditId = -1;
	}

	LViewI *GetEdit()
	{
		LViewI *e = NULL;
		if (View && View->GetParent())
			View->GetParent()->GetViewById(EditId, e);
		LAssert(e != 0);
		return e;
	}
};

class DropLst : public LPopup
{
public:
	DropDownBtnPriv *d;
	LList *Lst;

	DropLst(DropDownBtnPriv *priv, LView *owner) : Lst(NULL), LPopup(owner)
	{
		d = priv;

		int x = 300;
		LRect r(0, 0, x, 200);
		SetPos(r);
		AddView(Lst = new LList(10));
		Lst->Sunken(false);
		Lst->ShowColumnHeader(false);
		Lst->AddColumn("", x);
	}

	void OnPosChange()
	{
		LRect c = GetClient();
		c.Inset(1, 1);
		if (Lst)
			Lst->SetPos(c);
	}

	void OnPaint(LSurface *pDC)
	{
		pDC->Colour(LColour::Black);
		pDC->Box();
	}

	int OnNotify(LViewI *c, LNotification n)
	{
		if (c->GetId() == 10 &&
			n.Type == LNotifyItemClick)
		{
			LListItem *i = Lst->GetSelected();
			LViewI *e = d->GetEdit();
			if (e && i)
			{
				e->Name(i->GetText(0));
			}

			Visible(false);
		}

		return 0;
	}
};

DropDownBtn::DropDownBtn() :
	LDropDown(-1, 0, 0, 100, 24, NULL),
	ResObject(Res_Custom)
{
	d = new DropDownBtnPriv(this);
	d->EditId = -1;
	SetPopup(Pu = new DropLst(d, this));
}

DropDownBtn::~DropDownBtn()
{
	DeleteObj(d);
}

LString::Array DropDownBtn::GetList()
{
	LString::Array a;
	if (Pu && Pu->Lst)
	{
		for (auto i: *Pu->Lst)
		{
			a.New() = i->GetText(0);
		}
	}
	return a;
}

bool DropDownBtn::OnLayout(LViewLayoutInfo &Inf)
{
	if (Inf.Width.Min)
	{
		// Vertical layout
		Inf.Height.Min = Inf.Height.Max = 23;
	}
	else
	{
		// Horizontal layout
		Inf.Width.Min = Inf.Width.Max = 23;
	}
	return true;
}

bool DropDownBtn::SetList(int EditCtrl, LString::Array a)
{
	if (!Pu || !Pu->Lst)
		return false;
	Pu->Lst->Empty();
	d->EditId = EditCtrl;
	for (auto s: a)
	{
		LListItem *i = new LListItem;
		i->SetText(s);
		Pu->Lst->Insert(i);
	}

	return true;
}

class DropDownBtnFactory : public LViewFactory
{
	LView *NewView(const char *Class, LRect *Pos, const char *Text)
	{
		if (!stricmp(Class, "DropDownBtn"))
			return new DropDownBtn;
		return NULL;
	}

}	DropDownBtnFactoryInst;