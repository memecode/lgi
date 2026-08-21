#pragma once

bool Is8Bit(const char *Text);

LString LDecodeBase64Str(LString Str);
LString LDecodeQuotedPrintable(LString Str);
LString LEncodeQuotedPrintable(LString Str, int MaxLine = 76, int PreCount = 0);
LString LDecodeRfc2047(LString Str);
LString LEncodeRfc2047(LString Input, const char *InCharset, LString::Array *OutCharsets = NULL, ssize_t LineLength = 0);
