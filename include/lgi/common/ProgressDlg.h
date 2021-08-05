/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A progress window

#ifndef __LProgressDlg_H
#define __LProgressDlg_H

#include "lgi/common/TextLabel.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/ProgressView.h"
#include "lgi/common/Button.h"

/// Progress window pane, tracks one task.
class LProgressDlg;
class LgiClass LProgressPane : public Progress, public LLayout
{
	friend class LProgressDlg;

	Progress *Ref;
	LProgressDlg *Dlg;

protected:
	LTableLayout *t;
	LTextLabel *Desc;
	LTextLabel *ValText;
	LTextLabel *Rate;
	LProgressView *Bar;
	LButton *But;
	bool UiDirty;

public:
	LProgressPane(LProgressDlg *dlg);
	~LProgressPane();

	void SetDescription(const char *d) override;
	bool SetRange(const LRange &r) override;
	int64 Value() override { return Progress::Value(); }
	void Value(int64 v) override;
	LFont *GetFont() override;
	void UpdateUI();

	LProgressPane &operator++(int);
	LProgressPane &operator--(int);

	void OnCreate() override;
	int OnNotify(LViewI *Ctrl, LNotification &n) override;
	void OnPaint(LSurface *pDC) override;
	void OnPosChange() override;
};

/// Progress dialog
class LgiClass LProgressDlg : public LDialog, public Progress
{
	friend class LProgressPane;

protected:
	uint64 Ts, Timeout, YieldTs;
	LArray<LProgressPane*> Panes;
	bool CanCancel;

	void Resize();
	void TimeCheck();

public:
	/// Constructor
	LProgressDlg
	(
		/// The parent window
		LView *parent = NULL,
		/// Specify a timeout to become visible (in ms)
		uint64 timeout = 0
	);
	~LProgressDlg();

	/// Adds a pane. By default there is one pane.
	LProgressPane *Push();
	/// Pops off a pane
	void Pop(LProgressPane *p = 0);
	/// Gets the pane at index 'i'
	LProgressPane *ItemAt(int i);

	/// Sets up the Value function to yield every so often
	/// to update the screen
	void SetYieldTime(uint64 yt) { SetPulse((int) (YieldTs = yt)); }
	/// Set ability to cancel
	void SetCanCancel(bool cc);

	/// Returns the description of the first pane
	LString GetDescription() override;
	/// Sets the description of the first pane
	void SetDescription(const char *d) override;
	/// Returns the upper and lower limits of the first pane
	LRange GetRange() override;
	/// Sets the upper and lower limits of the first pane
	bool SetRange(const LRange &r) override;
	/// Returns the scaling factor of the first pane
	double GetScale() override;
	/// Sets the scaling factor of the first pane
	void SetScale(double s) override;
	/// Returns the current value of the first pane
	int64 Value() override;
	/// Sets the current value of the first pane
	void Value(int64 v) override;
	/// Returns the type description of the first pane
	LString GetType() override;
	/// Sets the type description of the first pane
	void SetType(const char *t) override;
	/// Returns whether the user has cancelled the operation
	bool IsCancelled() override;
	
	LProgressDlg &operator++(int);
	LProgressDlg &operator--(int);

	int OnNotify(LViewI *Ctrl, LNotification &n) override;
	LMessage::Result OnEvent(LMessage *Msg) override;
	void OnPaint(LSurface *pDC) override;
	void OnCreate() override;
	void OnPosChange() override;
	bool OnRequestClose(bool OsClose) override;
	void OnPulse() override;
};

#endif
