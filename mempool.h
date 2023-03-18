/*
 * mempool.h
 */

/* memory pool numbers */

enum
{
    MP_SCORE1 = 0,  /* scoring rule text */
    MP_SCORE2 = 1,  /* scorefile cache */
    MP_SATHREAD = 2 /* sathread.c storage */
};

void mp_init();
char *mp_savestr(char *, int);
char *mp_malloc(int, int);
void mp_free(int);
