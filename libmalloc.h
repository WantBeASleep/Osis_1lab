#include <string.h>
#ifndef SRC_MALLOC_H_
#define SRC_MALLOC_H_

void *libmalloc(size_t size);
void *libcalloc(size_t number, size_t size);
void libfree(void *p);
void *librealloc(void *p, size_t size);


#endif