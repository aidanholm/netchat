#ifndef CLIENT_UI_INPUT_H
#define CLIENT_UI_INPUT_H

#define __USE_XOPEN
#include <ncurses.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "debug.h"

typedef struct _client_ui_input_t {
	char value[1024];
	const char *prompt;
	uint16_t len;
} client_ui_input_t;

void client_ui_input_init(client_ui_input_t *ui);
void client_ui_input_input(client_ui_input_t *ui, int input);
void client_ui_input_resize(client_ui_input_t *ui, unsigned w, unsigned h);
void client_ui_input_draw(WINDOW *win, client_ui_input_t *ui, int focused);
void ui_input_clr(client_ui_input_t *ui);

#endif
