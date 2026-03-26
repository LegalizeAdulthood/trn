/* list.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <trn/list.h>

#include <config/common.h>
#include <trn/util.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

static void def_init_node(List *list, ListNode *node);

void list_init()
{
}

// Create the header for a dynamic list of items.
List *new_list(long low, long high, int item_size, int items_per_node, ListFlags flags, void (*init_node)(List *, ListNode *))
{
    List* list = (List*)safe_malloc(sizeof (List));
    list->m_first = nullptr;
    list->m_recent = nullptr;
    list->m_init_node = init_node? init_node : def_init_node;
    list->m_low = low;
    list->m_high = high;
    list->m_item_size = item_size;
    list->m_items_per_node = items_per_node;
    list->m_flags = flags;

    return list;
}

// The default way to initialize a node
static void def_init_node(List *list, ListNode *node)
{
    if (list->m_flags & LF_ZERO_MEM)
    {
        std::memset(node->data, 0, list->m_items_per_node * list->m_item_size);
    }
}

// Take the number of a list element and return its pointer.  This
// will allocate new list items as needed, keeping the list->high
// value up to date.
//
char *List::list_get_item(long index)
{
    ListNode * node = m_recent;
    ListNode* prevnode = nullptr;

    if (node && index < node->low)
    {
        node = m_first;
    }
    while (true)
    {
        if (!node || index < node->low)
        {
            node = (ListNode*)safe_malloc(m_items_per_node*m_item_size
                                        + sizeof (ListNode) - 1);
            if (m_flags & LF_SPARSE)
            {
                node->low = ((index - m_low) / m_items_per_node) * m_items_per_node + m_low;
            }
            else
            {
                node->low = index;
            }
            node->high = node->low + m_items_per_node - 1;
            node->data_high = node->data
                            + (m_items_per_node - 1) * m_item_size;
            m_high = std::max(node->high, m_high);
            if (prevnode)
            {
                node->next = prevnode->next;
                prevnode->next = node;
            }
            else
            {
                node->next = m_first;
                m_first = node;
            }
            m_init_node(this, node);
            break;
        }
        if (index <= node->high)
        {
            break;
        }
        prevnode = node;
        node = node->next;
    }
    m_recent = node;
    return node->data + (index - node->low) * m_item_size;
}

// Execute the indicated callback function on every item in the list.
//
bool List::walk_list(ListCallback *callback, int arg)
{
    int item_size = m_item_size;

    for (ListNode *node = m_first; node; node = node->next)
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

// Since the list can be sparsely allocated, find the nearest number
// that is already allocated, rounding in the indicated direction from
// the initial list number.
//
long List::existing_list_index(long index, int direction)
{
    ListNode * node = m_recent;
    ListNode* prevnode = nullptr;

    if (node && index < node->low)
    {
        node = m_first;
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
                m_recent = prevnode;
                return prevnode->high;
            }
            m_recent = node;
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
        return m_high + 1;
    }
    return m_low - 1;
}

// Increment the item pointer to the next allocated item.
// Returns nullptr if ptr is the last one.
//
char *List::next_list_item(char *ptr)
{
    ListNode* node = m_recent;

    if (ptr == node->data_high)
    {
        node = node->next;
        if (!node)
        {
            return nullptr;
        }
        m_recent = node;
        return node->data;
    }
    return ptr += m_item_size;
}

// Decrement the item pointer to the prev allocated item.
// Returns nullptr if ptr is the first one.
//
char *List::prev_list_item(char *ptr)
{
    ListNode * node = m_recent;

    if (ptr == node->data)
    {
        ListNode* prev = m_first;
        if (prev == node)
        {
            return nullptr;
        }
        while (prev->next != node)
        {
            prev = prev->next;
        }
        m_recent = prev;
        return prev->data_high;
    }
    return ptr -= m_item_size;
}

// Delete the list and all its allocated nodes.  If you need to cleanup
// the individual nodes, call walk_list() with a cleanup function before
// calling this.
//
void List::delete_list()
{
    ListNode *node = m_first;

    while (node)
    {
        ListNode *prev_node = node;
        node = node->next;
        std::free((char*)prev_node);
    }
}
