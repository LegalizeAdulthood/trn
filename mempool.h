/*
 * mempool.h
 */

/* memory pool numbers */
/* scoring rule text */
#define MP_SCORE1 0

/* scorefile cache */
#define MP_SCORE2 1

/* sathread.c storage */
#define MP_SATHREAD 2

void mp_init(void);
char *mp_savestr(char *, int);
char *mp_malloc(int, int);
void mp_free(int);
