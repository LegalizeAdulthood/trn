/* This file Copyright 1992 by Clifford A. Adams */
/* sathread.c
 *
 */

#include "common.h"
#include "sathread.h"

#include "cache.h"
#include "hash.h"
#include "mempool.h"
#include "sadesc.h" /* sa_desc_subject() */
#include "samisc.h"
#include "sorder.h"

static long s_sa_num_threads{};
static HASHTABLE *s_sa_thread_hash{};

void sa_init_threads()
{
    mp_free(MP_SATHREAD);
    s_sa_num_threads = 0;
    if (s_sa_thread_hash) {
	hashdestroy(s_sa_thread_hash);
	s_sa_thread_hash = 0;
    }
}

/* called only if the macro didn't find a value */
/* XXX: dependent on hash feature that data.dat_len is not used in
 * the default comparison function, so it can be used for a number.
 * later: write a custom comparison function.
 */
//long e;			/* entry number */
long sa_get_subj_thread(long e)
{
    bool old_untrim = g_untrim_cache;
    g_untrim_cache = true;
    const char *s = sa_desc_subject(e);
    g_untrim_cache = old_untrim;

    if (!s || !*s)
      return -2;
    if ((*s == '>') && (s[1] == ' '))
	s += 2;

    if (!s_sa_thread_hash) {
	s_sa_thread_hash = hashcreate(401, HASH_DEFCMPFUNC);
    }
    HASHDATUM data = hashfetch(s_sa_thread_hash, s, strlen(s));
    if (data.dat_ptr) {
	return (long)(data.dat_len);
    }
    char *p = mp_savestr(s, MP_SATHREAD);
    data = hashfetch(s_sa_thread_hash,p,strlen(s));
    data.dat_ptr = p;
    data.dat_len = (unsigned)(s_sa_num_threads+1);
    hashstorelast(data);
    s_sa_num_threads++;
    g_sa_ents[e].subj_thread_num = s_sa_num_threads;
    return s_sa_num_threads;
}

int sa_subj_thread_count(long a)
{
    int  count = 1;
    long b = a;

    while ((b = sa_subj_thread_next(b)) != 0)
	if (sa_basic_elig(b))
	    count++;
    return count;
}

/* returns basic_elig previous subject thread */
long sa_subj_thread_prev(long a)
{
    int j;

    int i = sa_subj_thread(a);
    while ((a = s_prev(a)) != 0) {
	if (!sa_basic_elig(a))
	    continue;
	if (!(j = g_sa_ents[a].subj_thread_num))
	    j = sa_subj_thread(a);
	if (i == j)
	    return a;
    }
    return 0L;
}

long sa_subj_thread_next(long a)
{
    int j;

    int i = sa_subj_thread(a);
    while ((a = s_next(a)) != 0) {
	if (!sa_basic_elig(a))
	    continue;
	if (!(j = g_sa_ents[a].subj_thread_num))
	    j = sa_subj_thread(a);
	if (i == j)
	    return a;
    }
    return 0L;
}
