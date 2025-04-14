/* trn/list.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_LIST_H
#define TRN_LIST_H

#include "trn/enum-flags.h"

#include <cstdint>

enum list_flags : std::uint8_t
{
    LF_NONE = 0x0,
    LF_ZERO_MEM = 0x1,
    LF_SPARSE = 0x2
};
DECLARE_FLAGS_ENUM(list_flags, std::uint8_t);

struct LISTNODE
{
    LISTNODE *next;
    long      low;
    long      high;
    char     *data_high;
    char      data[1]; /* this is actually longer */
};

struct LIST
{
    LISTNODE *first;
    LISTNODE *recent;
    void (*init_node)(LIST *, LISTNODE *);
    long       low;
    long       high;
    int        item_size;
    int        items_per_node;
    list_flags flags;
};

void list_init();
LIST *new_list(long low, long high, int item_size, int items_per_node, list_flags flags, void (*init_node)(LIST *, LISTNODE *));
char *listnum2listitem(LIST *list, long num);
long listitem2listnum(LIST *list, char *ptr);
bool walk_list(LIST *list, bool (*callback)(char *, int), int arg);
long existing_listnum(LIST *list, long num, int direction);
char *next_listitem(LIST *list, char *ptr);
char *prev_listitem(LIST *list, char *ptr);
void delete_list(LIST *list);

#endif
