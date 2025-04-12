/*
 * mempool.c
 */

#include "config/common.h"
#include "trn/mempool.h"

#include "trn/final.h"
#include "trn/util.h"
#include "util/util2.h"

/* any of these defines can be increased arbitrarily */
enum
{
    MAX_MEM_POOLS = 16,
    MAX_MEM_FRAGS = 4000,
    FRAG_SIZE = ((1024 * 64) - 32) /* number of bytes in a fragment */
};

struct MP_FRAG
{
    char *data;
    char *lastfree;
    long bytesfree;
    int next;
};

/* structure for extensibility */
struct MP_HEAD
{
    int current; /* index into mp_frag of most recent alloc */
};

static MP_FRAG s_mpfrags[MAX_MEM_FRAGS]{}; /* zero is unused */
static int     s_mp_first_free_frag{};
static MP_HEAD s_mpheads[MAX_MEM_POOLS]{};

static int mp_alloc_frag();
static void mp_free_frag(int f);

void mp_init()
{
    int i;
    for (i = 1; i < MAX_MEM_FRAGS; i++)
    {
        s_mpfrags[i].data = nullptr;
        s_mpfrags[i].lastfree = nullptr;
        s_mpfrags[i].bytesfree = 0;
        s_mpfrags[i].next = i+1;
    }
    s_mpfrags[i-1].next = 0;
    s_mp_first_free_frag = 1;   /* first free fragment */

    for (MP_HEAD &pool : s_mpheads)
    {
        pool.current = 0;
    }
}

/* returns the fragment number */
static int mp_alloc_frag()
{
    int f = s_mp_first_free_frag;
    if (f == 0) {
        printf("trn: out of memory pool fragments!\n");
        sig_catcher(0);         /* die. */
    }
    s_mp_first_free_frag = s_mpfrags[f].next;
    if (s_mpfrags[f].bytesfree)
    {
        return f;       /* already allocated */
    }
    s_mpfrags[f].data = (char*)safemalloc(FRAG_SIZE);
    s_mpfrags[f].lastfree = s_mpfrags[f].data;
    s_mpfrags[f].bytesfree = FRAG_SIZE;

    return f;
}

/* frees a fragment number */
static void mp_free_frag(int f)
{
#if 0
    /* old code to actually free the blocks */
    if (s_mpfrags[f].data)
    {
        free(s_mpfrags[f].data);
    }
    s_mpfrags[f].lastfree = nullptr;
    s_mpfrags[f].bytesfree = 0;
#else
    /* instead of freeing it, reset it for later use */
    s_mpfrags[f].lastfree = s_mpfrags[f].data;
    s_mpfrags[f].bytesfree = FRAG_SIZE;
#endif
    s_mpfrags[f].next = s_mp_first_free_frag;
    s_mp_first_free_frag = f;
}

char *mp_savestr(const char *str, memory_pool pool)
{
    if (!str) {
#if 1
        printf("\ntrn: mp_savestr(nullptr,%d) error.\n",pool);
        TRN_ASSERT(false);
#else
        return nullptr;         /* only a flesh wound... (;-) */
#endif
    }
    int len = strlen(str);
    if (len >= FRAG_SIZE) {
        printf("trn: string too big (len = %d) for memory pool!\n",len);
        printf("trn: (maximum length allowed is %d)\n",FRAG_SIZE);
        sig_catcher(0);         /* die. */
    }
    int f = s_mpheads[pool].current;
    /* just to be extra safe, keep 2 bytes unused at end of block */
    if (f == 0 || len >= s_mpfrags[f].bytesfree-2) {
        int oldf = f;
        f = mp_alloc_frag();
        s_mpfrags[f].next = oldf;
        s_mpheads[pool].current = f;
    }
    char *s = s_mpfrags[f].lastfree;
    safecpy(s,str,len+1);
    s_mpfrags[f].lastfree += len+1;
    s_mpfrags[f].bytesfree -= len+1;
    return s;
}

/* returns a pool-allocated string */
char *mp_malloc(int len, memory_pool pool)
{
    if (len == 0)
    {
        len = 1;
    }
    if (len >= FRAG_SIZE) {
        printf("trn: malloc size too big (len = %d) for memory pool!\n",len);
        printf("trn: (maximum length allowed is %d)\n",FRAG_SIZE);
        sig_catcher(0);         /* die. */
    }
    int f = s_mpheads[pool].current;
    if (f == 0 || len >= s_mpfrags[f].bytesfree) {
        int oldf = f;
        f = mp_alloc_frag();
        s_mpfrags[f].next = oldf;
        s_mpheads[pool].current = f;
    }
    char *s = s_mpfrags[f].lastfree;
    s_mpfrags[f].lastfree += len+1;
    s_mpfrags[f].bytesfree -= len+1;
    return s;
}

/* free a whole memory pool */
void mp_free(memory_pool pool)
{
    int f = s_mpheads[pool].current;
    while (f) {
        int oldnext = s_mpfrags[f].next;
        mp_free_frag(f);
        f = oldnext;
    }
    s_mpheads[pool].current = 0;
}
