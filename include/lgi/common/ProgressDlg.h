/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A progress window

#ifndef __GPROGRESSDLG_H
#define __GPROGRESSDLG_H

#include "lgi/common/TextLabel.h"
#include "lgi/common/TableLayout.h"

/// Progress window pane, tracks one task.
class GProgressDlg;
class LgiClass GProgressPane : public Progress, public GLayout
{
	friend class GProgressDlg;

	Progress *Ref;
	GProgressDlg *Dlg;

protected:
	GTableLayout *t;
	GTextLabel *Desc;
	GTextLabel *ValText;
	GTextLabel *Rate;
	GProgress *Bar;
	GButton *But;
	bool UiDirty;

public:
	GProgressPane(GProgressDlg *dlg);
	~GProgressPane();

	void SetDescription(const char *d) override;
	bool SetRange(const GRange &r) override;
	int64 Value() override { return Progress::Value(); }
	void Value(int64 v) override;
	GFont *GetFont() override;
	void UpdateUI();

	GProgressPane &operator++(int);
	GProgressPane &operator--(int);

	void OnCreate() override;
	int OnNotify(GViewI *Ctrl, int Flags) override;
	void OnPaint(GSurface *pDC) override;
	void OnPosChange() override;
};

/// Progress dialog
class LgiClass GProgressDlg : public LDialog, public Progress
{
	friend class GProgressPane;

protected:
	uint64 Ts, Timeout, YieldTs;
	GArray<GProgressPane*> Panes;
	bool CanCancel;

	void Resize();
	void TimeCheck();

public:
	/// Constructor
	GProgressDlg
	(
		/// The parent window
		GView *parent = NULL,
		/// Specify a timeout to become visible (in ms)
		uint64 timeout = 0
	);
	~GProgressDlg();

	/// Adds a pane. By default there is one pane.
	GProgressPane *Push();
	/// Pops off a pane
	void Pop(GProgressPane *p = 0);
	/// Gets the pane at index 'i'
	GProgressPane *ItemAt(int i);

	/// Sets up the Value function to yield every so often
	/// to update the screen
	void SetYieldTime(uint64 yt) { SetPulse((int) (YieldTs = yt)); }
	/// Set ability to cancel
	void SetCanCancel(bool cc);

	/// Returns the description of the first pane
	GString GetDescription() override;
	/// Sets the description of the first pane
	void SetDescription(const char *d) override;
	/// Returns the upper and lower limits of the first pane
	GRange GetRange() override;
	/// Sets the upper and lower limits of the first pane
	bool SetRange(const GRange &r) override;
	/// Returns the scaling factor of the first pane
	double GetScale() override;
	/// Sets the scaling factor of the first pane
	void SetScale(double s) override;
	/// Returns the current value of the first pane
	int64 Value() override;
	/// Sets the current value of the first pane
	void Value(int64 v) override;
	/// Returns the type description of the first pane
	GString GetType() override;
	/// Sets the type description of the first pane
	void SetType(const char *t) override;
	/// Returns whether the user has cancelled the operation
	bool IsCancelled() override;
	
	GProgressDlg &operator++(int);
	GProgressDlg &operator--(int);

	int OnNotify(GViewI *Ctrl, int Flags) override;
	GMessage::Result OnEvent(GMessage *Msg) override;
	void OnPaint(GSurface *pDC) override;
	void OnCreate() override;
	void OnPosChange() override;
	bool OnRequestClose(bool OsClose) override;
	void OnPulse() override;
};

#endif
