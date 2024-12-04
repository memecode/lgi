#pragma once

// Use RemoveAnsi instead...
[[deprecated]] extern char *SkipEscape(char *c);
[[deprecated]] extern void DeEscape(LString &s);
[[deprecated]] extern void DeEscape(char *s, ssize_t *bytes);

