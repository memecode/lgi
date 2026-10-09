#pragma once

#include "Debugger.h"

extern LDebugger *CreateGdbDebugger(BreakPointStore *bpStore,
									LStream *Log,
									class SystemIntf *Backend,
									SysPlatform platform,
									LStream *networkLog);
