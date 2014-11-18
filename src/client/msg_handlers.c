#include <signal.h>
#include <unistd.h>

#include "msg_handlers.h"
#include "client.h"
#include "debug.h"
#include "user.h"
#include "transfer.h"
#include "file.h"

char *buf = NULL;

int msg_handle_start_chat(client_t *client) {
	assert(client);

	uint16_t chat_id, num_ids, *client_ids;

	check_quiet(!msg_recv(client->socket, MSG_start_chat, &chat_id, &num_ids, &buf));
	client_ids = (uint16_t*)buf;

	check(num_ids > 0, "Can't start a chat with no participants.");

	check(!chat_add(client, chat_id, client_ids, num_ids),
			"Couldn't start a chat.");

	return 0;
error:
	return 1;
}

int msg_handle_leave_chat(client_t *client) {
	assert(client);

	uint16_t chat_id, client_id;
	chat_t *chat;

	check_quiet(!msg_recv(client->socket, MSG_leave_chat, &chat_id, &client_id));

	log_info("Got leave chat message: %hu", client_id);

	chat = chat_get(client, chat_id);
	chat_leave(client, chat, client_id);

	return 0;
error:
	return 1;
}

int msg_handle_send_file(struct _client_t *client) {
	assert(client);

	transfer_t transfer = {.sending = 0, .fd = -1, .offset = 0};
	uint16_t len;
	chat_t *chat;
	size_t index;

	check_quiet(!msg_recv(client->socket, MSG_send_file,
				&transfer.chat_id, &transfer.fsize, &len, &buf, &transfer.file_id, &transfer.sender_id));
	buf[len] = 0;
	check_quiet(chat = chat_get(client, transfer.chat_id));
	check_quiet(transfer.file_id);
	transfer.fname = strdup(buf);

	check_quiet(index = sp_vector_add(&client->transfers, &transfer));

	check(chat_add_file(client, chat, index, transfer.sender_id) != -1,
			"Couldn't add file entry to chat.");

	return 0;
error:
	return 1;
}

int msg_handle_file_part(client_t *client) {
	assert(client);

	uint16_t len;
	uint16_t file_id = client->download_file_id; /* FIXME */
	transfer_t *transfer;

	check_quiet(!msg_recv(client->socket, MSG_file_part, &len, &buf));
	check_quiet(transfer = client_get_transfer(client, file_id));

	void *file = file_map(transfer->fd, PROT_WRITE, transfer->offset, len);
	memcpy(file, buf, len);
	file_unmap(file, len);

	transfer->offset += len;
	assert(transfer->offset <= transfer->fsize);

	if(transfer->offset == transfer->fsize) {
		close(transfer->fd);
		client->transferring = 0;
	}

	return 0;
error:
	return 1;
}

int msg_handle_user_update(client_t *client) {
	assert(client);
	
	uint16_t user_id;
	uint8_t status, len;
	msg_recv(client->socket, MSG_user_update, &user_id, &status, &len, &buf);
	buf[len] = '\0';

	user_t *user = user_get(client, user_id);
	
	if(status == US_IDENTIFY) {
		/* Just joined, grab auto-assigned name and id */
		client->id = user_id;
		client_set_name(client, buf);
	} else if(user_id == client->id) {
		/* We just set our name/status: this is the response */
		client->status = status;
		client_set_name(client, buf);
	} else if(!user) {
		check(!user_add(client, user_id, status, buf),
				"Couldn't add a new user.");
	} else if(status == US_OFFLINE) {
		user_del(client, user);
	} else {
		log_info("Setting a name: <%s>", buf);
		user_set_name(client, user, buf);
		user->status = status;
	}

	return 0;
error:
	return 1;
}

int msg_handle_msg(client_t *client) {
	assert(client);

	uint16_t chat_id, sender_id, len;
	chat_t *chat;

	msg_recv(client->socket, MSG_msg, &chat_id, &sender_id, &len, &buf);
	buf[len] = 0;

	check(chat = chat_get(client, chat_id),
			"Couldn't handle message in nonexisting chat.");

	chat_add_msg(client, chat, buf, sender_id);

	return 0;
error:
	return 1;
}
