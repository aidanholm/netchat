#define _GNU_SOURCE
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdarg.h>

#include "msg.h"
#include "debug.h"

static int write_block(int fd, const void *buf, size_t len) {
	assert(fd >= 0);
	assert(buf);

	const void *cur = buf;
	do {
		int ret = write(fd, cur, len);
		check(ret >= 0, "Write error: %s", strerror(errno));
		cur += ret;
		len -= ret;
	} while(len);

	return 0;
error:
	return 1;
}

static int read_block(int fd, void *buf, size_t len) {
	assert(fd >= 0);
	assert(buf);

	void *cur = buf;
	do {
		int ret = recv(fd, cur, len, 0);
		if(ret < 0) switch(errno) {
			case EAGAIN:
				ret = 0;
				break;
			default:
				log_err("recv error: %s", strerror(errno));
				abort();
				break;
		}
		cur += ret;
		len -= ret;
	} while(len);

	/* FIXME: remove useless return val */
	return 0;
}

int _msg_send(int fd, const char *fmt, ...) {
	assert(fd >= 0);
	assert(fmt);

	va_list ap;
	size_t prev_arg = 0;
	const char *first = fmt;

	va_start(ap, fmt);

	while(*fmt) {
		if(!strncmp(fmt, "%hhu", strlen("%hhu")))
		{
			uint8_t arg = va_arg(ap, unsigned int);
			if(fmt == first) { /* Special case: check the message type */
				assert(arg < MSG_NUM_TYPES);
			}
			check_quiet(!write_block(fd, &arg, sizeof(uint8_t)));
			fmt += strlen("%hhu");
			prev_arg = arg;
		}
		else if(!strncmp(fmt, "%hu", strlen("%hu")))
		{
			uint16_t arg = va_arg(ap, unsigned int),
					 n_arg = ntohs(arg);
			check_quiet(!write_block(fd, &n_arg, sizeof(uint16_t)));
			fmt += strlen("%hu");
			prev_arg = arg;
		}
		else if(!strncmp(fmt, "%u", strlen("%u")))
		{
			uint32_t arg = htonl(va_arg(ap, unsigned int));
			check_quiet(!write_block(fd, &arg, sizeof(uint32_t)));
			fmt += strlen("%u");
			prev_arg = arg;
		}
		else if(!strncmp(fmt, "%p", strlen("%p")))
		{
			const char *arg = va_arg(ap, const char *);
			check_quiet(!write_block(fd, arg, prev_arg));
			fmt += strlen("%p");
		}
		else if(!strncmp(fmt, "%k", strlen("%k")))
		{
			uint16_t *arg = va_arg(ap, uint16_t *);
			uint16_t copy[prev_arg];
			memcpy(copy, arg, prev_arg*sizeof(*arg));
			for(unsigned int i=0; i<prev_arg; i++) {
				copy[i] = htons(copy[i]);
			}
			check_quiet(!write_block(fd, copy, prev_arg*2));
			fmt += strlen("%k");
		}
		else {
			debug("Invalid format specifier '%s'", fmt);
			abort();
		}
	}

	va_end(ap);

	return 0;
error:
	return 1;
}

int _msg_recv(int fd, const char *fmt, ...) {
	assert(fd >= 0);
	assert(fmt);

	va_list ap;
	size_t prev_arg = 0;

	va_start(ap, fmt);

	while(*fmt) {
		if(!strncmp(fmt, "%hhu", strlen("%hhu")))
		{
			uint8_t *arg = va_arg(ap, uint8_t *);
			check_quiet(!read_block(fd, arg, sizeof(uint8_t)));
			fmt += strlen("%hhu");
			prev_arg = *arg;
		}
		else if(!strncmp(fmt, "%hu", strlen("%hu")))
		{
			uint16_t *arg = va_arg(ap, uint16_t *);
			check_quiet(!read_block(fd, arg, sizeof(uint16_t)));
			*arg = ntohs(*arg);
			fmt += strlen("%hu");
			prev_arg = *arg;
		}
		else if(!strncmp(fmt, "%u", strlen("%u")))
		{
			uint32_t *arg = va_arg(ap, uint32_t *);
			check_quiet(!read_block(fd, arg, sizeof(uint32_t)));
			*arg = ntohl(*arg);
			fmt += strlen("%u");
			prev_arg = *arg;
		}
		else if(!strncmp(fmt, "%p", strlen("%p")))
		{
			char **arg = va_arg(ap, char **);
			*arg = realloc(*arg, prev_arg);
			check_quiet(!read_block(fd, *arg, prev_arg));
			fmt += strlen("%p");
		}
		else if(!strncmp(fmt, "%k", strlen("%k")))
		{
			uint16_t **arg = va_arg(ap, uint16_t **);
			*arg = realloc(*arg, prev_arg*2);
			check_quiet(!read_block(fd, *arg, prev_arg*2));
			for(unsigned int i=0; i<prev_arg; i++) {
				uint16_t old = (*arg)[i];
				(*arg)[i] = ntohs((*arg)[i]);
				debug("recv: %hu --> %hu", old, (*arg)[i]);
			}
			fmt += strlen("%k");
		}
		else {
			debug("Invalid format specifier '%s'", fmt);
			assert(0);
		}
	}

	va_end(ap);

	return 0;
error:
	return 1;
}
