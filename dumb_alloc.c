#include "dumb_alloc.h"
#include <stdint.h>
#include <string.h>
#include "align.h"

typedef struct {
    size_t flag_n_sz;
} Header;

#define HDR_FLAG_MASK (SIZE_MAX / 2 + 1)

#define HDR_ISUSED(H_) ((H_).flag_n_sz & HDR_FLAG_MASK)
#define HDR_SZ(H_)     ((H_).flag_n_sz & ~HDR_FLAG_MASK)

#define HDR_SETUSED(H_)   ((H_).flag_n_sz |= HDR_FLAG_MASK)
#define HDR_SETUNUSED(H_) ((H_).flag_n_sz &= ~HDR_FLAG_MASK)

#define HDR_INIT_USED(H_, Sz_) ((H_).flag_n_sz = (Sz_) | HDR_FLAG_MASK)

typedef struct {
    size_t nmem;
} Preface;

static inline
void *
find_unused(void *hdr_first, void *hdr_last, size_t n)
{
    for (char *p = hdr_first; p != hdr_last;) {
        const Header h = *(Header *) p;
        const size_t sz = HDR_SZ(h);
        if (sz >= n && !HDR_ISUSED(h)) {
            HDR_SETUSED(* (Header *) p);
            return p + ALIGN_TO_DUMB(sizeof(Header));
        }
        p += ALIGN_TO_DUMB(sizeof(Header)) + sz;
    }
    return NULL;
}

bool
dumb_alloc_init(DumbAllocData d)
{
    if (!d.setnmem(d.userdata, 0, ALIGN_TO_DUMB(sizeof(Preface)))) {
        return false;
    }
    Preface *p = d.mem;
    p->nmem = ALIGN_TO_DUMB(sizeof(Preface));
    return true;
}

void *
dumb_malloc(DumbAllocData d, size_t n)
{
    if (!n) {
        return NULL;
    }
    n = ALIGN_TO_DUMB(n);

    Preface *p = d.mem;
    const size_t nmem = p->nmem;
    char *last = ((char *) d.mem) + nmem;

    void *unused = find_unused(
        ((char *) d.mem) + ALIGN_TO_DUMB(sizeof(Preface)),
        last,
        n);
    if (unused) {
        return unused;
    }

    const size_t newnmem = nmem + ALIGN_TO_DUMB(sizeof(Header)) + n;
    if (!d.setnmem(d.userdata, nmem, newnmem)) {
        return NULL;
    }

    p->nmem = newnmem;
    HDR_INIT_USED(*(Header *) last, n);
    return last + ALIGN_TO_DUMB(sizeof(Header));
}

void
dumb_free(DumbAllocData d, void *block)
{
    if (!block) {
        return;
    }
    Header *ph = (Header *) (((char *) block) - ALIGN_TO_DUMB(sizeof(Header)));
    HDR_SETUNUSED(*ph);

    Preface *p = d.mem;
    const size_t nmem = p->nmem;
    const size_t nblock = HDR_SZ(*ph);
    if (((char *) d.mem) + nmem == ((char *) block) + nblock) {
        const size_t newnmem = nmem - nblock - ALIGN_TO_DUMB(sizeof(Header));
        if (d.setnmem(d.userdata, nmem, newnmem)) {
            p->nmem = newnmem;
        }
    }
}

void *
dumb_realloc(DumbAllocData d, void *block, size_t n)
{
    if (!block) {
        return dumb_malloc(d, n);
    }
    if (!n) {
        dumb_free(d, block);
        return NULL;
    }

    const Header h = * (Header *) (((char *) block) - ALIGN_TO_DUMB(sizeof(Header)));
    const size_t oldn = HDR_SZ(h);
    if (oldn >= n) {
        return block;
    }

    void *ret = dumb_malloc(d, n);
    if (!ret) {
        return NULL;
    }
    memcpy(ret, block, oldn);
    dumb_free(d, block);
    return ret;
}
