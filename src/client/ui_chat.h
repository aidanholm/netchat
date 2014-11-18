#ifndef CLIENT_UI_CHAT_H
#define CLIENT_UI_CHAT_H

#include <ncurses.h>

#include "chat.h"
#include "ui_input.h"

typedef struct _client_ui_chat_t {
	chat_t *chat;
	WINDOW *win;
	int w, h;
	client_ui_input_t input;
} client_ui_chat_t;

int client_ui_chat_init(client_ui_chat_t *ui);
void client_ui_chat_free(client_ui_chat_t *ui);
void client_ui_chat_draw(struct _client_t *client, client_ui_chat_t *ui);
void client_ui_chat_input(struct _client_t *client, client_ui_chat_t *ui, int input);

void client_ui_chat_show(client_ui_chat_t* ui, chat_t *chat);
void client_ui_chat_scroll(client_ui_chat_t *ui, int scroll);

#endif
