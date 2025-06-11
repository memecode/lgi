#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////
// Simple missing capabilities bar
//
// Use in conjuction with LCapabilityTarget, and more specifically it's NeedsCapability callback.
// Add actions as needed to resolve the missing capability.
///////////////////////////////////////////////////////////////////////////////////////////////

#include "lgi/common/Button.h"

class LMissingCapsBar : public LView
{
public:
	using TCallback = std::function<void()>;

private:
	int nextId = 100;
	struct Action
	{
		LString name;
		int id;
		TCallback cb;
		LButton *btn;
	};
	LArray<Action> actions;

public:
	LMissingCapsBar()
	{
		Name("Not set...");
		Visible(false);
	}

	// Set the name of the missing capability and the message
	void Set(const char *CapName, const char *Message)
	{
		Name(LString::Fmt("Missing '%s': %s", CapName, Message));
		Visible(true);
	}

	// Add an action to resolve the issue:
	void Action(const char *btnName, std::function<void()> callback)
	{
		auto &a = actions.New();
		a.name = btnName;
		a.id = nextId++;
		a.cb = std::move(callback);
		AddView(a.btn = new LButton(a.id, 0, 0, -1, -1, btnName));
	}

	void OnPosChange() override
	{
		auto client = GetClient();
		auto space = LTableLayout::CellSpacing;
		int btnWid = 0;
		for (auto &a: actions)
			btnWid += a.btn->X();
		int x = client.X() - (actions.Length() * space) - btnWid;
		for (auto &a: actions)
		{
			LRect r(x, 1, x + a.btn->X() - 1, client.Y() - 2);
			a.btn->SetPos(r);
			x += a.btn->X() + space;
		}
		AttachChildren();
	}

	int OnNotify(LViewI *Ctrl, const LNotification &n) override
	{
		for (auto &a: actions)
		{
			if (a.id == Ctrl->GetId() &&
				n.Type == LNotifyItemClick)
			{
				a.cb();
			}
		}

		return 0;
	}

	bool OnLayout(LViewLayoutInfo &Inf) override
	{
		if (Inf.Width.Min)
		{
			// Height
			if (Visible())
			{
				auto yPx = (int) (LSysFont->GetHeight() * 1.8);
				Inf.Height.Min = yPx;
				Inf.Height.Max = yPx;
			}
			else
			{
				Inf.Height.Min = 0;
				Inf.Height.Max = 0;
			}
		}
		else
		{
			// Width
			Inf.Width.Min = LViewLayoutInfo::FILL;
			Inf.Width.Max = LViewLayoutInfo::FILL;
		}

		return true;
	}

	void OnPaint(LSurface *pDC) override
	{
		auto client = GetClient();

		pDC->Colour(LColour(200, 0, 0));
		pDC->Rectangle();
	
		if (auto n = Name())
		{
			if (auto fnt = GetFont())
			{
				fnt->Colour(LColour::White, LColour::Red);
				fnt->Transparent(true);
				LDisplayString ds(fnt, n);
				ds.Draw(pDC, LTableLayout::CellSpacing, (client.Y()-ds.Y())/2);
			}
		}
	}
};

