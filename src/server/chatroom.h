#ifndef CHATROOM_H
#define CHATROOM_H

#include "server.h"
#include "client.h"
#include "vector.h"

typedef struct _chatroom_t {
	vector_t clients;
} chatroom_t;

chatroom_t *chatroom_new(server_t *server);
int chatroom_client_add(server_t *server, chatroom_t *chatroom, client_t *client);
void chatroom_start(server_t *server, chatroom_t *chatroom);

int chatroom_send_msg(const server_t *server, const chatroom_t *chatroom, const client_t *from, const char *msg);
int chatroom_send_file(const server_t *server, const chatroom_t *chatroom, uint16_t file_id);

int chatroom_client_is_present(server_t *server, chatroom_t *chatroom, client_t *client);
void chatroom_client_leave(server_t *server, chatroom_t *chatroom, client_t *client);

#endif
