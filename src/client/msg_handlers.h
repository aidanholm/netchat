#ifndef MSG_HANDLERS_H
#define MSG_HANDLERS_H

#include <stdlib.h>
#include "msg.h"

struct _client_t;

int msg_handle_start_chat(struct _client_t *client);
int msg_handle_leave_chat(struct _client_t *client);
int msg_handle_send_file(struct _client_t *client);
#define msg_handle_recv_file NULL
int msg_handle_file_part(struct _client_t *client);
int msg_handle_user_update(struct _client_t *client);
int msg_handle_msg(struct _client_t *client);

typedef int (*msg_handler_t)(struct _client_t *client);

#endif
