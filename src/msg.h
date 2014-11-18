#ifndef MSG_H
#define MSG_H

#include <assert.h>
#include <stdint.h>

#define MSG_TYPES \
	X(start_chat , "%hu%hu%k"        , chat id; num ids; htons(ids)) \
	X(leave_chat , "%hu%hu"          , chat id; user id) \
	X(msg        , "%hu%hu%hu%s"     , chat id; sender id; length; msg[length]) \
	X(send_file  , "%hu%u%hu%s%hu%hu", chat id; fsize; l; fname[l]; file id; sender) \
	X(file_part  , "%hu%s"           , length; blob[length]) \
	X(recv_file  , "%hu%u"           , chat id; file id) \
	X(user_update, "%hu%hhu%hhu%s"   , userid; status; length; name[length]) \

/*
 * Automatically generated stuff below
 */

#define X(name,fmt,comment) MSG_##name, 
typedef enum _msg_type_t { MSG_invalid = -1, MSG_TYPES MSG_NUM_TYPES } msg_type_t;
#undef X

#define X(name,fmt,comment) static const char * const msg_format_send_MSG_##name = "%hhu" fmt;
MSG_TYPES;
#undef X

#define X(name,fmt,comment) static const char * const msg_format_recv_MSG_##name = fmt;
MSG_TYPES;
#undef X

static inline msg_type_t msg_get_type(uint8_t type) {
	return (type < MSG_NUM_TYPES) ? type : MSG_invalid;
}

#define msg_send(fd, type, ...) \
	_msg_send((fd), msg_format_send_##type, (uint8_t)(type), ##__VA_ARGS__)

#define msg_recv(fd, type, ...) \
	_msg_recv((fd), msg_format_recv_##type, ##__VA_ARGS__)

extern int _msg_send(int fd, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern int _msg_recv(int fd, const char *fmt, ...) __attribute__ ((format ( scanf, 2, 3)));

#endif
