#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdlib.h>
#include <stdint.h>
#include "chat.h"

typedef struct _transfer_t {
	uint16_t chat_id, file_id, sender_id;
	const char *fname;
	int fd;
	uint32_t fsize, offset;
	int sending;
} transfer_t;

int transfer_init(transfer_t *transfer);
void transfer_free(transfer_t *transfer);

const char* transfer_begin_upload(transfer_t *transfer, const chat_t *chat, const char *name);
const char* transfer_begin_download(transfer_t *transfer, const char *name);

#endif /* end of include guard: TRANSFER_H */
