/*
 * trn/mempool.h
 */
#ifndef TRN_MEMPOOL_H
#define TRN_MEMPOOL_H

// memory pool numbers

enum MemoryPool
{
    MP_SCORE1 = 0,  // scoring rule text
    MP_SCORE2 = 1,  // score file cache
    MP_SATHREAD = 2 // sathread.c storage
};

void mp_init();
char *mp_save_str(const char *str, MemoryPool pool);
char *mp_malloc(int len, MemoryPool pool);
void mp_free(MemoryPool pool);

#endif
