#include <assert.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "debug.h"
#include "client.h"
#include "user.h"
#include "msg.h"
#include "msg_handlers.h"
#include "file.h"
#include "macros.h"

static void *client_net_thread(void *c);
int client_connect(client_t *client, const char *hostname, unsigned int port);

#define X(name,fmt,fmt_comment) msg_handle_##name,
msg_handler_t msg_handlers[MSG_NUM_TYPES]= { MSG_TYPES };
#undef X

int client_init(client_t *client, const char *host, unsigned int port) {
	assert(client);

	check(!client_ui_init(&client->ui, client),
			"Couldn't initialize client UI.");

	vector_init(&client->users, sizeof(user_t));
	vector_init(&client->chats, sizeof(chat_t));
	vector_init(&client->names, 256*sizeof(char));
	check(!pthread_mutex_init(&client->users_mutex, NULL),
			"Couldn't create mutex");

	check(!client_connect(client, host, port),
			"Couldn't connect to %s:%d", host, port);

	client->name_index = -1;
	client->file_fd = -1;
	client->fchat_file_i = -1;
	client->status = US_ONLINE;

	return 0;
error:
	return 1;
}

static inline int msg_handle(msg_type_t type, client_t *client) {
	assert(MSG_invalid <= type && type < MSG_NUM_TYPES);
	assert(client);

	check(type != MSG_invalid, "Invalid message code '%d'", type);
	check(msg_handlers[type], "Invalid message type '%d'", type);
	check_quiet(!msg_handlers[type](client));

	return 0;
error:
	return 1;
}

void client_sigusr1_handler(__attribute__((unused)) int num) {
	return;
}

int client_connect(client_t *client, const char *hostname, unsigned int port) {
	assert(client);
	assert(hostname);
	assert(port);

	struct hostent *server;
	struct sockaddr_in server_addr;
	struct timeval tv;

	client->socket = socket(AF_INET, SOCK_STREAM, 0);
	check(client->socket != -1, "Couldn't create socket: %s", strerror(errno));

	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

	server = gethostbyname(hostname);
	check(server, "Couldn't find host '%s': %s", hostname, strerror(errno));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	check(!connect(client->socket, (struct sockaddr *) &server_addr,
			sizeof(server_addr)),
			"Couldn't connect to '%s': %s", hostname, strerror(errno));

	client->running = 1;
	struct sigaction handler = {.sa_handler=client_sigusr1_handler};
	sigaction(SIGUSR1, &handler, 0);

	check(!pthread_create(&client->net_thread, NULL, client_net_thread, client),
		"Couldn't create net thread: %s", strerror(errno));

	return 0;
error:
	return 1;
}

int client_disconnect(client_t *client) {
	assert(client);
	assert(client->socket >= 0);

	client->running = 0;

	/* Interrupt any ongoing read, then close the socket */
	client->running = 0;
	pthread_kill(client->net_thread, SIGUSR1);
	pthread_join(client->net_thread, NULL);
	close(client->socket);

	vector_free(&client->users);
	vector_free(&client->chats);
	vector_free(&client->names);

	return 0;
}

int client_upload_file(client_t *client, chat_t *chat, const char *fname) {
	assert(client);
	assert(chat);
	assert(fname);
	assert(strlen(fname));

	struct stat f_stat;

	if(client->file_fd >= 0) {
		if(client->sending)
			chat_add_msg(client, chat, "Cannot upload: already uploading another file.", 0);
		else
			chat_add_msg(client, chat, "Cannot upload: already downloading another file.", 0);
		return 1;
	}

	/*assert(client->fchat_file_i == (size_t)-1);*/

	if((client->file_fd = open(fname, O_RDONLY)) < 0) {
		switch(errno) {
			case ENOENT:
				chat_add_msg(client, chat, "Cannot upload: no such file.", 0);
				break;
			case EACCES:
				chat_add_msg(client, chat, "Cannot upload: permission denied.", 0);
				break;
			case EINTR:
				chat_add_msg(client, chat, "Cannot upload: interrupted.", 0);
				break;
			case ELOOP:
				chat_add_msg(client, chat, "Cannot upload: too many symbolic links.", 0);
				break;
			default:
				chat_add_msg(client, chat, "Cannot upload: miscellaneous error.", 0);
				break;
		}
		return 1;
	}

	fstat(client->file_fd, &f_stat);

	if(f_stat.st_size > UINT32_MAX) {
		chat_add_msg(client, chat, "Cannot upload: file too large.", 0);
		close(client->file_fd);
		return 1;
	}

	client->fsize = f_stat.st_size;
	client->offset = 0;
	client->sending = 1;

	msg_send(client->socket, MSG_send_file, chat->id, client->fsize,
			(uint16_t)strlen(fname), fname, (uint16_t)0, (uint16_t)0);

	/* r used because fchat_file_i is unsigned, and error code is -1 */
	int r = chat_add_file(client, chat, fname, client->fsize, client->id, 0);
	check(r != -1, "Couldn't add file entry to chat.");
	client->fchat = chat;
	client->fchat_file_i = r;

	return 0;
error:
	return 1;
}

