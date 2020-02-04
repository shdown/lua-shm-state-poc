#include "dumb_alloc.h"
#include <stdint.h>
#include <string.h>
#include "align.h"

typedef struct {
    size_t nmem;
} Preface;

typedef struct {
    size_t m;
} Header;

static const size_t UNUSED_MASK = 1;

static
size_t
h_size_if_unused(Header h)
{
    return (h.m & UNUSED_MASK) ? (h.m & ~UNUSED_MASK) : 0;
}

static
size_t
h_size(Header h)
{
    return h.m & ~UNUSED_MASK;
}

static
void
h_mark_used(Header *h)
{
    h->m &= ~UNUSED_MASK;
}

static
void
h_mark_unused(Header *h)
{
    h->m |= UNUSED_MASK;
}

static
Header
h_create_used(size_t sz)
{
    return (Header) {.m = sz};
}

bool
dumb_alloc_init(DumbAllocData d)
{
    const size_t initnmem = ALIGN_TO_DUMB(sizeof(Preface));

    if (!d.setnmem(d.userdata, 0, initnmem))
        return false;

    Preface p = {.nmem = initnmem};
    memcpy(d.mem, &p, sizeof(p));

    return true;
}

void *
dumb_malloc(DumbAllocData d, size_t n)
{
    static const size_t HALF_ADDR_SPACE = (SIZE_MAX - ALIGN_TO_DUMB(sizeof(Header))) / 2;
    if (!n || n > HALF_ADDR_SPACE)
        return NULL;

    n = ALIGN_TO_DUMB(n);

    Preface p;
    memcpy(&p, d.mem, sizeof(p));

    char *last = ((char *) d.mem) + p.nmem;
    char *cur = ((char *) d.mem) + ALIGN_TO_DUMB(sizeof(Preface));
    while (cur != last) {
        Header h;
        memcpy(&h, cur, sizeof(h));

        const size_t unused_sz = h_size_if_unused(h);
        if (unused_sz >= n) {
            h_mark_used(&h);
            memcpy(cur, &h, sizeof(h));
            return cur + ALIGN_TO_DUMB(sizeof(Header));
        }

        cur += ALIGN_TO_DUMB(sizeof(Header));
        cur += h_size(h);
    }

    const size_t newnmem = p.nmem + ALIGN_TO_DUMB(sizeof(Header)) + n;
    if (newnmem > HALF_ADDR_SPACE)
        return NULL;
    if (!d.setnmem(d.userdata, p.nmem, newnmem))
        return NULL;

    p.nmem = newnmem;
    memcpy(d.mem, &p, sizeof(p));

    Header h = h_create_used(n);
    memcpy(last, &h, sizeof(h));
    return last + ALIGN_TO_DUMB(sizeof(Header));
}

void
dumb_free(DumbAllocData d, void *block)
{
    if (!block)
        return;

    void *hdr_addr = ((char *) block) - ALIGN_TO_DUMB(sizeof(Header));

    Header h;
    memcpy(&h, hdr_addr, sizeof(h));
    const size_t block_size = h_size(h);

    Preface p;
    memcpy(&p, d.mem, sizeof(p));
    if (
            ((char *) d.mem) + p.nmem ==
            ((char *) block) + block_size)
    {
        const size_t newnmem = p.nmem - block_size - ALIGN_TO_DUMB(sizeof(Header));
        if (d.setnmem(d.userdata, p.nmem, newnmem)) {
            p.nmem = newnmem;
            memcpy(d.mem, &p, sizeof(p));
        }

    } else {
        h_mark_unused(&h);
        memcpy(hdr_addr, &h, sizeof(h));
    }
}

void *
dumb_realloc(DumbAllocData d, void *block, size_t n)
{
    if (!block)
        return dumb_malloc(d, n);

    if (!n) {
        dumb_free(d, block);
        return NULL;
    }

    void *hdr_addr = ((char *) block) - ALIGN_TO_DUMB(sizeof(Header));
    Header h;
    memcpy(&h, hdr_addr, sizeof(h));
    const size_t block_size = h_size(h);

    if (block_size >= n)
        return block;

    void *r = dumb_malloc(d, n);
    if (!r)
        return NULL;
    memcpy(r, block, block_size);
    dumb_free(d, block);
    return r;
}
