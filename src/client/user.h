#ifndef USER_H
#define USER_H

#include <stdint.h>
#include "status.h"
#include "client.h"
#include "debug.h"

typedef struct _user_t {
	uint16_t id;
	user_status_t status;
	int selected;
	unsigned name_index;
} user_t;

static int user_set_name(client_t *client, user_t *user, const char name[256]);

__attribute__((unused))
static int user_id_cmp(const void *A, const void *B) {
	const user_t *a = A, *b = B;
	return sort_by(a->id, b->id);
}

__attribute__((unused))
static int user_add(client_t *client, uint16_t id, uint8_t status, const char name[256]) {
	user_t new_user = { .id = id, .status = status, .selected = 0 };
	user_set_name(client, &new_user, name);

	/* TODO: check status */

	check_quiet(vector_add(&client->users, &new_user, 1));
	qsort(client->users.data, client->users.size,
			client->users.item_size, user_id_cmp);

	return 0;
error:
	return 1;
}

__attribute__((unused))
static void user_del(client_t *client, user_t *user) {
	vector_del(&client->users, vector_indexof(&client->users, user));
	qsort(client->users.data, client->users.size,
			client->users.item_size, user_id_cmp);
}

__attribute__((unused))
static user_t *user_get(client_t *client, uint16_t id) {
	user_t key = {.id = id};
	return bsearch(&key, client->users.data, client->users.size,
			client->users.item_size, user_id_cmp);
}

__attribute__((unused))
static const char *user_get_name(const client_t *client, const user_t *user) {
	assert(client);
	assert(user);

	return vector_get(&client->names, user->name_index);
}

__attribute__((unused))
static int user_set_name(client_t *client, user_t *user, const char name[256]) {
	assert(client);
	assert(user);
	assert(name);
	assert(0<strlen(name) && strlen(name)<256);

	const char *old_name = user_get_name(client, user);

	if(!strcmp(name, old_name))
		return 0;

	char *n;
	if(!(n = vector_add(&client->names, name, 1)))
		return 1;
	user->name_index = vector_indexof(&client->names, n);

	char buf[strlen(old_name)+strlen(" is now known as ")+strlen(name)+1];
	sprintf(buf, "%s is now known as %s", old_name, name);

	chat_t *chat;
	vector_foreach(&client->chats, chat) {
		uint16_t *c_id;
		vector_foreach(&chat->users, c_id) {
			if(*c_id == user->id) goto user_present;
		}
		continue;
user_present:
		chat_add_msg(client, chat, buf, 0);
	}

	return 0;
}

#endif

