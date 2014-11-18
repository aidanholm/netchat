#include "msg_handlers.h"
#include "server.h"
#include "client.h"
#include "chatroom.h"
#include "debug.h"
#include "utilities/file.h"

char *buf = NULL;

int msg_handle_start_chat(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t num_ids, *client_ids, unused;
	client_t **clients;

	check_quiet(!msg_recv(client->fd, MSG_start_chat, &unused, &num_ids, &buf));

	client_ids = (uint16_t *)&buf[0];
	clients = alloca(sizeof(*clients)*num_ids);

	/* Ensure all ids are valid */
	for(unsigned int i=0; i<num_ids; i++) {
		/* FIXME: move to sp_vector */
		check_quiet(client_ids[i] <= server->clients.largest_id);
		check_quiet(clients[i] = sp_vector_get(&server->clients, client_ids[i]));
	}

	/* Create the new chatroom */
	chatroom_t *chatroom;
	check_quiet(chatroom = chatroom_new(server));
	chatroom_client_add(server, chatroom, client);
	for(unsigned int i=0; i<num_ids; i++)
		chatroom_client_add(server, chatroom, clients[i]);

	chatroom_start(server, chatroom);

	return 0;
error:
	return 1;
}

int msg_handle_leave_chat(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t chat_id, unused;
	chatroom_t *chatroom;

	check_quiet(!msg_recv(client->fd, MSG_leave_chat, &chat_id, &unused));
	check_quiet(chat_id <= server->chatrooms.largest_id);;
	check_quiet(chatroom = sp_vector_get(&server->chatrooms, chat_id));
	chatroom_client_leave(server, chatroom, client);

	return 0;
error:
	return 1;
}

int msg_handle_msg(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t chat_id, len, unused;
	chatroom_t *chatroom;

	check_quiet(!msg_recv(client->fd, MSG_msg, &chat_id, &unused, &len, &buf));
	check_quiet(chatroom = sp_vector_get(&server->chatrooms, chat_id));
	check_quiet(chatroom_client_is_present(server, chatroom, client));
	assert(len < sizeof(buf)); /* FIXME */
	buf[len] = 0;
	client_recv_msg(server, chatroom, client, buf);

	return 0;
error:
	return 1;
}

int msg_handle_send_file(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t chat_id, len, unused;
	uint32_t fsize;
	check_quiet(!msg_recv(client->fd,
				MSG_send_file, &chat_id, &fsize, &len, &buf, &unused, &unused));
	assert(len < sizeof(buf)); /* FIXME */
	buf[len] = 0;

	log_info("Beginning to receive file '%s' from client <%s>: size %u",
			buf, client->name, fsize);

	/* FIXME: assumes buf is null-terminated, is it? */
	check(len < 256, "Filename too long.");
	check_quiet(!client_start_file_send(client, buf, fsize, chat_id));

	return 0;
error:
	return 1;
}

int msg_handle_recv_file(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t chat_id;
	uint32_t file_id;

	log_info("Beginning to send file to client <%s>, %d", client->name, client->file_fd);

	check_quiet(!msg_recv(client->fd, MSG_recv_file, &chat_id, &file_id));
	check_quiet(client_start_file_recv(server, client, chat_id, file_id));

	return 0;
error:
	return 1;
}

int msg_handle_file_part(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	uint16_t len;

	check_quiet(!msg_recv(client->fd, MSG_file_part, &len, &buf));
	check_quiet(!client_recv_file_part(server, client, buf, len));

	return 0;
error:
	return 1;
}

int msg_handle_user_update(struct _server_t *server, struct _client_t *client) {
	assert(server);
	assert(client);

	uint16_t unused;
	uint8_t status, len;
	client_t *other;

	check_quiet(!msg_recv(client->fd, MSG_user_update, &unused, &status, &len, &buf));
	buf[len] = 0;

	/* Check other clients' names */
	sp_vector_foreach(&server->clients, other) {
		if(other == client) continue;
		if(!strcmp(buf, other->name)) {
			goto skip_set_name;
		}
	}

	/* Set name */
	strncpy(client->name, buf, sizeof(client->name));

skip_set_name:

	/* Set status */
	if(status < US_NUM_STATUSES)
		client->status = status;

	/* Distribute notification message: also tell the client */
	sp_vector_foreach(&server->clients, other) {
		msg_send(other->fd, MSG_user_update,
				(uint16_t)sp_vector_indexof(&server->clients, client),
				status, (uint8_t)strlen(client->name), client->name);
	}

	return 0;
error:
	return 1;
}
