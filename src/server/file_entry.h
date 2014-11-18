#ifndef FILE_ENTRY_H
#define FILE_ENTRY_H

#include <stdint.h>

typedef struct _file_entry_t {
	int fd;
	char fname[256];
	uint32_t fsize;
	uint16_t chat_id, sender;
} file_entry_t;

#endif
