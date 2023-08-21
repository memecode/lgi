#ifndef _GMDI_H_
#define _GMDI_H_

#include "lgi/common/Layout.h"

#define MDI_TAB_STYLE	1

class LMdiChild : public LLayout
{
	friend class LMdiParent;
	
	class LMdiChildPrivate *d;

public:
	LMdiChild();
	~LMdiChild();

	const char *GetClass() { return "LMdiChild"; }

	#if MDI_TAB_STYLE
	int GetOrder();
	#else
	void OnPaint(LSurface *pDC);	
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	LCursor GetCursor(int x, int y);
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

class LMdiParent : public LLayout
{
	friend class LMdiChild;
	class LMdiParentPrivate *d;

	#if MDI_TAB_STYLE
	int GetNextOrder();
	#endif
	LMdiChild *IsChild(LViewI *v);
	LArray<LMdiChild*> &PrivChildren();
	
	bool SetScrollBars(bool x, bool y);

public:
	LMdiParent();
	~LMdiParent();
	
	const char *GetClass() { return "LMdiParent"; }

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
	bool GetChildren(LArray<T*> &Views)
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
