#ifndef CLIENT_CHAT_H
#define CLIENT_CHAT_H

#include <stdint.h>
#include "vector.h"

struct _client_t;

typedef enum {
	CR_FILE,
	CR_MSG
} chat_row_type_t;

typedef struct _chat_file_t {
	size_t id; /* transfer_t id */
} chat_file_t;

typedef struct _chat_msg_t {
	size_t start;
	uint16_t len, num_lines;
} chat_msg_t;

typedef struct _chat_row_t {
	chat_row_type_t type;
	union {
		chat_msg_t msg;
		chat_file_t file;
	};
	unsigned int name_index;
	unsigned int from;
} chat_row_t;

typedef struct _chat_t {
	vector_t chars;
	vector_t rows;
	vector_t users;
	uint16_t id;
	uint16_t top, cursor, morebelow;
	uint8_t max_namelen;
	uint8_t unread;
} chat_t;

int chat_add(struct _client_t *client, uint16_t chat_id, uint16_t *client_ids, uint16_t num_ids);
void chat_del(struct _client_t *client, chat_t *chat);
chat_t *chat_get(struct _client_t *client, uint16_t id);

int chat_add_msg(struct _client_t *client, chat_t *chat, const char *msg, uint16_t from);
int chat_add_file(struct _client_t *client, chat_t *chat, size_t index, uint16_t from_id);

int chat_send_msg(struct _client_t *client, chat_t *chat, const char *msg);
int chat_send_file(struct _client_t *client, chat_t *chat, const char *msg);
const char *chat_row_name(struct _client_t *client, chat_row_t row);

void chat_leave(struct _client_t *client, chat_t *chat, uint16_t other_id);

#endif
