#pragma once

extern bool Is8Bit(char *Text);
extern char *DecodeBase64Str(char *Str, ssize_t Len = -1);
extern char *DecodeQuotedPrintableStr(char *Str, ssize_t Len = -1);
extern char *DecodeRfc2047(char *Str);
extern char *EncodeRfc2047(char *Str, const char *CodePage, List<char> *CharsetPrefs, ssize_t LineLength = 0);
