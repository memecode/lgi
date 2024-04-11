#pragma once

bool Is8Bit(const char *Text);

[[deprecated]] char *DecodeBase64Str(char *Str, ssize_t Len = -1);
LString LDecodeBase64Str(LString Str);

[[deprecated]] char *DecodeQuotedPrintableStr(char *Str, ssize_t Len = -1);
LString LDecodeQuotedPrintable(LString Str);
LString LEncodeQuotedPrintable(LString Str, int MaxLine = 76, int PreCount = 0);

[[deprecated]] char *DecodeRfc2047(char *Str);
LString LDecodeRfc2047(LString Str);

[[deprecated]] char *EncodeRfc2047(char *Input, const char *InCharset, LString::Array *OutCharsets = NULL, ssize_t LineLength = 0);
LString LEncodeRfc2047(LString Input, const char *InCharset, LString::Array *OutCharsets = NULL, ssize_t LineLength = 0);
