#pragma once

#include "ProjectBackend.h"

enum FileSelectType
{
	SelectOpen,
	SelectOpenFolder,
	SelectSave,
};

extern void RemoteFileSelect(LViewI *parent,
							ProjectBackend *backend,
							FileSelectType type,
							std::function<void(LString)> callback);