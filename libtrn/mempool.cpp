/*
 * mempool.c
 */

#include "config/common.h"
#include "trn/mempool.h"

#include "trn/final.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// any of these defines can be increased arbitrarily
enum
{
    MAX_MEM_POOLS = 16,
    MAX_MEM_FRAGS = 4000,
    FRAG_SIZE = ((1024 * 64) - 32) // number of bytes in a fragment
};

struct MemoryPoolFragment
{
    char *data;
    char *last_free;
    long  bytes_free;
    int   next;
};

// structure for extensibility
struct MemoryPoolHead
{
    int current; // index into mp_frag of most recent alloc
};

static MemoryPoolFragment s_mp_frags[MAX_MEM_FRAGS]{}; // zero is unused
static int                s_mp_first_free_frag{};
static MemoryPoolHead     s_mp_heads[MAX_MEM_POOLS]{};

static int mp_alloc_frag();
static void mp_free_frag(int f);

void mp_init()
{
    int i;
    for (i = 1; i < MAX_MEM_FRAGS; i++)
    {
        s_mp_frags[i].data = nullptr;
        s_mp_frags[i].last_free = nullptr;
        s_mp_frags[i].bytes_free = 0;
        s_mp_frags[i].next = i+1;
    }
    s_mp_frags[i-1].next = 0;
    s_mp_first_free_frag = 1;   // first free fragment

    for (MemoryPoolHead &pool : s_mp_heads)
    {
        pool.current = 0;
    }
}

// returns the fragment number
static int mp_alloc_frag()
{
    int f = s_mp_first_free_frag;
    if (f == 0)
    {
        std::printf("trn: out of memory pool fragments!\n");
        sig_catcher(0);         // die.
    }
    s_mp_first_free_frag = s_mp_frags[f].next;
    if (s_mp_frags[f].bytes_free)
    {
        return f;       // already allocated
    }
    s_mp_frags[f].data = (char*)safe_malloc(FRAG_SIZE);
    s_mp_frags[f].last_free = s_mp_frags[f].data;
    s_mp_frags[f].bytes_free = FRAG_SIZE;

    return f;
}

// frees a fragment number
static void mp_free_frag(int f)
{
#if 0
    // old code to actually free the blocks
    if (s_mpfrags[f].data)
    {
        std::free(s_mpfrags[f].data);
    }
    s_mpfrags[f].lastfree = nullptr;
    s_mpfrags[f].bytesfree = 0;
#else
    // instead of freeing it, reset it for later use
    s_mp_frags[f].last_free = s_mp_frags[f].data;
    s_mp_frags[f].bytes_free = FRAG_SIZE;
#endif
    s_mp_frags[f].next = s_mp_first_free_frag;
    s_mp_first_free_frag = f;
}

char *mp_save_str(const char *str, MemoryPool pool)
{
    if (!str)
    {
#if 1
        std::printf("\ntrn: mp_savestr(nullptr,%d) error.\n",pool);
        TRN_ASSERT(false);
#else
        return nullptr;         // only a flesh wound... (;-)
#endif
    }
    int len = std::strlen(str);
    if (len >= FRAG_SIZE)
    {
        std::printf("trn: string too big (len = %d) for memory pool!\n",len);
        std::printf("trn: (maximum length allowed is %d)\n",FRAG_SIZE);
        sig_catcher(0);         // die.
    }
    int f = s_mp_heads[pool].current;
    // just to be extra safe, keep 2 bytes unused at end of block
    if (f == 0 || len >= s_mp_frags[f].bytes_free - 2)
    {
        int oldf = f;
        f = mp_alloc_frag();
        s_mp_frags[f].next = oldf;
        s_mp_heads[pool].current = f;
    }
    char *s = s_mp_frags[f].last_free;
    safe_copy(s,str,len+1);
    s_mp_frags[f].last_free += len+1;
    s_mp_frags[f].bytes_free -= len+1;
    return s;
}

// returns a pool-allocated string
char *mp_malloc(int len, MemoryPool pool)
{
    if (len == 0)
    {
        len = 1;
    }
    if (len >= FRAG_SIZE)
    {
        std::printf("trn: malloc size too big (len = %d) for memory pool!\n",len);
        std::printf("trn: (maximum length allowed is %d)\n",FRAG_SIZE);
        sig_catcher(0);         // die.
    }
    int f = s_mp_heads[pool].current;
    if (f == 0 || len >= s_mp_frags[f].bytes_free)
    {
        int oldf = f;
        f = mp_alloc_frag();
        s_mp_frags[f].next = oldf;
        s_mp_heads[pool].current = f;
    }
    char *s = s_mp_frags[f].last_free;
    s_mp_frags[f].last_free += len+1;
    s_mp_frags[f].bytes_free -= len+1;
    return s;
}

// free a whole memory pool
void mp_free(MemoryPool pool)
{
    int f = s_mp_heads[pool].current;
    while (f)
    {
        int oldnext = s_mp_frags[f].next;
        mp_free_frag(f);
        f = oldnext;
    }
    s_mp_heads[pool].current = 0;
}
