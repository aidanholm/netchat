#include <assert.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <wchar.h>

#include "ui_chat.h"
#include "user.h"
#include "debug.h"
#include "macros.h"
#include "utf8.h"

int client_ui_chat_init(client_ui_chat_t *ui) {
	assert(ui);

	ui->win = newwin(0, 0, 0, 0);
	ui->chat_index = (unsigned) -1;
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

	const chat_t *chat = client_current_chat(client);
	const unsigned name_width = chat->max_namelen;
	const char *username = chat_row_name(client, *msg);
	char *m = vector_get(&chat->chars, msg->msg.start);
	unsigned int ystart = *y;
	int indent;

	mvwprintw(ui->win, *y, 0, "  % *s%s ", name_width-utf8_scrlen(username), "", username);
	mvwchgat(ui->win, *y, 2, name_width, 0, COL_NAME, NULL);
	indent = name_width + 1;

	/* Very basic wrapping: just breaks anywhere! At least it's
	 * multilingual and indents properly... */
	while(*m) {
		/* Find where to break */
		char *line_end = m;
		int displen = 0;
		do {
			wchar_t wchar;
			line_end += utf8_char_bytewidth(line_end);
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

	const chat_t *chat = client_current_chat(client);
	const char *username = chat_row_name(client, *file);
	const unsigned name_width = chat->max_namelen;
	transfer_t *transfer = sp_vector_get(&client->transfers, file->file.id);

	unsigned percent = transfer->offset*100/transfer->fsize;

	switch(percent) {
		case 0: mvwprintw(ui->win, (*y)++, 0, "  % *s%s | File [%s] (%u bytes) | Ready to download",
			name_width-utf8_scrlen(username), "", username, transfer->fname, transfer->fsize);
			 break;
		case 100: mvwprintw(ui->win, (*y)++, 0, "  % *s%s | File [%s] (%u bytes) | Finished",
			name_width-utf8_scrlen(username), "", username, transfer->fname, transfer->fsize);
			 break;
		 default: mvwprintw(ui->win, (*y)++, 0, "  % *s%s | File [%s] (%u bytes) | %u%%",
			name_width-utf8_scrlen(username), "", username, transfer->fname, transfer->fsize, percent);
			 break;
	}

	mvwchgat(ui->win, *y-1, 2, name_width, 0, COL_NAME, NULL);
}

void client_ui_chat_draw(client_t *client, client_ui_chat_t *ui) {
	assert(client);
	assert(ui);
	assert(ui->win);

	chat_t *chat = client_current_chat(client);
	WINDOW *win = ui->win;
	werase(win);

	if(!chat) {
		const char *msg = "Select a user on the left to start chatting!";
		mvwprintw(win, ui->h/2, (ui->w-strlen(msg))/2, msg);
		wrefresh(win);
		return;
	}

	/* Scroll the chat view in order to keep chat->cursor within the view */

	if(chat->top > chat->cursor)
		chat->top = chat->cursor;
	else if(chat->top < chat->cursor) {
		/* Count the number of lines from the top until we hit the first
		 * not-fully-visible message */
		int num_lines = 0;
		for(unsigned int m=chat->top; m<chat->rows.size; m++) {
			chat_row_t *row = vector_get(&chat->rows, m);
			num_lines += row->type == CR_FILE ? 1 : row->msg.num_lines;
			if(num_lines >= ui->h-1) {
				/* Message m is not fully visible */
				if(chat->cursor >= m)
					chat->top++;
				else
					break;
			}
		}
	}
	/* Draw chat messages */

	unsigned int y = 0;
	chat->morebelow = 0;
	for(unsigned int i=chat->top; i<chat->rows.size ; i++) {
		chat_row_t *row = vector_get(&chat->rows, i);

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

		/* Always draw the next message out of view;
		 * needed so we know how many lines it is, so we can scroll down */
		if(chat->morebelow)
			break;
		if(y>=(unsigned)ui->h-1)
			chat->morebelow = 1;
	}

	wattron(ui->win, COLOR_PAIR(COL_CURSOR));

	/* Draw chat cursor */
	if(chat->rows.size>0) {
		int y = 0;
		for(int m=chat->top; m<chat->cursor; m++) {
			chat_row_t *row = vector_get(&chat->rows, m);
			y += row->type == CR_FILE ? 1 : row->msg.num_lines;
		}
		mvwprintw(win, y, 0, "▶");
	}

	/* Draw more above/below indicators */
	if(chat->rows.size>0 && chat->top>0)
		mvwprintw(win, 0, ui->w-1, "▲");
	if(chat->morebelow)
		mvwprintw(win, ui->h-2, ui->w-1, "▼");

	wattroff(ui->win, COLOR_PAIR(COL_CURSOR));

	client_ui_input_draw(win, &ui->input, client->ui.focus == FOCUS_CHAT );

	wrefresh(win);
}

void client_ui_chat_input(client_t *client, client_ui_chat_t *ui, int input) {
	assert(ui);
	assert(input);

	chat_t *chat = client_current_chat(client);

	if(!chat)
		return;

	pthread_mutex_lock(&client->users_mutex);

	switch(input) {
		case KEY_ENTER:
		case '\r':
		case '\n':
			if(ui->input.chars.data-1 == 0) break;
			chat_send_msg(client, chat, ui->input.chars.data);
			ui_input_clr(&ui->input);
			break;
		default:
			client_ui_input_input(&ui->input, input);
			break;
	}

	pthread_mutex_unlock(&client->users_mutex);
}

void client_ui_chat_scroll(const client_t *client, client_ui_chat_t *ui, int scroll) {
	assert(ui);
	assert(scroll != 0);

	chat_t *chat = client_current_chat(client);

	if(!chat)
		return;

	if(scroll < 0)
		chat->cursor = max(0, chat->cursor+scroll);
	else
		chat->cursor = min(chat->rows.size-1, (unsigned)chat->cursor+scroll);
}
