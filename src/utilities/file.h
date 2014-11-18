#ifndef FILE_H
#define FILE_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

/* Used when server/client receives a file: they first create the file
 * of the right size, then mmap chunks in one at a time
 * Returns -1 on error
 *
 * Server use: path='/tmp/', flags = O_TMPFILE
 * Client use: path='outfile', flags = 
 */
int file_create(const char *path, uint32_t size);
void *file_map(int fd, int prot, off_t offset, size_t size);
void file_unmap(void *mem, size_t size);

#endif
