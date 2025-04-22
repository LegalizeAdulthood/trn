/* trn/list.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_LIST_H
#define TRN_LIST_H

#include "trn/enum-flags.h"

#include <cstdint>

enum ListFlags : std::uint8_t
{
    LF_NONE = 0x0,
    LF_ZERO_MEM = 0x1,
    LF_SPARSE = 0x2
};
DECLARE_FLAGS_ENUM(ListFlags, std::uint8_t);

struct ListNode
{
    ListNode *next;
    long      low;
    long      high;
    char     *data_high;
    char      data[1]; // this is actually longer
};

struct List
{
    ListNode *first;
    ListNode *recent;
    void (*init_node)(List *, ListNode *);
    long      low;  // TODO: ArticleNum
    long      high; // TODO: ArticleNum
    int       item_size;
    int       items_per_node;
    ListFlags flags;
};

void list_init();
List *new_list(long low, long high, int item_size, int items_per_node, ListFlags flags, void (*init_node)(List *, ListNode *));
char *list_get_item(List *list, long index);
long list_get_index(List *list, char *item);
bool walk_list(List *list, bool (*callback)(char *, int), int arg);
long existing_list_index(List *list, long index, int direction);
char *next_list_item(List *list, char *ptr);
char *prev_list_item(List *list, char *ptr);
void delete_list(List *list);

#endif
