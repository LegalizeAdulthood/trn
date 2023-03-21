/*
 * mempool.h
 */
#ifndef MEMPOOL_H
#define MEMPOOL_H

/* memory pool numbers */

enum
{
    MP_SCORE1 = 0,  /* scoring rule text */
    MP_SCORE2 = 1,  /* scorefile cache */
    MP_SATHREAD = 2 /* sathread.c storage */
};

void mp_init();
char *mp_savestr(const char *str, int pool);
char *mp_malloc(int len, int pool);
void mp_free(int pool);

#endif
