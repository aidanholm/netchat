#ifndef UTF8_H
#define UTF8_H

#include <wchar.h>
#include <assert.h>
#include <stdlib.h>
#include "debug.h"

extern int wcswidth (const wchar_t *__s, size_t __n) __THROW;

__attribute__((unused))
static int utf8_scrlen(const char *s) {
	assert(s);

	wchar_t wcstr[512];
	mbstowcs(wcstr, s, sizeof(wcstr)/sizeof(*wcstr));
	return wcswidth(wcstr, sizeof(wcstr)/sizeof(*wcstr));
}

__attribute__((unused))
static int utf8_char_bytewidth(const char *s) {
	assert(s);

	if((*s & 0x80) == 0x00) return 1;
	else if((*s & 0xe0) == 0xc0) return 2;
	else if((*s & 0xf0) == 0xe0) return 3;
	else if((*s & 0xf8) == 0xf0) return 4;
	else abort();
}

__attribute__((unused))
static const char *utf8_char_prev(const char *s) {
	assert(s);
		/* First two bits == 10 --> follow up byte */
	do s--; while((*s & 0xc0) == 0x80);

	return s;
}

#endif /* end of include guard: UTF8_H */
