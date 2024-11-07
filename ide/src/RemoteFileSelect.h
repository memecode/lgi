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
							LString initialPath,
							std::function<void(LString)> callback);