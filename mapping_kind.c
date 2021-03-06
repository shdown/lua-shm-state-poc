#define _DEFAULT_SOURCE
#include "mapping_kind.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

enum {
    Kb = 1024,
    Mb = 1024 * Kb,
    Gb = 1024 * Mb,
};

static
void *
create_mapping_portable(size_t len)
{
    const int fd = open("/dev/zero", O_RDWR);
    if (fd < 0) {
        perror("open: /dev/zero");
        abort();
    }
    void *r = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return r == MAP_FAILED ? NULL : r;
}

#ifdef __linux__
static
void *
create_mapping_ondemand(size_t len)
{
    void *r = mmap(
        NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0);
    return r == MAP_FAILED ? NULL : r;
}

static
int
punch_hole(void *addr, size_t len)
{
    return madvise(addr, len, MADV_REMOVE);
}
#endif

const MappingKind MAPPING_KIND_PORTABLE = {
    .default_len = 16 * Mb,
    .create = create_mapping_portable,
};

const MappingKind MAPPING_KIND_ONDEMAND = {
#if UINTPTR_MAX == 0xffffffffffffffffULL
    // 64-bit system
    .default_len = 16ULL * Gb,
#else
    // 32-bit system (or lesser, which we don't support)
    .default_len = Gb,
#endif

#ifdef __linux__
    .create = create_mapping_ondemand,
    .reclaim_pages = punch_hole,
#endif
};

#ifdef __linux__
const MappingKind *mapping_kind_pdefault = &MAPPING_KIND_ONDEMAND;
#else
const MappingKind *mapping_kind_pdefault = &MAPPING_KIND_PORTABLE;
#endif
