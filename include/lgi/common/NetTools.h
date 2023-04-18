/// \file
/// \author Matthew Allen
/// \brief General socket / mail-news related classes and functions

#pragma once

#include "lgi/common/LgiNetInc.h"
#include "lgi/common/StringClass.h"

/// Get a specific value from a list of headers (as a dynamic string)
LgiNetFunc char *InetGetHeaderField(const char *Headers, const char *Field, ssize_t Len = -1);
LgiNetFunc LString LGetHeaderField(LString Headers, const char *Field);

/// Gets a sub field of a header value
LgiNetFunc char *InetGetSubField(const char *s, const char *Field);
LgiNetFunc LString LGetSubField(LString HeaderValue, const char *Field);
