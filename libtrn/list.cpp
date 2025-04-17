/* list.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "trn/list.h"
#include "config/common.h"

#include "trn/util.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

static void def_init_node(List *list, ListNode *node);

void list_init()
{
}

/* Create the header for a dynamic list of items.
*/
List *new_list(long low, long high, int item_size, int items_per_node, ListFlags flags, void (*init_node)(List *, ListNode *))
{
    List* list = (List*)safe_malloc(sizeof (List));
    list->first = nullptr;
    list->recent = nullptr;
    list->init_node = init_node? init_node : def_init_node;
    list->low = low;
    list->high = high;
    list->item_size = item_size;
    list->items_per_node = items_per_node;
    list->flags = flags;

    return list;
}

/* The default way to initialize a node */
static void def_init_node(List *list, ListNode *node)
{
    if (list->flags & LF_ZERO_MEM)
    {
        std::memset(node->data, 0, list->items_per_node * list->item_size);
    }
}

/* Take the number of a list element and return its pointer.  This
** will allocate new list items as needed, keeping the list->high
** value up to date.
*/
char *list_get_item(List *list, long index)
{
    ListNode* node = list->recent;
    ListNode* prevnode = nullptr;

    if (node && index < node->low)
    {
        node = list->first;
    }
    while (true)
    {
        if (!node || index < node->low)
        {
            node = (ListNode*)safe_malloc(list->items_per_node*list->item_size
                                        + sizeof (ListNode) - 1);
            if (list->flags & LF_SPARSE)
            {
                node->low = ((index - list->low) / list->items_per_node) * list->items_per_node + list->low;
            }
            else
            {
                node->low = index;
            }
            node->high = node->low + list->items_per_node - 1;
            node->data_high = node->data
                            + (list->items_per_node - 1) * list->item_size;
            list->high = std::max(node->high, list->high);
            if (prevnode)
            {
                node->next = prevnode->next;
                prevnode->next = node;
            }
            else
            {
                node->next = list->first;
                list->first = node;
            }
            list->init_node(list, node);
            break;
        }
        if (index <= node->high)
        {
            break;
        }
        prevnode = node;
        node = node->next;
    }
    list->recent = node;
    return node->data + (index - node->low) * list->item_size;
}

/* Take the pointer of a list element and return its number.  The item
** must already exist or this will infinite loop.
*/
long list_get_index(List *list, char *item)
{
    int item_size = list->item_size;

    for (ListNode *node = list->recent;; node = node->next)
    {
        if (!node)
        {
            node = list->first;
        }
        int i = node->high - node->low + 1;
        for (char *cp = node->data; i--; cp += item_size)
        {
            if (item == cp)
            {
                list->recent = node;
                return (item - node->data) / list->item_size + node->low;
            }
        }
    }
    return -1;
}

/* Execute the indicated callback function on every item in the list.
*/
bool walk_list(List *list, bool (*callback)(char *, int), int arg)
{
    int item_size = list->item_size;

    for (ListNode *node = list->first; node; node = node->next)
    {
        int i = node->high - node->low + 1;
        for (char *cp = node->data; i--; cp += item_size)
        {
            if (callback(cp, arg))
            {
                return true;
            }
        }
    }
    return false;
}

/* Since the list can be sparsely allocated, find the nearest number
** that is already allocated, rounding in the indicated direction from
** the initial list number.
*/
long existing_list_index(List *list, long index, int direction)
{
    ListNode* node = list->recent;
    ListNode* prevnode = nullptr;

    if (node && index < node->low)
    {
        node = list->first;
    }
    while (node)
    {
        if (index <= node->high)
        {
            if (direction > 0)
            {
                index = std::max(index, node->low);
            }
            else if (direction == 0)
            {
                if (index < node->low)
                {
                    index = 0;
                }
            }
            else if (index < node->low)
            {
                if (!prevnode)
                {
                    break;
                }
                list->recent = prevnode;
                return prevnode->high;
            }
            list->recent = node;
            return index;
        }
        prevnode = node;
        node = node->next;
    }
    if (!direction)
    {
        return 0;
    }
    if (direction > 0)
    {
        return list->high + 1;
    }
    return list->low - 1;
}

/* Increment the item pointer to the next allocated item.
** Returns nullptr if ptr is the last one.
*/
char *next_list_item(List *list, char *ptr)
{
    ListNode* node = list->recent;

    if (ptr == node->data_high)
    {
        node = node->next;
        if (!node)
        {
            return nullptr;
        }
        list->recent = node;
        return node->data;
    }
#if 0
    if (node->high > list->high)
    {
        if ((ptr - node->data) / list->item_size + node->low >= list->high)
        {
            return nullptr;
        }
    }
#endif
    return ptr += list->item_size;
}

/* Decrement the item pointer to the prev allocated item.
** Returns nullptr if ptr is the first one.
*/
char *prev_list_item(List *list, char *ptr)
{
    ListNode* node = list->recent;

    if (ptr == node->data)
    {
        ListNode* prev = list->first;
        if (prev == node)
        {
            return nullptr;
        }
        while (prev->next != node)
        {
            prev = prev->next;
        }
        list->recent = prev;
        return prev->data_high;
    }
    return ptr -= list->item_size;
}

/* Delete the list and all its allocated nodes.  If you need to cleanup
** the individual nodes, call walk_list() with a cleanup function before
** calling this.
*/
void delete_list(List *list)
{
    ListNode* node = list->first;

    while (node)
    {
        ListNode *prevnode = node;
        node = node->next;
        std::free((char*)prevnode);
    }
    std::free((char*)list);
}
