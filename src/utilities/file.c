#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "file.h"
#include "debug.h"

int file_create(const char *path, uint32_t size) {
	assert(path);
	assert(size > 0);

	int fd;

	if(!*path) { /* The server passes "" */
		char name[] = "/tmp/chatserver-XXXXXX";
		fd = mkstemp(name);
		unlink(name);
	} else { /* CLIENT */
		assert(strlen(path) > 0);
		/* mmap needs O_RDWR */
		if((fd = open(path, O_RDWR|O_CREAT|O_EXCL, 0644)) == -1) {
			/* The client caller file must handle already-existing case */
			 return -1;
		}
	}

	/* Stretch the file to the right size */
	if(lseek(fd, size-1, SEEK_SET) < 0 || write(fd, "", 1) != 1) {
		log_err("Error stretching '%s': %s", path, strerror(errno));
		unlink(path);
		close(fd);
		goto error;
	}

	return fd;
error:
	return -1;
}

void *file_map(int fd, int prot, off_t offset, size_t size) {
	assert(fd >= 0);
	assert(prot == PROT_READ || prot == PROT_WRITE);

	void *mem;
	if((mem = mmap(NULL, size, prot, MAP_SHARED, fd, offset)) == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	return mem;
}

void file_unmap(void *mem, size_t size) {
	assert(mem);
	assert(size > 0);

	munmap(mem, size);
}
