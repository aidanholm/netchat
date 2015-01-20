#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>

#include "server.h"
#include "client.h"
#include "chatroom.h"
#include "file_entry.h"
#include "msg_handlers.h"
#include "status.h"
#include "debug.h"
#include "macros.h"

server_t *g_server; /* Yuck, used for sigint handler */

static void *server_accept_thread(void *server);

#define X(name,fmt,fmt_comment) msg_handle_##name,
msg_handler_t msg_handlers[MSG_NUM_TYPES]= { MSG_TYPES };
#undef X

void sigint_handler(__attribute__((unused)) int num) {
	g_server->running = 0;
}

void sigusr1_handler(__attribute__((unused)) int num) {
}

static inline int msg_handle(msg_type_t type, server_t *server, client_t *client) {
	assert(MSG_invalid <= type && type < MSG_NUM_TYPES);
	assert(server);
	assert(client);

	check(type != MSG_invalid, "Invalid message code '%d'", type);
	check(msg_handlers[type], "Invalid message type '%d'", type);
	check_quiet(!msg_handlers[type](server, client));

	return 0;
error:
	return 1;
}

int server_init(server_t *server, unsigned int port) {
	assert(server);

	log_info("Server starting...");

	struct sockaddr_in server_addr;

	sp_vector_init(&server->clients, sizeof(client_t));
	sp_vector_init(&server->chatrooms, sizeof(chatroom_t));
	sp_vector_init(&server->files, sizeof(file_entry_t));
	server->clients_mutex = ((pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER);

	server->socket = socket(AF_INET, SOCK_STREAM, 0);
	check(server->socket != -1, "Couldn't create socket: %s", strerror(errno));

	int on = 1;
	check(!setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)),
			"Couldn't set socket 'reuse address' option: %s", strerror(errno));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	check(!bind(server->socket, (struct sockaddr*) &server_addr, sizeof(server_addr)),
			"Couldn't bind socket to port %u: %s", port, strerror(errno));
	listen(server->socket, 0);

	server->running = 1;
	check(!pthread_create(&server->accept_thread, NULL, server_accept_thread, server),
		"Couldn't create accept thread: %s", strerror(errno));

	struct sigaction int_handler = {.sa_handler=sigint_handler};
	sigaction(SIGINT, &int_handler, 0);
	struct sigaction usr1_handler = {.sa_handler=sigusr1_handler};
	sigaction(SIGUSR1, &usr1_handler, 0);

	log_info("Server started.");

	return 0;
error:
	return 1;
}

void server_free(server_t *server) {
	assert(server);

	log_info("Server stopping...");

	client_t *client;

	pthread_mutex_lock(&server->clients_mutex);

	/* FIXME */
	/*sp_vector_foreach(&server->chatrooms, chatroom)*/
		/*chatroom_send_msg(server, chatroom, NULL, "Server shutting down...\n");*/

	sp_vector_foreach(&server->clients, client)
		client_disconnect(server, client);

	server->running = 0;
	pthread_kill(server->accept_thread, SIGUSR1);
	pthread_join(server->accept_thread, NULL);
	close(server->socket);

	sp_vector_free(&server->clients);
	sp_vector_free(&server->chatrooms);

	pthread_mutex_unlock(&server->clients_mutex);
	pthread_mutex_destroy(&server->clients_mutex);

	log_info("Server stopped.");
}

static void *server_accept_thread(void *s) {
	server_t *server = s;

	while(server->running) {
		client_t new_client, *client;
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		size_t id;
		int fd;

		fd = accept(server->socket, (struct sockaddr *) &client_addr, &client_len);
		if(fd < 0) switch(errno) {
			case EINTR:
				continue;
			default:
				log_warn("Couldn't accept connection: %s %d", strerror(errno), errno);
				break;
		}

		pthread_mutex_lock(&server->clients_mutex);

		/* TODO: when the client connects, we have to tell all the other clients, */
		/* before we start any new chats, otherwise a client might immediately    */
		/* jump into a group chat and invite someone who doesn't know about the   */
		/* new client yet, and they'll get a client_id that isn't in their list   */
		/* of clients */

		check_quiet(id = sp_vector_add(&server->clients, &new_client));
		client = sp_vector_get(&server->clients, id);
		client_connect(server, client, fd);
error:
		pthread_mutex_unlock(&server->clients_mutex);
	}

	return NULL;
}

void server_loop(server_t *server) {
	assert(server);

	while(server->running) {
		client_t *client;
		unsigned i;
		struct pollfd fds[server->clients.size];

		/* We just build an array of pollfd structs on the stack for now;
		 * TODO: keep this array in between iterations. This adds complications;
		 * We cannot modify fds[] during a poll() call, so we have to interrupt
		 * the call to poll, then rebuild the array. This would allow us to
		 * remove the timeout to poll() however (see below)*/
		i=0;
		sp_vector_foreach(&server->clients, client) {
			fds[i++] = (struct pollfd){.fd = client->fd, .events=POLLIN|POLLOUT};
		}

		/* A timeout is necesary because a new client may connect after poll()
		 * is called; without a timeout the client would be ignored until after
		 * another message was sent/received, and poll() was called again.
		 * The alternative is interrupting poll() with a system call whenever
		 * a client is added or removed, before rebuilding the pollfd table */
		check_warn(poll(fds, sizeof(fds)/sizeof(*fds), 100) >= 0,
				"Error polling sockets: %s", strerror(errno));

		pthread_mutex_lock(&server->clients_mutex);

		i=0;
		sp_vector_foreach(&server->clients, client) {
			assert(fds[i].fd == client->fd);
			if(fds[i].revents & POLLIN) {
				uint8_t type;
				int r = read(client->fd, &type, 1);

				if(r == 0) {
					client_disconnect(server, client);
					size_t client_id = sp_vector_indexof(&server->clients, client);
					sp_vector_del(&server->clients, client_id);
				} else if(r > 0) {
					msg_handle(msg_get_type(type), server, client);
				}
			} else if(fds[i].revents & POLLOUT) {
				client_send_file_part(client);
			}
			i++;
		}

		pthread_mutex_unlock(&server->clients_mutex);
	}
}

int main(int argc, char **argv) {
	server_t server;
	long int port;

	g_server = &server;

	if(argc >= 2) {
		check_warn(argc == 2, "Excess arguments ignored");
		check((port = strtol(argv[1], NULL, 10)) != LONG_MIN,
				"Invalid port number '%s'", argv[1]);
	} else
		port = 1024;
	log_info("Using port %ld", port);

	check(!server_init(&server, port), "Couldn't start server, sorry!");
	server_loop(&server);
	server_free(&server);

	return 0;
error:
	return 1;
}
