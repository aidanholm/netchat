#ifndef CLIENT_H
#define CLIENT_H

#include <jansson.h>
#include <pthread.h>
#include "client_ui.h"
#include "vector.h"
#include "macros.h"
#include "status.h"

typedef struct _client_t {
	client_ui_t ui;
	int socket;
	int running;
	pthread_t net_thread;
	vector_t users;
	pthread_mutex_t users_mutex;
	vector_t chats;
	vector_t names;

	unsigned int name_index;
	user_status_t status;
	uint16_t id;

	/* File transfer */
	int file_fd;
	uint32_t fsize, offset;
	int sending;
	chat_t *fchat;
	size_t fchat_file_i;
} client_t;

int client_upload_file(client_t *client, chat_t *chat, const char *fname);
int client_download_file(client_t *client, chat_file_t *file, const char *name);
int client_send_file_part(client_t *client);
int client_disconnect(client_t *client);
int client_set_status(client_t *client, user_status_t status);
int client_send_name(client_t *client, const char *name);
int client_set_name(client_t *client, const char name[256]);
chat_t *client_current_chat(client_t *client);

#endif
