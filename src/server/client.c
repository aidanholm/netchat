#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "server.h"
#include "chatroom.h"
#include "client.h"
#include "msg.h"
#include "debug.h"
#include "file.h"
#include "file_entry.h"
#include "macros.h"

static void client_gen_name(server_t *server, client_t *client, char name[256]) {
	assert(server);
	assert(client);
	assert(name);

	unsigned int client_id = sp_vector_indexof(&server->clients, client);
	snprintf(name, 256, "User_%u", client_id);
}

void client_connect(server_t *server, client_t *client, int fd) {
	assert(server);
	assert(client);
	assert(fd >= 0);

	const uint16_t client_id = sp_vector_indexof(&server->clients, client);

	memset(client, 0, sizeof(*client));

	client->fd = fd;
	client->file_fd = -1;
	client_gen_name(server, client, client->name);
	vector_init(&client->chatrooms, sizeof(uint32_t));

	log_info("Client <%s> has connected.", client->name);

	/* Inform the new client about its own id and name */
	msg_send(client->fd, MSG_user_update, client_id,
			(uint8_t)US_IDENTIFY, (uint8_t)strlen(client->name), client->name);

	client_t *other;
	sp_vector_foreach(&server->clients, other) {
		if(other == client) continue;
		uint16_t other_id = sp_vector_indexof(&server->clients, other);

		/* Inform the new client about all online users */
		msg_send(client->fd, MSG_user_update, other_id,
				(uint8_t)other->status, (uint8_t)strlen(other->name), other->name);

		/* Inform all online clients about the new user */
		msg_send(other->fd, MSG_user_update, client_id,
				(uint8_t)client->status, (uint8_t)strlen(client->name), client->name);
	}
}

void client_send_msg(const server_t *server, const chatroom_t *chatroom, const client_t *to, const client_t *from, const char *buf) {
	assert(server);
	assert(chatroom);
	assert(to);
	assert(buf);

	uint16_t chatroom_id = sp_vector_indexof(&server->chatrooms, chatroom),
			 sender_id = from ? sp_vector_indexof(&server->clients, from) : 0,
			 len = strlen(buf);
	msg_send(to->fd, MSG_msg, chatroom_id, sender_id, len, buf);
}

/* The server has just received a message from the client, and should now send it
 * out to all other clients in the chatroom */
int client_recv_msg(server_t *server, chatroom_t *chatroom, client_t *client, const char *buf) {
	assert(server);
	assert(chatroom);
	assert(client);
	assert(buf);
	assert(chatroom_client_is_present(server, chatroom, client));

	log_info("<%s> (%zu): %s", client->name,
			sp_vector_indexof(&server->clients, client), buf);

	return chatroom_send_msg(server, chatroom, client, buf);
}

void client_disconnect(server_t *server, client_t *client) {
	assert(server);
	assert(client);

	log_info("Client <%s> is disconnecting...", client->name);

	uint16_t *chatroom_id;

	vector_foreach(&client->chatrooms, chatroom_id) {
		chatroom_t *chatroom = sp_vector_get(&server->chatrooms, *chatroom_id);
		chatroom_client_leave(server, chatroom, client);
	}

	vector_free(&client->chatrooms);
	close(client->fd);

	log_info("Client <%s> has disconnected.", client->name);

	client_t *other;
	sp_vector_foreach(&server->clients, other) {
		if(other == client) continue;
		uint16_t client_id = sp_vector_indexof(&server->clients, client);

		/* Inform all online clients about the user leaving */
		msg_send(other->fd, MSG_user_update, client_id,
				(uint8_t)US_OFFLINE, (uint8_t)strlen(client->name), client->name);
	}
}

int client_start_file_send(client_t *client, char *fname, uint32_t fsize, uint16_t chat_id) {
	assert(client);
	assert(fname);
	assert(strlen(fname)<256);
	assert(chat_id);

	if(client->file_fd != -1) {
		/* TODO: what to do if there's already a transfer? */
		/* TODO: send a start transfer failed msg */
		return 1;
	}

	check((client->file_fd = file_create("", fsize)) != -1,
			"Couldn't create a temporary file for a file transfer.");
	strcpy(client->fname, fname);
	client->fsize = fsize;
	client->offset = 0;
	client->fchat = chat_id;

	return 0;
error:
	assert(client->file_fd == -1);
	return 1;
}

int client_start_file_recv(server_t *server, client_t *client, uint16_t chat_id, uint32_t file_id) {
	assert(server);
	assert(client);
	assert(chat_id);

	file_entry_t *file;

	check(client->file_fd == -1, "Client already transferring a file.");
	check(file_id <= server->files.largest_id, "Invalid file id");
	check(file = sp_vector_get(&server->files, file_id), "Invalid file id");
	check(file->chat_id == chat_id, "Client not permitted to receive this file.");

	assert(file->fd >= 0);
	assert(file->fsize > 0);
	assert(strlen(file->fname) > 0);

	client->sending = 1;
	client->file_fd = file->fd;
	client->fsize = file->fsize;
	client->offset = 0;
	strcpy(client->fname, file->fname);

	return 0;
error:
	return 1;
}

int client_recv_file_part(server_t *server, client_t *client, char *buf, uint16_t len) {
	assert(server);
	assert(client);

	check(client->file_fd != -1, "No file transfer currently ongoing for <%s>.", client->name);

	len = min(len, client->fsize - client->offset);

	assert(client->fsize - client->offset > 0);
	check(len, "Bad block length from client.");

	void *file = file_map(client->file_fd, PROT_WRITE, client->offset, len);
	memcpy(file, buf, len);
	client->offset += len;

	file_unmap(file, len);

	assert(client->offset <= client->fsize);
	if(client->offset == client->fsize) {
		unsigned file_entry_id;
		file_entry_t file_entry = {
			.fd = client->file_fd,
			.fsize = client->fsize,
			.chat_id = client->fchat,
			.sender = sp_vector_indexof(&server->clients, client)
		};
		strcpy(file_entry.fname, client->fname);

		check_quiet((file_entry_id = sp_vector_add(&server->files, &file_entry)));
		client->file_fd = -1;

		chatroom_t *chatroom = sp_vector_get(&server->chatrooms, client->fchat);

		chatroom_send_file(server, chatroom, file_entry_id);

		log_info("File upload finished: size = %u, chat = %hu",
				file_entry.fsize, file_entry.chat_id);
	}

	return 0;
error:
	return 1;
}

void client_send_file_part(client_t *client) {
	assert(client);

	if(!client->sending)
		return;
	assert(client->file_fd != -1);

	uint16_t len = min(4096, client->fsize - client->offset);
	char *buf = file_map(client->file_fd, PROT_READ, client->offset, len);
	msg_send(client->fd, MSG_file_part, len, buf);
	file_unmap(buf, len);

	client->offset += len;
	assert(client->offset <= client->fsize);

	debug("Sending... %u/%u", client->offset, client->fsize);

	if(client->offset == client->fsize) {
		debug("Done sending... %u/%u", client->offset, client->fsize);
		client->sending = 0;
		client->file_fd = -1;
	}
}
