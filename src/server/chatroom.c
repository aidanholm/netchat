#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <alloca.h>

#include "server.h"
#include "chatroom.h"
#include "client.h"
#include "msg.h"
#include "file_entry.h"
#include "debug.h"

chatroom_t *chatroom_new(server_t *server) {
	chatroom_t new_chatroom;
	unsigned int id;

	vector_init(&new_chatroom.clients, sizeof(uint16_t));
	id = sp_vector_add(&server->chatrooms, &new_chatroom);
	return id ? sp_vector_get(&server->chatrooms, id) : NULL;
}

int chatroom_client_add(server_t *server, chatroom_t *chatroom, client_t *client) {
	assert(server);
	assert(chatroom);
	assert(client);
	assert(mutex_locked(&server->clients_mutex));

	uint16_t client_id = sp_vector_indexof(&server->clients, client),
			 chatroom_id = sp_vector_indexof(&server->chatrooms, chatroom);

	check_quiet(vector_add(&chatroom->clients, &client_id, 1));
	check_quiet(vector_add(&client->chatrooms, &chatroom_id, 1));

	log_info("<%s> has joined chatroom %u", client->name, chatroom_id);

	return 0;
error:
	return 1;
}

void chatroom_start(server_t *server, chatroom_t *chatroom) {
	assert(server);
	assert(chatroom);
	assert(chatroom->clients.size > 0);

	const uint16_t chat_id = sp_vector_indexof(&server->chatrooms, chatroom);
	const unsigned short num_clients = chatroom->clients.size;
	uint16_t client_ids[chatroom->clients.size], *client_id;
	unsigned int zzi = 0;

	vector_foreach(&chatroom->clients, client_id)
		client_ids[zzi++] = *client_id;

	vector_foreach(&chatroom->clients, client_id) {
		client_t *client = sp_vector_get(&server->clients, *client_id);
		log_info("Sending start_chat message to <%s>", client->name);
		msg_send(client->fd, MSG_start_chat, chat_id, num_clients, client_ids);
	}
}

int chatroom_send_msg(const server_t *server, const chatroom_t *chatroom, const client_t *from, const char *msg) {
	assert(server);
	assert(chatroom);
	assert(mutex_locked(&server->clients_mutex));

	uint16_t *c_id;

	vector_foreach(&chatroom->clients, c_id) {
		client_t *client = sp_vector_get(&server->clients, *c_id);
		client_send_msg(server, chatroom, client, from, msg);
	}

	return 0;
}

int chatroom_send_file(const server_t *server, const chatroom_t *chatroom, uint16_t file_id) {
	assert(server);
	assert(chatroom);
	assert(file_id);
	assert(mutex_locked(&server->clients_mutex));

	uint16_t *c_id,
			 chat_id = sp_vector_indexof(&server->chatrooms, chatroom);
	file_entry_t *file = sp_vector_get(&server->files, file_id);

	vector_foreach(&chatroom->clients, c_id) {
		debug("Notifying %hu about file from %hu", *c_id, file->sender);
		if(*c_id == file->sender) continue;
		client_t *client = sp_vector_get(&server->clients, *c_id);
		msg_send(client->fd, MSG_send_file, chat_id, file->fsize,
				(uint16_t)strlen(file->fname), file->fname, file_id, file->sender);
	}

	return 0;
}

int chatroom_client_is_present(server_t *server, chatroom_t *chatroom, client_t *client) {
	assert(server);
	assert(chatroom);
	assert(client);
	assert(mutex_locked(&server->clients_mutex));

	uint16_t client_id = sp_vector_indexof(&server->clients, client), *c_id;

	vector_foreach(&chatroom->clients, c_id) {
		if(*c_id == client_id)
			return 1;
	}

	return 0;
}

void chatroom_client_leave(server_t *server, chatroom_t *chatroom, client_t *client) {
	assert(server);
	assert(chatroom);
	assert(client);
	assert(mutex_locked(&server->clients_mutex));

	if(!chatroom_client_is_present(server, chatroom, client))
		return;

	/* TODO: chatroom vs chat */

	const uint16_t client_id = sp_vector_indexof(&server->clients, client),
			 chat_id = sp_vector_indexof(&server->chatrooms, chatroom);
	uint16_t *c_id;

	/* Remove that client from the list of clients in the room */
	vector_foreach(&chatroom->clients, c_id) {
		if(*c_id == client_id) {
			vector_del(&chatroom->clients, vector_indexof(&chatroom->clients, c_id));
			break;
		}
	}

	/* ...and send it to each client in the chatroom */
	vector_foreach(&chatroom->clients, c_id) {
		client_t *other = sp_vector_get(&server->clients, *c_id);
		msg_send(other->fd, MSG_leave_chat, chat_id, client_id);
	}

	log_info("Client <%s> has left.", client->name);
}
