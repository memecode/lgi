#pragma once

extern char *SkipEscape(char *c);
extern void DeEscape(GString &s);
extern void DeEscape(char *s, ssize_t *bytes);

