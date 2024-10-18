#pragma once

#include "ProjectBackend.h"

extern void RemoteFileSelect(LViewI *parent, ProjectBackend *backend, std::function<void(LString)> callback);