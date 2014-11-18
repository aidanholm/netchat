#include "client_ui.h"
#include "ui_input.h"

static inline void ncurses_win_rect(WINDOW *win, int x0, int y0, int w, int h, char ch) {
	for(int y=y0; y<y0+h; y++)
		for(int x=x0; x<x0+w; x++)
			mvwaddch(win, y, x, ch);
}

void client_ui_input_init(client_ui_input_t *ui) {
	assert(ui);
	memset(ui, 0, sizeof(*ui));
}

void client_ui_input_input(client_ui_input_t *ui, int input) {
	switch(input) {
		case KEY_BACKSPACE:
		case KEY_DC:
		case 0x7F: /* ASCII delete */
			if(ui->len == 0) break;
			assert(ui->value[ui->len] == 0);
			do {
				ui->len--;
				/* First two bits == 10 --> follow up byte */
			} while(ui->len && (ui->value[ui->len] & 0xc0)==0x80);
			ui->value[ui->len] = 0;
			break;
		default:
			if(input < 256) {
				if(input < ' ') input = ' ';
				if(ui->len < sizeof(ui->value)-4) {
					ui->value[ui->len++] = input;
					ui->value[ui->len] = 0;
				}
			}
			break;
	}
}

void client_ui_input_draw(WINDOW *win, client_ui_input_t *ui, int focused) {
	int w, h;
	getmaxyx(win, h, w);
	int col = focused ? COL_INPUT_SEL : COL_INPUT;
	wattron(win, COLOR_PAIR(col));
	ncurses_win_rect(win, 0, h-1, w, 1, ' ');
	mvwprintw(win, h-1, 1, ui->prompt ?: "");
	mvwprintw(win, h-1, 1+strlen(ui->prompt ?: ""), ui->value);
	wattroff(win, COLOR_PAIR(col));
}

void ui_input_clr(client_ui_input_t *ui) {
	ui->value[0] = 0;
	ui->len = 0;
}
