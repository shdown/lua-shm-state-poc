#ifndef mapping_kind_h_
#define mapping_kind_h_

#include <stddef.h>

typedef struct {
    size_t default_len;
    void * (*create)(size_t len);
    int (*reclaim_pages)(void *addr, size_t len);
} MappingKind;

extern const MappingKind MAPPING_KIND_PORTABLE;
extern const MappingKind MAPPING_KIND_ONDEMAND;
extern const MappingKind *mapping_kind_pdefault;

#endif
