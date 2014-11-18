#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "server.h"
#include "vector.h"
#include "status.h"

struct _chatroom_t;

typedef struct _client_t {
	int fd;
	char name[256];
	user_status_t status;
	vector_t chatrooms;

	/* File transfer information */
	char fname[256];
	uint32_t fsize, offset;
	int file_fd;
	int sending;
	uint16_t fchat;
} client_t;

void client_send_msg(const server_t *server, const struct _chatroom_t *chatroom, const client_t *to, const client_t *from, const char *buf);
int client_recv_msg(server_t *server, struct _chatroom_t *chatroom, client_t *client, const char *buf);

void client_connect(server_t *server, client_t *client, int fd);
void client_disconnect(server_t *server, client_t *client);

int client_start_file_send(client_t *client, char *fname, uint32_t fsize, uint16_t chat_id);
int client_start_file_recv(server_t *server, client_t *client, uint16_t chat_id, uint32_t file_id);
int client_recv_file_part(server_t *server, client_t *client, char *buf, uint16_t len);
void client_send_file_part(client_t *client);

#endif
