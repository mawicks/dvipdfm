#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

void *new (unsigned long size);

void release (void *mem);


#define NEW(n,type) (type *)(new ((n)*sizeof(type)))

#endif /* MEM_H */
