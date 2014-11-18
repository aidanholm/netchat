#ifndef CLIENT_UI_SIDEBAR_H
#define CLIENT_UI_SIDEBAR_H

#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>

struct _client_t;

typedef struct _client_ui_sidebar_t {
	WINDOW *win;
	unsigned int current;
	int move_mult;
	struct _client_t *client;
} client_ui_sidebar_t;

int client_ui_sidebar_init(client_ui_sidebar_t *ui, struct _client_t *client);
void client_ui_sidebar_free(client_ui_sidebar_t *ui);
void client_ui_sidebar_draw(client_ui_sidebar_t *ui);
void client_ui_sidebar_input(client_ui_sidebar_t *ui, int input);
void client_ui_sidebar_scroll(client_ui_sidebar_t *ui, int scroll);

#endif
