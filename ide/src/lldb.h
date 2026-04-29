#pragma once

#include "Debugger.h"

extern LDebugger *CreateLldbDebugger(BreakPointStore *bpStore,
									LStream *Log,
									class SystemIntf *Backend,
									SysPlatform platform,
									LStream *networkLog);
