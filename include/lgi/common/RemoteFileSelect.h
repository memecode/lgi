#pragma once

#include "lgi/common/SystemIntf.h"

enum FileSelectType
{
	SelectOpen,
	SelectOpenFolder,
	SelectSave,
};

extern void RemoteFileSelect(LViewI *parent,
							SystemIntf *systemIntf,
							FileSelectType type,
							LString initialPath,
							std::function<void(LString)> callback);