#include <assert.h>
#include "vector.h"
#include "ui_sidebar.h"
#include "user.h"
#include "msg.h"
#include "macros.h"

static inline void ncurses_win_rect(WINDOW *win, int x0, int y0, int w, int h, char ch) {
	for(int y=y0; y<y0+h; y++)
		for(int x=x0; x<x0+w; x++)
			mvwaddch(win, y, x, ch);
}

static void client_ui_draw_sidebar_sec(client_ui_sidebar_t *ui, const char *heading, vector_t *items,
		void (*item_renderer)(client_t *client, const void*, WINDOW *, int, int, int), unsigned int current, unsigned int dy) {
	assert(ui);
	assert(heading);
	assert(items);
	assert(item_renderer);

	const wchar_t* heading_bf = L"\u2550";

	for(int x=0; x<20; x++) mvwaddwstr(ui->win, dy, x, heading_bf);
    mvwprintw(ui->win, dy, 1, heading);

	void *item;
	vector_foreach(items, item) {
		unsigned int i = vector_indexof(items, item);

		if(i==current) {
			wattron(ui->win, COLOR_PAIR(1));
			ncurses_win_rect(ui->win, 0, 1+i+dy, 20, 1, ' ');
		}

		item_renderer(ui->client, item, ui->win, 0, 1+i+dy, i==current);

		if(i==current)
			wattroff(ui->win, COLOR_PAIR(1));
	}
}

void client_ui_chat_renderer(client_t *client, const void *c, WINDOW *win, int x, int y, int selected) {
	assert(client);
	assert(c);
	assert(win);
	assert(selected==0 || selected==1);

	const chat_t *chat = c;
	char out[21];

	const char *prefix = " ⚫ ";
	int colour = COL_US_ONLINE;

	strcpy(out, prefix);
	if(chat->users.size == 1) {
		snprintf(out, sizeof(out), "%sJust me", prefix);
		colour = COL_US_BUSY; /* HACk: red */
	} else if(chat->users.size == 2) {
		/* Get the other id, could be either one, due to how the server tells us */
		uint16_t *other_id = vector_get(&chat->users, 0);
		if(*other_id == client->id) other_id = vector_get(&chat->users, 1);
		user_t *other = user_get(client, *other_id);
		snprintf(out, sizeof(out), "%s%s", prefix, user_get_name(client, other));

		colour = other->status == US_ONLINE ? COL_US_ONLINE :
				 other->status == US_BUSY   ? COL_US_BUSY   :
				 other->status == US_AWAY   ? COL_US_AWAY   : colour;
	} else
		snprintf(out, sizeof(out), "%sGroup chat (%hhu)", prefix, (uint8_t)chat->users.size);

	out[20] = 0;
	mvwprintw(win, y, x, out);
	mvwchgat(win, y, x+1, 1, 0, colour+selected, NULL);
}


void client_ui_user_renderer(client_t *client, const void *u, WINDOW *win, int x, int y, int selected) {
	assert(client);
	assert(u);
	assert(win);
	assert(selected==0 || selected==1);

	const user_t *user = u;
	char out[21];

	const char *prefix = " ⚫ ";
	strncpy(out, prefix, strlen(prefix));
	strncpy(out+strlen(prefix), user_get_name(client, user), 20-strlen(prefix));
	out[20] = 0;

	mvwprintw(win, y, x, out);
	int colour = user->status == US_ONLINE ? COL_US_ONLINE :
		         user->status == US_BUSY   ? COL_US_BUSY   :
		         user->status == US_AWAY   ? COL_US_AWAY   :
				 COL_CURSOR;
	mvwchgat(win, y, x+1, 1, 0, colour+selected, NULL);

	/* Draw selected cursor */
	if(user->selected) {
		mvwprintw(win, y, x, "▶");
		mvwchgat(win, y, x+1, 1, 0, COL_CURSOR+selected, NULL);
	}
}

int client_ui_sidebar_init(client_ui_sidebar_t *ui, struct _client_t *client) {
	assert(ui);
	assert(client);

	ui->win = newwin(0, 0, 0, 0);
	ui->current = 0;
	ui->client = client;

	return 0;
}

void client_ui_sidebar_free(client_ui_sidebar_t *ui) {
	assert(ui);
	delwin(ui->win);
}

