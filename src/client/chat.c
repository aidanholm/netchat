#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#define __USE_XOPEN
#include <wchar.h>

#include "client.h"
#include "chat.h"
#include "msg.h"
#include "debug.h"
#include "user.h"

static int chat_id_cmp(const void *A, const void *B) {
	const chat_t *a = A, *b = B;
	return sort_by(a->id, b->id);
}

static int utf8_scrlen(const char *msg) {
	wchar_t wcstr[512];
	mbstowcs(wcstr, msg, sizeof(wcstr)/sizeof(*wcstr));
	int len = wcswidth(wcstr, sizeof(wcstr)/sizeof(*wcstr));
	debug("Screen width of '%s' is %d", msg, len);
	return len;
}

int chat_add(client_t *client, uint16_t chat_id, uint16_t *client_ids, uint16_t num_ids) {
	assert(client);
	assert(client_ids);
	assert(num_ids > 0);
	assert(chat_id);

	chat_t chat;

	/* Initialize the chat */
	chat.id = chat_id;
	chat.top = chat.cursor = chat.morebelow = 0;
	chat.max_namelen = 0;
	vector_init(&chat.chars, sizeof(char));
	vector_init(&chat.rows,  sizeof(chat_row_t));
	vector_init(&chat.users, sizeof(uint16_t));
	check_quiet(vector_add(&chat.users, client_ids, num_ids));

	/* Add the chat to the client's set of chats */
	check_quiet(vector_add(&client->chats, &chat, 1));
	qsort(client->chats.data, client->chats.size, client->chats.item_size, chat_id_cmp);

	return 0;
error:
	vector_free(&chat.users);
	return 1;
}

void chat_leave(client_t *client, chat_t *chat, uint16_t other_id) {
	assert(client);
	assert(chat);
	assert(other_id);

	uint16_t *c_id;
	user_t *other;
	const char *other_name;

	vector_foreach(&chat->users, c_id) {
		debug("Comparing %hu and %hu", *c_id, other_id);
		if(*c_id == other_id) {
			vector_del(&chat->users, vector_indexof(&chat->users, c_id));
			goto deleted;
		}
	}

	debug("No such user in chat");
	return;

deleted:
	other = user_get(client, other_id);
	other_name = user_get_name(client, other);
	char buf[strlen(other_name)+strlen(" has left the chat.")+1];
	sprintf(buf, "%s has left the chat.", other_name);
	chat_add_msg(client, chat, buf, 0);
}

void chat_del(client_t *client, chat_t *chat) {
	assert(client);
	assert(chat);

	msg_send(client->socket, MSG_leave_chat, chat->id, client->id);

	vector_del(&client->chats, vector_indexof(&client->chats, chat));
	qsort(client->chats.data, client->chats.size,
			client->chats.item_size, chat_id_cmp);
}

chat_t *chat_get(client_t *client, uint16_t id) {
	chat_t key = {.id = id};
	return bsearch(&key, client->chats.data, client->chats.size,
			client->chats.item_size, chat_id_cmp);
}

int chat_send_msg(client_t *client, chat_t *chat, const char *msg) {
	assert(client);
	assert(chat);
	assert(msg);
	assert(client->id);
	assert(chat->id);

	msg_send(client->socket, MSG_msg, chat->id, client->id, (uint16_t)strlen(msg), msg);

	return 0;
}

int chat_add_msg(client_t *client, chat_t *chat, const char *msg, uint16_t from_id) {
	assert(client);
	assert(chat);
	assert(msg);

	chat_row_t new;
	const void *start;
	user_t *from = user_get(client, from_id);

	/* TODO: validate */

	new.type = CR_MSG;
	new.name_index = from ? from->name_index : from_id==client->id ? client->name_index : -1;
	new.from = from_id;

	new.msg.num_lines = 0;
	new.msg.len = strlen(msg);
	check_quiet(start = vector_add(&chat->chars, msg, new.msg.len+1));
	new.msg.start = vector_indexof(&chat->chars, start);
	check_quiet(vector_add(&chat->rows, &new, 1));

#if 0
	/* Disabled, can make cursor scroll out of sight */
	if(chat->cursor == chat->rows.size - 2)
		chat->cursor++;

	int num_lines = 0;
	for(int m=chat->cursor-1; m>=chat->top; m--) {
		chat_row_t *row = vector_get(&chat->rows, m);
		num_lines += row->type == CR_FILE ? 1 : row->msg.num_lines;

		if(num_lines > client->ui.chat.h-1) {
			chat->top = m+1;
			break;
		}
	}
	
#endif

	chat->max_namelen = max(chat->max_namelen, utf8_scrlen(chat_row_name(client, new)));

	return 0;
error:
	return 1;
}

int chat_add_file(client_t *client, chat_t *chat, size_t index, uint16_t from_id) {
	assert(client);
	assert(chat);

	chat_row_t new = {.type = CR_FILE, .from = from_id, .file.id = index};

	/* Get the name from the name index */
	const user_t *from = user_get(client, from_id);
	new.name_index = from ? from->name_index : from_id==client->id ? client->name_index : -1;

	check_quiet(vector_add(&chat->rows, &new, 1));

	chat->max_namelen = max(chat->max_namelen, utf8_scrlen(chat_row_name(client, new)));

	return chat->rows.size-1;
error:
	return -1;
}

const char *chat_row_name(client_t *client, chat_row_t row) {
	assert(client);

	if(row.name_index == (unsigned)-1) {
		assert(!user_get(client, row.from));
		return row.from==client->id ?
			vector_get(&client->names, client->name_index) : "";
	} else {
	   return vector_get(&client->names, row.name_index);
	}
}
