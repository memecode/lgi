#ifndef _GMDI_H_
#define _GMDI_H_

#include "lgi\common\Layout.h"

#define MDI_TAB_STYLE	1

class GMdiChild : public LLayout
{
	friend class GMdiParent;
	
	class GMdiChildPrivate *d;

public:
	GMdiChild();
	~GMdiChild();

	const char *GetClass() { return "GMdiChild"; }

	#if MDI_TAB_STYLE
	int GetOrder();
	#else
	void OnPaint(LSurface *pDC);	
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	LgiCursor GetCursor(int x, int y);
	LRect &GetClient(bool InClientSpace = true);
	#endif

	bool Attach(LViewI *p);
	bool Detach();

	bool PourAll();
	const char *Name();
	bool Name(const char *n);
	
	virtual void Raise();
	virtual void Lower();
	virtual void OnTitleClick(LMouse &m);
	virtual void OnButtonClick(LMouse &m);
	virtual void OnPaintButton(LSurface *pDC, LRect &rc);
};

class GMdiParent : public LLayout
{
	friend class GMdiChild;
	class GMdiParentPrivate *d;

	#if MDI_TAB_STYLE
	int GetNextOrder();
	#endif
	GMdiChild *IsChild(LViewI *v);
	::GArray<GMdiChild*> &PrivChildren();

public:
	GMdiParent();
	~GMdiParent();
	
	const char *GetClass() { return "GMdiParent"; }

	bool HasButton();
	void HasButton(bool b);

	void OnPaint(LSurface *pDC);
	bool Attach(LViewI *p);
	bool OnViewMouse(LView *View, LMouse &m);
	bool OnViewKey(LView *View, LKey &Key);
	void OnChildrenChanged(LViewI *Wnd, bool Attaching);
	LRect NewPos();
	LViewI *GetTop();

	template<typename T>
	bool GetChildren(::GArray<T*> &Views)
	{
		for (auto c : PrivChildren())
		{
			T *t = dynamic_cast<T*>(c);
			if (t)
				Views.Add(t);
		}
		#if MDI_TAB_STYLE
		Views.Sort
		(
			[]
			(T **a, T **b)
			{
				return (*a)->GetOrder() - (*b)->GetOrder();
			}
		);
		#endif
		return Views.Length() > 0;
	}

	
	#if MDI_TAB_STYLE
	void OnPosChange();
	void OnMouseClick(LMouse &m);
	bool Detach();
	#endif
};

#endif
