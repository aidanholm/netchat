#include <assert.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <wchar.h>

#include "ui_chat.h"
#include "user.h"
#include "debug.h"
#include "macros.h"

static inline void ncurses_win_rect(WINDOW *win, int x0, int y0, int w, int h, char ch) {
	for(int y=y0; y<y0+h; y++)
		for(int x=x0; x<x0+w; x++)
			mvwaddch(win, y, x, ch);
}

int client_ui_chat_init(client_ui_chat_t *ui) {
	assert(ui);

	ui->win = newwin(0, 0, 0, 0);
	ui->chat = NULL;
	client_ui_input_init(&ui->input);
	ui->input.prompt = "> ";

	return 0;
}

void client_ui_chat_free(client_ui_chat_t *ui) {
	assert(ui);
	delwin(ui->win);
}

static void client_ui_chat_msg_draw(client_t *client, client_ui_chat_t *ui, unsigned int *y, unsigned int ylim, chat_row_t *msg) {
	assert(client);
	assert(ui);
	assert(ui->win);
	assert(*y < ylim);

	const unsigned name_width = ui->chat->max_namelen;
	const char *username = chat_row_name(client, *msg);
	char *m = vector_get(&ui->chat->chars, msg->msg.start);
	unsigned int ystart = *y;
	int indent;

	mvwprintw(ui->win, *y, 2, "% *s ", name_width, username);
	mvwchgat(ui->win, *y, 2, name_width, 0, COL_NAME, NULL);
	indent = name_width + 1;

	/* Very basic wrapping: just breaks anywhere! At least it's
	 * multilingual and indents properly... */
	while(*m) {
		char *line_end = m;
		int displen = 0;
		do {
			wchar_t wchar;
			/* Advance line_end (utf8) */
			if((*line_end & 0x80) == 0x00) line_end += 1;
			else if((*line_end & 0xe0) == 0xc0) line_end += 2;
			else if((*line_end & 0xf0) == 0xe0) line_end += 3;
			else if((*line_end & 0xf8) == 0xf0) line_end += 4;
			/* Find display width of char */
			mbstowcs(&wchar, line_end, 1);
			displen += wcwidth(wchar);
		} while(displen<ui->w-indent-2-2-2 && *line_end);

		/* Draw [m .. line_end) */
		if(*y < ylim)
			mvwprintw(ui->win, (*y)++, 2+indent, "| %.*s", line_end-m, m);
		m = line_end;
	}

	msg->msg.num_lines = *y - ystart;
}

static void client_ui_chat_file_draw(client_t *client, client_ui_chat_t *ui, unsigned int *y, unsigned int ylim, chat_row_t *file) {
	assert(client);
	assert(ui);
	assert(ui->win);
	assert(*y < ylim);

	const char *username = chat_row_name(client, *file);
	const unsigned name_width = ui->chat->max_namelen;

	switch(file->file.percent) {
		case -1: mvwprintw(ui->win, (*y)++, 2, "% *s | File [%s] (%u bytes) | Ready to download",
			name_width, username, file->file.fname, file->file.fsize);
			 break;
		case 100: mvwprintw(ui->win, (*y)++, 2, "% *s | File [%s] (%u bytes) | Finished",
			name_width, username, file->file.fname, file->file.fsize);
			 break;
		 default: mvwprintw(ui->win, (*y)++, 2, "% *s | File [%s] (%u bytes) | %u%%",
			name_width, username, file->file.fname, file->file.fsize, file->file.percent);
			 break;
	}

	mvwchgat(ui->win, *y-1, 2, name_width, 0, COL_NAME, NULL);
}

void client_ui_chat_draw(client_t *client, client_ui_chat_t *ui) {
	assert(client);
	assert(ui);
	assert(ui->win);

	WINDOW *win = ui->win;
	werase(win);

	if(!ui->chat) {
		const char *msg = "Select a user on the left to start chatting!";
		mvwprintw(win, ui->h/2, (ui->w-strlen(msg))/2, msg);
		wrefresh(win);
		return;
	}

	/* Draw chat messages */

	unsigned int y = 0;
	ui->chat->morebelow = 0;
	for(unsigned int i=ui->chat->top; i<ui->chat->rows.size ; i++) {
		chat_row_t *row = vector_get(&ui->chat->rows, i);

		switch(row->type) {
			case CR_MSG:
				client_ui_chat_msg_draw(client, ui, &y, ui->h-1, row);
				break;
			case CR_FILE:
				client_ui_chat_file_draw(client, ui, &y, ui->h-1, row);
				break;
			default:
				assert(0);
				break;
		}

		if(y>=(unsigned)ui->h-1) {
			/*if(i<ui->chat->rows.size-1)*/
			ui->chat->morebelow = 1;
			break;
		}
	}

	wattron(ui->win, COLOR_PAIR(COL_CURSOR));

	/* Draw chat cursor */
	if(ui->chat->rows.size>0) {
		int y = 0;
		for(int m=ui->chat->top; m<ui->chat->cursor; m++) {
			chat_row_t *row = vector_get(&ui->chat->rows, m);
			y += row->type == CR_FILE ? 1 : row->msg.num_lines;
		}
		mvwprintw(win, y, 0, "▶");
	}

	/* Draw more above/below indicators */
	if(ui->chat->rows.size>0 && ui->chat->top>0)
		mvwprintw(win, 0, ui->w-1, "▲");
	if(ui->chat->morebelow)
		mvwprintw(win, ui->h-2, ui->w-1, "▼");

	wattroff(ui->win, COLOR_PAIR(COL_CURSOR));

	mvwprintw(win, 0, ui->w-3, "%u", strlen(client->ui.input.value));
	client_ui_input_draw(win, &ui->input, client->ui.focus == FOCUS_CHAT );

	wrefresh(win);
}

void client_ui_chat_input(client_t *client, client_ui_chat_t *ui, int input) {
	assert(ui);
	assert(input);

	if(!ui->chat)
		return;

	pthread_mutex_lock(&client->users_mutex);

	switch(input) {
		case KEY_ENTER:
		case '\r':
		case '\n':
			if(ui->input.len == 0) break;
			assert(strlen(ui->input.value));
			chat_send_msg(client, ui->chat, ui->input.value);
			ui->input.value[0] = 0;
			ui->input.len = 0;
			break;
		default:
			client_ui_input_input(&ui->input, input);
			break;
	}

	pthread_mutex_unlock(&client->users_mutex);
}

void client_ui_chat_show(client_ui_chat_t* ui, chat_t *chat) {
	ui->chat = chat;
}

void client_ui_chat_scroll(client_ui_chat_t *ui, int scroll) {
	assert(ui);
	assert(scroll != 0);

	if(!ui->chat)
		return;

	if(scroll < 0) {
		if(ui->chat->cursor == ui->chat->top)
			ui->chat->top = max(0, ui->chat->top+scroll);
		ui->chat->cursor = max(0, ui->chat->cursor+scroll);
	} else {
		if(ui->chat->morebelow)
			ui->chat->top = min(ui->chat->rows.size-1, (unsigned)ui->chat->top+scroll);
		ui->chat->cursor = min(ui->chat->rows.size-1, (unsigned)ui->chat->cursor+scroll);
	}
}
