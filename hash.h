/*
 * general-purpose in-core hashing
 */
/* This file is an altered version of a set of hash routines by
 * Geoffrey Collyer.  See hash.c for his copyright.
 */

struct HASHDATUM
{
    char* dat_ptr;
    unsigned dat_len;
};

using HASHCMPFUNC = int (*)(char *, int, HASHDATUM);

#define HASH_DEFCMPFUNC (HASHCMPFUNC) nullptr

HASHTABLE *hashcreate(unsigned size, int (*cmpfunc)(char *, int, HASHDATUM));
void hashdestroy(HASHTABLE *tbl);
void hashstore(HASHTABLE *tbl, char *key, int keylen, HASHDATUM data);
void hashdelete(HASHTABLE *tbl, char *key, int keylen);
HASHDATUM hashfetch(HASHTABLE *tbl, char *key, int keylen);
void hashstorelast(HASHDATUM data);
void hashwalk(HASHTABLE *tbl, int (*nodefunc)(int, HASHDATUM *, int), int extra);
