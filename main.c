#include "dumb_alloc.h"
#include "align.h"
#include "mapping_kind.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

//------------------------------------------------------------------------------

#define PTH_CHECK(Expr_) pth_check_impl(Expr_, #Expr_, __FILE__, __LINE__)

static
void
pth_check_impl(int ret, const char *expr, const char *file, int line)
{
    if (ret == 0)
        return;
    fprintf(stderr, "PTH_CHECK(%s) failed at %s:%d: %s\n", expr, file, line, strerror(ret));
    abort();
}

//------------------------------------------------------------------------------

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    char parentsturn;
} Preface;

static
void
preface_init(Preface *p)
{
    pthread_mutexattr_t mtx_attr;
    PTH_CHECK(pthread_mutexattr_init(&mtx_attr));
    PTH_CHECK(pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED));
    PTH_CHECK(pthread_mutex_init(&p->mtx, &mtx_attr));

    pthread_condattr_t cond_attr;
    PTH_CHECK(pthread_condattr_init(&cond_attr));
    PTH_CHECK(pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED));
    PTH_CHECK(pthread_cond_init(&p->cond, &cond_attr));

    p->parentsturn = 1;
}

//------------------------------------------------------------------------------

static size_t pagesize;

static
void
memmgr_init(void)
{
#ifdef _SC_PAGESIZE
    long r = sysconf(_SC_PAGESIZE);
#else
    long r = sysconf(_SC_PAGE_SIZE);
#endif
    if (r < 0) {
        perror("sysconf");
        abort();
    }
    pagesize = r;
}

typedef struct {
    void *addr;
    size_t len;
    int (*reclaim_pages)(void *addr, size_t len);
} Mapping;

static
void
memshrink(Mapping *m, size_t newnmem)
{
    if (!m->reclaim_pages)
        return;

    // Prevent overflow.
    if (newnmem > SIZE_MAX - pagesize)
        return;

    const size_t addr_off = ALIGN_TO(newnmem, pagesize);

    const size_t len = m->len;
    if (addr_off > len)
        return;

    const size_t reclaim_len = len - addr_off;
    if (reclaim_len < pagesize)
        return;

    if (m->reclaim_pages((char *) m->addr + addr_off, reclaim_len) < 0) {
        perror("reclaim_pages");
        abort();
    }
}

static
bool
setnmem(void *userdata, size_t oldnmem, size_t newnmem)
{
    Mapping *m = userdata;
    if (oldnmem > newnmem) {
        memshrink(m, newnmem);
        return true;
    } else {
        return m->len - ALIGN_TO_DUMB(sizeof(Preface)) >= newnmem;
    }
}

static
DumbAllocData
make_dumb_alloc_data(Mapping *m)
{
    return (DumbAllocData) {
        .mem = (char *) m->addr + ALIGN_TO_DUMB(sizeof(Preface)),
        .userdata = m,
        .setnmem = setnmem,
    };
}

static
void *
l_realloc(void *userdata, void *ptr, size_t oldn, size_t newn)
{
    (void) oldn;
    return dumb_realloc(make_dumb_alloc_data(userdata), ptr, newn);
}

//------------------------------------------------------------------------------

static
const char *
get_lua_errmsg(lua_State *L, int pos)
{
    const char *msg = lua_tostring(L, pos);
    return msg ? msg : "(threw an error object that cannot be converted to string)";
}

static
bool
check_lua_error(lua_State *L, int ret)
{
    if (ret == 0)
        return true;
    fprintf(stderr, "Lua: %s\n", get_lua_errmsg(L, -1));
    return false;
}

static
int
l_panicf(lua_State *L)
{
    fprintf(stderr, "Lua PANIC: %s\n", get_lua_errmsg(L, -1));
    return 0;
}

static
int
l_getpid(lua_State *L)
{
    lua_pushinteger(L, getpid());
    return 1;
}

static
int
l_sleep(lua_State *L)
{
    const double arg = luaL_checknumber(L, 1);
    struct timespec req = {.tv_sec = arg};
    req.tv_nsec = (arg - req.tv_sec) * 1e9;

    struct timespec rem;
    int r;
    while ((r = nanosleep(&req, &rem)) < 0 && errno == EINTR)
        req = rem;

    if (r < 0)
        perror("nanosleep");
    return 0;
}

static
void
inject_lib(lua_State *L)
{
    lua_newtable(L); // L: table

    lua_pushcfunction(L, l_getpid); // L: table l_getpid
    lua_setfield(L, -2, "getpid"); // L: table

    lua_pushcfunction(L, l_sleep); // L: table l_sleep
    lua_setfield(L, -2, "sleep"); // L: table

    lua_setglobal(L, "shm_poc"); // L: -
}

//------------------------------------------------------------------------------

static pid_t sibling;

