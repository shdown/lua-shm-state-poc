#ifndef dumb_alloc_h_
#define dumb_alloc_h_

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    void *mem;
    void *userdata;
    bool (*setnmem)(void *userdata, size_t oldnmem, size_t newnmem);
} DumbAllocData;

bool
dumb_alloc_init(DumbAllocData d);

void *
dumb_malloc(DumbAllocData d, size_t n);

void *
dumb_realloc(DumbAllocData d, void *block, size_t n);

void
dumb_free(DumbAllocData d, void *block);

#endif