void client_ui_sidebar_draw(client_ui_sidebar_t *ui) {
	assert(ui);
	int w, h;

	getmaxyx(ui->win, h, w);
	werase(ui->win);

	unsigned int cursor;

	cursor = ui->current < ui->client->users.size ? ui->current : -1;
	client_ui_draw_sidebar_sec(ui, " Users ", &ui->client->users,
			client_ui_user_renderer, cursor, 0);

	cursor = ui->current < ui->client->users.size ? -1 : ui->current - ui->client->users.size;
	client_ui_draw_sidebar_sec(ui, " Chats ", &ui->client->chats,
			client_ui_chat_renderer, cursor, 1+ui->client->users.size);

	wrefresh(ui->win);
}

void client_ui_sidebar_input(client_ui_sidebar_t *ui, int input) {
	assert(ui);
	assert(input);

	pthread_mutex_lock(&ui->client->users_mutex);

	int over_user = (ui->current < ui->client->users.size),
		over_chat = !!client_current_chat(ui->client);
	user_t *user;
	uint16_t user_ids[ui->client->users.size], uid_i = 0; /* Ugly */

	switch(input) {
		/*
		 * User selection and chat starting
		 */
		case ' ': /* Select/deselect a user */
			if(over_user){
				user_t *other = vector_get(&ui->client->users, ui->current);
				other->selected = !other->selected;
			}
			break;
		case KEY_ENTER:
		case '\r':
		case '\n': /* Start a chat with selected users */
			if(over_user) {
				/* Always chat with this user */
				user_t *current_user = vector_get(&ui->client->users, ui->current);
				current_user->selected = 1;
				
				/* Build a list of user ids, reset selected state */
				vector_foreach(&ui->client->users, user) {
					if(user->selected)
						user_ids[uid_i++] = user->id;
					user->selected = 0;
				}
				msg_send(ui->client->socket, MSG_start_chat,
						(uint16_t)0, (uint16_t)uid_i, user_ids);
			} else if(over_chat) {
				ui->client->ui.focus = FOCUS_CHAT;
			}
			break;
		/*
		 * Leaving a chat
		 */
		case 'l':
			if(over_chat) {
				size_t i = ui->client->ui.chat.chat_index;
				chat_del(ui->client, client_current_chat(ui->client));
				/* Now select a different chat to display */
				if(i == ui->client->chats.size) i--;
				ui->client->ui.chat.chat_index = i < ui->client->chats.size ? i : (unsigned)-1;
				if(ui->client->chats.size == 0)
					ui->current = ui->client->users.size-1;
				debug("New chat_index = %u", ui->client->ui.chat.chat_index);
				ui->current = ui->client->users.size + ui->client->ui.chat.chat_index;
			}
			break;
		/*
		 * Setting your status
		 */
		case 'b':
			client_set_status(ui->client, US_BUSY);
			break;
		case 'a':
			client_set_status(ui->client, US_AWAY);
			break;
		case 'o':
			client_set_status(ui->client, US_ONLINE);
			break;
		/*
		 * Stuff handled by client_ui.c: up/download, set name
		 */
		case 'u':
		case 'd':
		case 'n':
			switch(input) {
				case 'u': ui->client->ui.input.prompt = "Upload from: "; break;
				case 'd': ui->client->ui.input.prompt = "Download to: "; break;
				case 'n': ui->client->ui.input.prompt = "New name: "; break;
			}
			ui->client->ui.op = input;
			ui->client->ui.focus = FOCUS_INPUT;
			break;
	}

	pthread_mutex_unlock(&ui->client->users_mutex);
}

void client_ui_sidebar_scroll(client_ui_sidebar_t *ui, int scroll) {
	assert(ui);
	assert(scroll != 0);

	pthread_mutex_lock(&ui->client->users_mutex);

	if(scroll < 0 && (unsigned)-scroll > ui->current)
		ui->current = 0;
	else
		ui->current += scroll;

	ui->current = min(ui->current, ui->client->users.size+ui->client->chats.size-1);

	chat_t *chat = NULL;
	if(ui->current >= ui->client->users.size) {
		unsigned int current = ui->current - ui->client->users.size;
		if(current < ui->client->chats.size) {
			chat = vector_get(&ui->client->chats, current);
			assert(chat);
		}
	}

	ui->client->ui.chat.chat_index = chat ? vector_indexof(&ui->client->chats, chat) : (unsigned)-1;

	pthread_mutex_unlock(&ui->client->users_mutex);
}
