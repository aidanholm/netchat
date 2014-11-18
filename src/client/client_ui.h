#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include "ui_chat.h"
#include "ui_sidebar.h"

struct _client_t;

typedef enum {
	FOCUS_CHAT,
	FOCUS_SIDEBAR,
	FOCUS_INPUT,
} client_ui_focus_t;

typedef struct _client_ui_t {
	client_ui_focus_t focus;
	unsigned int move_mult;

	client_ui_chat_t chat;
	client_ui_sidebar_t sidebar;
	client_ui_input_t input;
	char op;
} client_ui_t;

/* Colour codes for the UI */
#define COL_US_ONLINE 3
#define COL_US_ONLINE_SEL 4
#define COL_US_BUSY 5
#define COL_US_BUSY_SEL 6
#define COL_US_AWAY 7
#define COL_US_AWAY_SEL 8
#define COL_INPUT 9
#define COL_INPUT_SEL 10
#define COL_NAME COL_US_ONLINE
#define COL_CURSOR COL_US_ONLINE

int client_ui_init(client_ui_t *ui, struct _client_t *client);
void client_ui_free(client_ui_t *ui);
void client_ui_run(client_ui_t *ui, struct _client_t *client);

#endif
