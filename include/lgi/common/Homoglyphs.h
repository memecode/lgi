#pragma once

extern bool HasHomoglyphs(const char *utf8, ssize_t len = -1);
extern bool RemoveZeroWidthCharacters(char *utf8, ssize_t len = -1);
