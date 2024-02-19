/*
 * mempool.h
 */
#ifndef TRN_MEMPOOL_H
#define TRN_MEMPOOL_H

/* memory pool numbers */

enum memory_pool
{
    MP_SCORE1 = 0,  /* scoring rule text */
    MP_SCORE2 = 1,  /* scorefile cache */
    MP_SATHREAD = 2 /* sathread.c storage */
};

void mp_init();
char *mp_savestr(const char *str, memory_pool pool);
char *mp_malloc(int len, memory_pool pool);
void mp_free(memory_pool pool);

#endif
