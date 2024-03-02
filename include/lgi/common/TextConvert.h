#pragma once

bool Is8Bit(const char *Text);

[[deprecated]] LgiFunc char *DecodeBase64Str(char *Str, ssize_t Len = -1);
LgiExtern LString LDecodeBase64Str(LString Str);

[[deprecated]] LgiFunc char *DecodeQuotedPrintableStr(char *Str, ssize_t Len = -1);
LgiExtern LString LDecodeQuotedPrintableStr(LString Str);

[[deprecated]] LgiFunc char *DecodeRfc2047(char *Str);
LgiExtern LString LDecodeRfc2047(LString Str);

[[deprecated]] LgiFunc char *EncodeRfc2047(char *Input, const char *InCharset, LString::Array *OutCharsets = NULL, ssize_t LineLength = 0);
LgiExtern LString LEncodeRfc2047(LString Input, const char *InCharset, LString::Array *OutCharsets = NULL, ssize_t LineLength = 0);
