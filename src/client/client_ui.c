#include <assert.h>
#include <signal.h>
#include <string.h>
#include <locale.h>

#include "client_ui.h"
#include "client.h"
#include "user.h"
#include "debug.h"

static void client_ui_draw(client_ui_t *ui, client_t *client);
static void client_ui_draw_toosmall(int w, int h);

int client_ui_init(client_ui_t *ui, client_t *client) {
	assert(ui);
	assert(client);

	setlocale(LC_ALL, "");
    initscr();
	cbreak();
    noecho();
	set_escdelay(25);
    timeout(25);
    keypad(stdscr, TRUE);
	curs_set(0);
	start_color();

	/* Set up colours */
	init_pair(1,COLOR_BLACK, COLOR_WHITE);
	init_pair(2,COLOR_WHITE, COLOR_BLACK);

	init_pair(COL_US_ONLINE, COLOR_GREEN, COLOR_BLACK);
	init_pair(COL_US_ONLINE_SEL, COLOR_GREEN, COLOR_WHITE);
	init_pair(COL_US_BUSY, COLOR_RED, COLOR_BLACK);
	init_pair(COL_US_BUSY_SEL, COLOR_RED, COLOR_WHITE);
	init_pair(COL_US_AWAY, COLOR_YELLOW, COLOR_BLACK);
	init_pair(COL_US_AWAY_SEL, COLOR_YELLOW, COLOR_WHITE);

	init_pair(COL_INPUT, COLOR_GREEN, COLOR_BLACK);
	init_pair(COL_INPUT_SEL, COLOR_BLACK, COLOR_WHITE);

	check(!client_ui_chat_init(&ui->chat),
			"Couldn't initialize chat UI.");
	check(!client_ui_sidebar_init(&ui->sidebar, client),
			"Couldn't initialize chat UI.");
	client_ui_input_init(&ui->input);

	ui->focus = FOCUS_SIDEBAR;
	ui->move_mult = 0;

	return 0;
error:
	return 1;
}

void client_ui_free(client_ui_t *ui) {
	client_ui_chat_free(&ui->chat);
	client_ui_sidebar_free(&ui->sidebar);
    endwin();
}

void client_ui_run(client_ui_t *ui, client_t *client) {
	assert(ui);
	assert(client);

	pthread_mutex_lock(&client->users_mutex);
	client_ui_draw(ui, client);
	pthread_mutex_unlock(&client->users_mutex);

	do {
		pthread_mutex_lock(&client->users_mutex);
		refresh();
		client_ui_draw(ui, client);
		pthread_mutex_unlock(&client->users_mutex);

		chat_t *current_chat = client_current_chat(client);
		int input = getch();

		if(input == ERR)
			continue;

		if((!current_chat || ui->focus == FOCUS_SIDEBAR) && input == 'q') {
			return;
		}
		/* Switch focus keybindings (also resets mult_move) */
		else if(ui->focus == FOCUS_SIDEBAR && input == 'i' && current_chat) {
			ui->move_mult = 0;
			ui->focus = FOCUS_CHAT;
		}
		else if(ui->focus == FOCUS_CHAT && input == 27/* excape */)
		{
			if(ui->move_mult)
				ui->move_mult = 0;
			else
				ui->focus = FOCUS_SIDEBAR;
		}
		/* Multi-move */
		else if(ui->focus == FOCUS_SIDEBAR && '0' <= input && input <= '9')
		{
			ui->move_mult *= 10;
			ui->move_mult += input - '0';
			continue;
		}
		/* input bar handling */
		else if(ui->focus == FOCUS_INPUT) {
			if(input == KEY_ENTER || input == '\r' || input == '\n') {
				if(ui->input.chars.size-1 > 0) {
					assert(strlen(ui->input.chars.data));

					if(ui->op == 'n')
						client_send_name(client, ui->input.chars.data);
					else if(ui->op == 'u' && current_chat) {
						client_upload_file(client, client_current_chat(client), ui->input.chars.data);
					} else if(ui->op == 'd' && current_chat) {
						chat_row_t *row = vector_get(&current_chat->rows, current_chat->cursor);
						if(row->type == CR_FILE)
							client_download_file(client, &row->file, ui->input.chars.data);
					}
					ui_input_clr(&ui->input);
					ui->input.prompt = NULL;
				}
				ui->focus = FOCUS_SIDEBAR;
			}
			/* Escape cancels the operation */
			else if(input == 27) {
				ui_input_clr(&ui->input);
				ui->input.prompt = NULL;
				ui->focus = FOCUS_SIDEBAR;
			} else
				client_ui_input_input(&ui->input, input);
		}
		/* Scroll the sidebar and chat window: focus-independant */
		else if(input == KEY_DOWN) {
			debug("Mult-move: %u", ui->move_mult);
			client_ui_sidebar_scroll(&ui->sidebar, (ui->move_mult ?: 1));
		}
		else if(input == KEY_UP)
			client_ui_sidebar_scroll(&ui->sidebar, -(ui->move_mult ?: 1));
		else if(ui->focus != FOCUS_CHAT && input == 'j')
			client_ui_chat_scroll(client, &ui->chat, (ui->move_mult ?: 1));
		else if(ui->focus != FOCUS_CHAT && input == 'k')
			client_ui_chat_scroll(client, &ui->chat, -(ui->move_mult ?: 1));
		/* Pass everything else along */
		else switch(ui->focus) {
			case FOCUS_CHAT:
				client_ui_chat_input(client, &ui->chat, input);
				break;
			case FOCUS_SIDEBAR:
				client_ui_sidebar_input(&ui->sidebar, input);
				break;
			default:
				break;
		}
		ui->move_mult = 0;
	} while(client->running);
}

static void client_ui_draw(client_ui_t *ui, client_t *client) {
	int w, h;
	getmaxyx(stdscr, h, w);

	if(w < 30 || h < 15)
		client_ui_draw_toosmall(w, h);
	else {
		mvwin(ui->sidebar.win, 0, 0);
		wresize(ui->sidebar.win, h, 20);

		mvwin(ui->chat.win, 0, 21);
		wresize(ui->chat.win, h, w-21);
		getmaxyx(ui->chat.win, ui->chat.h, ui->chat.w);

		const wchar_t* wstr = L"\u258F";

		/* Draw the separator */
		attron(COLOR_PAIR(2));
		for(int y=0; y<h; y++) mvaddwstr(y, 20, wstr);
		attroff(COLOR_PAIR(2));

		client_ui_sidebar_draw(&ui->sidebar);

		client_ui_chat_draw(client, &ui->chat);
		if(ui->focus == FOCUS_INPUT)
			client_ui_input_draw(stdscr, &ui->input, ui->focus == FOCUS_INPUT);
	}
}

static void client_ui_draw_toosmall(int w, int h) {
	erase();

	attron(A_BLINK);
	const char *msg = "Window too small!";
	mvprintw(h/2, (w-strlen(msg))/2, msg);
	attroff(A_BLINK);

	refresh();
}
