#pragma once

bool Is8Bit(const char *Text);

[[deprecated]] char *DecodeBase64Str(char *Str, ssize_t Len = -1);
LString LDecodeBase64Str(LString Str);

[[deprecated]] char *DecodeQuotedPrintableStr(char *Str, ssize_t Len = -1);
LString LDecodeQuotedPrintableStr(LString Str);

[[deprecated]] char *DecodeRfc2047(char *Str);
LString LDecodeRfc2047(LString Str);

[[deprecated]] char *EncodeRfc2047(char *Str, const char *Charset, List<char> *CharsetPrefs, ssize_t LineLength = 0);
LString LEncodeRfc2047(LString Str, const char *Charset, List<char> *CharsetPrefs, ssize_t LineLength = 0);