static
void
handle_signal(int signo)
{
    int saved_errno = errno;
    kill(sibling, signo);
    raise(signo);
    errno = saved_errno;
}

static
void
signals_setup(void)
{
    sibling = getpid();

    struct sigaction siga = {.sa_handler = handle_signal, .sa_flags = SA_RESETHAND};
    if (sigemptyset(&siga.sa_mask) < 0) {
        perror("sigemptyset");
        abort();
    }
    if (sigaction(SIGHUP, &siga, NULL) < 0 ||
        sigaction(SIGINT, &siga, NULL) < 0 ||
        sigaction(SIGQUIT, &siga, NULL) < 0 ||
        sigaction(SIGTERM, &siga, NULL) < 0)
    {
        perror("sigaction");
        abort();
    }
}

static
void
signals_onfork(pid_t res)
{
    if (res > 0)
        sibling = res;
}

//------------------------------------------------------------------------------

static
size_t
parse_mapping_len(const char *arg)
{
    char *endptr;
    errno = 0;
    size_t r = strtoull(arg, &endptr,  10);
    if (errno || endptr == arg)
        return 0;
    switch (*endptr) {
    case 'G':
        r *= 1024;
        // fall through
    case 'M':
        r *= 1024;
        // fall through
    case 'K':
        r *= 1024;
        // fall through
    case '\0':
        break;
    default:
        return 0;
    }
    if (*endptr && endptr[1]) {
        return 0;
    }
    return r;
}

static
void
usage(void)
{
    fprintf(stderr, "USAGE: %s [{ -p | -o }] [-n MAPPING_LENGTH]\n"
                    "\n"
                    "OPTIONS:\n"
                    "    -p: create portable /dev/zero mapping.\n"
                    "    -o: create Linux-specific on-demand mapping.\n"
                    "    -n MAPPING_LENGTH: set mapping length. Supported suffixes:\n"
                    "        K for kilobytes;\n"
                    "        M for megabytes;\n"
                    "        G for gigabytes.\n"
                    ,
            PROGRAM_NAME);
    exit(2);
}

int
main(int argc, char **argv)
{
    memmgr_init();
    MappingKind m_kind = *mapping_kind_pdefault;
    Mapping m = {.len = 0};

    for (int c; (c = getopt(argc, argv, "n:po")) != -1;) {
        switch (c) {
            case 'n':
                if (!(m.len = parse_mapping_len(optarg))) {
                    fprintf(stderr, "E: invalid mapping length.\n");
                    usage();
                }
                if (m.len > SIZE_MAX / 2) {
                    fprintf(stderr, "E: mapping length is too big.\n");
                    usage();
                }
                break;
            case 'p':
                m_kind = MAPPING_KIND_PORTABLE;
                break;
            case 'o':
                m_kind = MAPPING_KIND_ONDEMAND;
                break;
            case '?':
                usage();
        }
    }
    if (optind != argc) {
        usage();
    }

    if (!m_kind.create) {
        fprintf(stderr, "E: specified mapping kind is not available on your system.\n");
        return 1;
    }
    m.reclaim_pages = m_kind.reclaim_pages;

    if (!m.len)
        m.len = m_kind.default_len;
    m.len += ALIGN_TO_DUMB(sizeof(Preface));

    if (!(m.addr = m_kind.create(m.len))) {
        perror("mmap");
        return 1;
    }

    Preface *preface = m.addr;
    preface_init(preface);
    if (!dumb_alloc_init(make_dumb_alloc_data(&m))) {
        fprintf(stderr, "E: allocated shared mapping is too small.\n");
        return 1;
    }

    lua_State *L = lua_newstate(l_realloc, &m);
    if (!L) {
        fprintf(stderr, "E: lua_newstate() failed (out of memory?)\n");
        return 1;
    }
    lua_atpanic(L, l_panicf);
    luaL_openlibs(L);
    inject_lib(L);
    if (!check_lua_error(L, luaL_loadfile(L, "demo.lua")) ||
        !check_lua_error(L, lua_pcall(L, 0, 0, 0)))
    {
        return 1;
    }

    signals_setup();

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    signals_onfork(pid);

    char isparent;
    const char *funcname;
    if (pid) {
        // parent
        isparent = 1;
        funcname = "f";
    } else {
        // child
        isparent = 0;
        funcname = "g";
    }
    while (1) {
        PTH_CHECK(pthread_mutex_lock(&preface->mtx));
        while (preface->parentsturn != isparent) {
            PTH_CHECK(pthread_cond_wait(&preface->cond, &preface->mtx));
        }

        lua_getglobal(L, funcname);
        check_lua_error(L, lua_pcall(L, 0, 0, 0));

        preface->parentsturn = !isparent;
        PTH_CHECK(pthread_cond_signal(&preface->cond));
        PTH_CHECK(pthread_mutex_unlock(&preface->mtx));
    }
}
