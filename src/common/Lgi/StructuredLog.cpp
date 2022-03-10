#pragma once

#include "lgi/common/Lgi.h"
#include "lgi/common/StructuredLog.h"

LStructuredLog::LStructuredLog(const char *FileName)
{
	LFile::Path p(LSP_APP_INSTALL);
	p += FileName;
	f.Open(p, O_WRITE);
}

LStructuredLog::~LStructuredLog()
{
}