int client_download_file(client_t *client, chat_file_t *file, const char *name) {
	assert(client);
	assert(file);
	assert(name);
	assert(strlen(name));

	chat_t *chat = client->ui.chat.chat;

	if((client->file_fd = file_create(name, file->fsize)) == -1) {
		switch (errno) {
			case EEXIST:
				chat_add_msg(client, chat, "File already exists.", 0);
				return 1;
				break;
		}
	}

	client->sending = 0;
	client->offset = 0;
	client->fsize = file->fsize;
	msg_send(client->socket, MSG_recv_file, chat->id, file->id); 

	return 0;
}

int client_send_file_part(client_t *client) {
	assert(client);

	if(client->file_fd == -1 || !client->sending)
		return 0;

	uint16_t len = min(4096, client->fsize - client->offset);
	char *buf = file_map(client->file_fd, PROT_READ, client->offset, len);
	check_quiet(!msg_send(client->socket, MSG_file_part, len, buf));
	file_unmap(buf, len);

	client->offset += len;
	assert(client->offset <= client->fsize);

	/* Update the file progress bar in the UI */
	chat_row_t *row = vector_get(&client->fchat->rows, client->fchat_file_i);
	assert(row->type == CR_FILE);
	row->file.percent = 100*client->offset/client->fsize;

	if(client->offset == client->fsize) {
		client->sending = 0;
		client->file_fd = -1;
	}

	return 0;
error:
	return 1;
}

int client_set_status(client_t *client, user_status_t status) {
	assert(client);
	assert(US_invalid < status && status < US_NUM_STATUSES );

	client->status = status;
	const char *name = vector_get(&client->names, client->name_index);

	log_info("My name is '%s'", name);

	return msg_send(client->socket, MSG_user_update,
			(uint16_t)0, (uint8_t)status, (uint8_t)strlen(name), name);
}

int client_set_name(client_t *client, const char name[256]) {
	assert(client);
	assert(0<strlen(name) && strlen(name)<256);

	const char *old_name = client->name_index < client->names.size ?
		vector_get(&client->names, client->name_index) : NULL;
  
	if(old_name && !strcmp(name, old_name))
		return 0;

	char *n;
	if(!(n = vector_add(&client->names, name, 1))) return 1;
	client->name_index = client->names.size-1;

	if(old_name) {
		char buf[strlen("You are now known as ")+strlen(name)+1];
		sprintf(buf, "You are now known as %s", name);

		chat_t *chat;
		vector_foreach(&client->chats, chat)
			chat_add_msg(client, chat, buf, 0);
	}

	return 0;
}

chat_t *client_current_chat(client_t *client) {
	assert(client);

	return client->ui.chat.chat;
}

int client_send_name(client_t *client, const char *name) {
	assert(client);
	assert(0<strlen(name) && strlen(name)<256);

	return msg_send(client->socket, MSG_user_update,
			(uint16_t)0, (uint8_t)client->status,
			(uint8_t)strlen(name), name);
}

static void *client_net_thread(void *c) {
	client_t *client = c;

	while(client->running) {
		uint8_t type;
		int r = read(client->socket, &type, 1);
		if(r == 0) {
			/* Client closed, just exit for now */
			/*client->running = 0;*/
			log_info("Server closed...");
			break;
		} else if(r > 0){
			pthread_mutex_lock(&client->users_mutex);
			check(!msg_handle(msg_get_type(type), client),
					"Message handling error.");
		pthread_mutex_unlock(&client->users_mutex);
		} else {
			switch(errno) {
				case EAGAIN:
				case EINTR:
					break;
				default:
					debug("Unhandled read error: %s", strerror(errno));
					abort();
			}
		}
		check(!client_send_file_part(client),
				"File transmission error.");
	}
error:
	return NULL;
}

/* TODO: grep for mutex use, add assertions */
/* TODO: longjmp error handling */

int main(int argc, char **argv) {
	client_t client;
	long int port;
	const char *host;

	if(argc >= 3) {
		check_warn(argc == 3, "Excess arguments ignored");
		check((port = strtol(argv[2], NULL, 10)) != LONG_MIN,
				"Invalid port number '%s'", argv[1]);
	} else
		port = 1024;

	host = argc >= 2 ? argv[1] : "localhost";

	log_info("Connecting to %s:%ld", host, port);

	check_quiet(!client_init(&client, host, port));
	client_ui_run(&client.ui, &client);
	client_disconnect(&client);
	client_ui_free(&client.ui);

	pthread_mutex_destroy(&client.users_mutex);

    return 0;
error:
	return 1;
}
