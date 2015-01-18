#include "client_ui.h"
#include "ui_input.h"
#include "utf8.h"

static inline void ncurses_win_rect(WINDOW *win, int x0, int y0, int w, int h, char ch) {
	for(int y=y0; y<y0+h; y++)
		for(int x=x0; x<x0+w; x++)
			mvwaddch(win, y, x, ch);
}

void client_ui_input_init(client_ui_input_t *ui) {
	assert(ui);
	memset(ui, 0, sizeof(*ui));
	vector_init(&ui->chars, sizeof(char));
	ui_input_clr(ui);
}

void client_ui_input_input(client_ui_input_t *ui, int input) {
	switch(input) {
		case KEY_BACKSPACE:
		case KEY_DC:
		case 0x7F: /* ASCII delete */
			if(ui->chars.size-1 == 0) break;
			char *chars = ui->chars.data;
			assert(chars[ui->chars.size-1] == 0);
			size_t newlen = utf8_char_prev(&chars[ui->chars.size-1]) - chars;
			chars[newlen] = 0;
			ui->chars.size = newlen+1;
			break;
		default:
			if(input < 256) {
				input = input < ' ' ? ' ' : input;

				/* Strip the last null byte, and add the char+null byte */
				char ch[2] = {input, '\0'};
				if(ui->chars.size) ui->chars.size--;
				vector_add(&ui->chars, ch, 2);
			}
			break;
	}
}

void client_ui_input_draw(WINDOW *win, client_ui_input_t *ui, int focused) {
	int w, h, col = focused ? COL_INPUT_SEL : COL_INPUT;
	getmaxyx(win, h, w);
	wattron(win, COLOR_PAIR(col));
	ncurses_win_rect(win, 0, h-1, w, 1, ' ');
	mvwprintw(win, h-1, 1, ui->prompt ?: "");
	mvwprintw(win, h-1, 1+strlen(ui->prompt ?: ""), ui->chars.data);
	wattroff(win, COLOR_PAIR(col));
}

void ui_input_clr(client_ui_input_t *ui) {
	ui->chars.size = 0;
	const char null = '\0';
	vector_add(&ui->chars, &null, 1);
}
