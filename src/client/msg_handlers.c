#include <signal.h>
#include <unistd.h>

#include "msg_handlers.h"
#include "client.h"
#include "debug.h"
#include "user.h"
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

	uint16_t chat_id, len, file_id, sender_id;
	uint32_t fsize;
	chat_t *chat;

	check_quiet(!msg_recv(client->socket, MSG_send_file, &chat_id, &fsize, &len, &buf, &file_id, &sender_id));
	buf[len] = 0;
	check_quiet(chat = chat_get(client, chat_id));
	check_quiet(file_id);

	/* r used because fchat_file_i is unsigned, and error code is -1 */
	int r = chat_add_file(client, chat, buf, fsize, sender_id, file_id);
	check(r != -1, "Couldn't add file entry to chat.");
	client->fchat = chat;
	client->fchat_file_i = r;

	return 0;
error:
	return 1;
}

int msg_handle_file_part(client_t *client) {
	assert(client);

	uint16_t len;

	check_quiet(!msg_recv(client->socket, MSG_file_part, &len, &buf));

	void *file = file_map(client->file_fd, PROT_WRITE, client->offset, len);
	memcpy(file, buf, len);
	client->offset += len;

	/* Update the file progress bar in the UI */
	chat_row_t *row = vector_get(&client->fchat->rows, client->fchat_file_i);
	assert(row->type == CR_FILE);
	row->file.percent = 100*client->offset/client->fsize;

	file_unmap(file, len);

	assert(client->offset <= client->fsize);
	if(client->offset == client->fsize) {
		close(client->file_fd);
		client->fchat_file_i = -1;
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
