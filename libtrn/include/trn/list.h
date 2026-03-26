/* trn/list.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_LIST_H
#define TRN_LIST_H

#include <trn/enum-flags.h>

#include <cstdint>
#include <cstdlib>

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

struct List;

using ListNodeInit = void(List *list, ListNode *node);
using ListCallback = bool(char *, int);

struct List
{
    char *list_get_item(long index);
    bool  walk_list(ListCallback *callback, int arg);
    long  existing_list_index(long index, int direction);
    char *next_list_item(char *ptr);
    char *prev_list_item(char *ptr);
    void  delete_list();

    ListNode     *m_first;
    ListNode     *m_recent;
    ListNodeInit *m_init_node;
    long          m_low;  // TODO: ArticleNum
    long          m_high; // TODO: ArticleNum
    int           m_item_size;
    int           m_items_per_node;
    ListFlags     m_flags;
};

void list_init();
List *new_list(long low, long high, int item_size, int items_per_node, ListFlags flags, ListNodeInit *init_node);
inline void delete_list(List *list)
{
    list->delete_list();
    std::free((char*)list);
}

#endif
