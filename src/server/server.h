#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <sys/types.h>
#include "sp_vector.h"

typedef struct _server_t {
	sp_vector_t clients;
	sp_vector_t chatrooms;
	sp_vector_t files;
	pthread_mutex_t clients_mutex;
	int socket;
	pthread_t accept_thread;
	int running;
} server_t;

#endif
