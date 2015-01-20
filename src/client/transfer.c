#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "transfer.h"
#include "file.h"

const char* transfer_begin_upload(transfer_t *transfer, const chat_t *chat, const char *name) {
	assert(transfer);
	assert(chat);
	assert(name);
	
	struct stat f_stat;

	transfer->fname = strdup(name);
	transfer->chat_id = chat->id;
	transfer->offset = 0;
	transfer->sending = 1;

	if((transfer->fd = open(name, O_RDONLY)) < 0) {
		switch(errno) {
			case ENOENT:
				return "Cannot upload: no such file.";
				break;
			case EACCES:
				return "Cannot upload: permission denied.";
				break;
			case EINTR:
				return "Cannot upload: interrupted.";
				break;
			case ELOOP:
				return "Cannot upload: too many symbolic links.";
				break;
		}
		return "Cannot upload: miscellaneous error.";
	}

	fstat(transfer->fd, &f_stat);
	transfer->fsize = f_stat.st_size;

	return NULL;
}

const char* transfer_begin_download(transfer_t *transfer, const char *name) {
	assert(transfer);
	assert(transfer->fsize > 0);
	assert(name);

	if(transfer->sending)
		return "You uploaded this file!";

	transfer->offset = 0;
	if((transfer->fd = file_create(name, transfer->fsize)) == -1) {
		switch(errno) {
			case EEXIST:
				return "Cannot download: file already exists.";
		}
		return "Cannot download: miscellaneous error.";
	}

	return NULL;
}
