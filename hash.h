/*
 * general-purpose in-core hashing
 */
/* This file is an altered version of a set of hash routines by
 * Geoffrey Collyer.  See hash.c for his copyright.
 */

struct hashdatum {
    char* dat_ptr;
    unsigned dat_len;
};

#define HASH_DEFCMPFUNC (int (*)(char *, int, HASHDATUM)) nullptr

HASHTABLE *hashcreate(unsigned, int (*)(char *, int, HASHDATUM));
void hashdestroy(HASHTABLE *);
void hashstore(HASHTABLE *, char *, int, HASHDATUM);
void hashdelete(HASHTABLE *, char *, int);
HASHDATUM hashfetch(HASHTABLE *, char *, int);
void hashstorelast(HASHDATUM);
void hashwalk(HASHTABLE *, int (*)(int, HASHDATUM *, int), int);
