
#ifndef __FILE_TRANS_PROG_H
#define __FILE_TRANS_PROG_H

#include "lgi/common/ProgressStatusPane.h"

// Status bar panes
#define	_STATUS_THROTTLE			0
#define	_STATUS_HISTORY				1
#define	_STATUS_INFO				2
#define _STATUS_POSITION			3	
#define _STATUS_PROGRESS			4
#define _STATUS_RATE				5
#define	_STATUS_TIME_LEFT			6
#define	_STATUS_MAX					7

// Options
#define OPT_Throttle				"Throttle"	// (int)
#define OPT_PipeSize				"PipeSize"	// (int)

// Classes
class FileTransferProgress : public LStatusPane, public Progress
{
	uint64 StartTime;
	int64 StartPos;
	LProgressStatusPane *ProgressPane;
	LArray<LStatusPane*> StatusInfo;
	
	int64 DspVal;
	void UpdateUi();

public:
	FileTransferProgress(LDom *App, LStatusBar *Status, bool Limit = false);

	// Progress Api (must be thread-safe)
	bool SetRange(const LRange &r) override;
	void Value(int64 v) override;

	// Parameters
	constexpr static const char *sStartValue = "StartValue";
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;

	// Impl
	LMessage::Result OnEvent(LMessage *m) override;
	void OnCreate() override;
	void OnPulse() override;
};

#endif
