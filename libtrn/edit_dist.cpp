/* edit_dist.c
*/
/* The authors make no claims as to the fitness or correctness of this software
 * for any use whatsoever, and it is provided as is. Any use of this software
 * is at the user's own risk.
 */

#include "config/common.h"             // Declare MemorySize
#include "trn/edit_dist.h"

#include "trn/util.h"               // Declare safe_malloc()

#include <algorithm>
#include <cstdlib>

// edit_dist -- returns the minimum edit distance between two strings
//
//        Program by:  Mark Maimone   CMU Computer Science   13 Nov 89
//        Last Modified:  28 Jan 90
//
//   If the input strings have length n and m, the algorithm runs in time
//   O(nm) and space O(min(m,n)).
//
// HISTORY
//   13 Nov 89 (mwm) Created edit_dist().
//
//   17 May 93 (mwm) Improved performance when used with trn's newsgroup
//   processing; assume all costs are 1, and you can terminate when a
//   threshold is exceeded.
//


#define TRN_SPEEDUP             // Use a less-general version of the
                                // routine, one that's better for trn.
                                // All change costs are 1, and it's okay
                                // to terminate if the edit distance is
                                // known to exceed MIN_DIST

#define THRESHOLD 4000          // worry about allocating more memory only
                                // when this # of bytes is exceeded
#define STRLENTHRESHOLD ((int) ((THRESHOLD / sizeof (int) - 3) / 2))

namespace
{

void swap_int(int &x, int &y)
{
    int tmp = x;
    x = y;
    y = tmp;
}

void swap_char(const char *&x, const char *&y)
{
    const char *tmp = x;
    x = y;
    y = tmp;
}

template <typename T>
T min3(T x, T y, T z)
{
    return std::min(std::min(x, y), z);
}

} // namespace

static int s_insert_cost = 1;
static int s_delete_cost = 1;

// edit_distn -- returns the edit distance between two strings, or -1 on failure

int edit_distn(const char *from, int from_len, const char *to, int to_len)
{
#ifndef TRN_SPEEDUP
    int ins;
    int del;
    int ch;
#endif
    int row;
    int col;
    int index;
    int radix;                  // radix for modular indexing
#ifdef TRN_SPEEDUP
    int low;
#endif
    int* buffer;                // pointer to storage for one row
                                //         of the d.p. array
    static int store[THRESHOLD / sizeof (int)];
                                        // a small amount of static
                                        // storage, to be used when the
                                        // input strings are small enough

// Handle trivial cases when one string is empty

    if (from == nullptr || !from_len)
    {
        if (to == nullptr || !to_len)
        {
            return 0;
        }
        return to_len * s_insert_cost;
    }
    if (to == nullptr || !to_len)
    {
        return from_len * s_delete_cost;
    }

    // Initialize registers

    radix = 2 * from_len + 3;
#ifdef TRN_SPEEDUP
#define ins 1
#define del 1
#define ch 1
#define swap_cost 1
#else
    ins  = s_insert_cost;
    del  = s_delete_cost;
    ch   = change_cost;
#endif

// Make from short enough to fit in the static storage, if it's at all possible

    if (from_len > to_len && from_len > STRLENTHRESHOLD)
    {
        swap_int(from_len, to_len);
        swap_char(from, to);
#ifndef TRN_SPEEDUP
        swap_int(ins, del);
#endif
    } // if from_len > to_len

// Allocate the array storage (from the heap if necessary)

    if (from_len <= STRLENTHRESHOLD)
    {
        buffer = store;
    }
    else
    {
        buffer = (int *) safe_malloc((MemorySize) radix * sizeof(int));
    }

    // Here's where the fun begins.  We will find the minimum edit distance
    // using dynamic programming.  We only need to store two rows of the matrix
    // at a time, since we always progress down the matrix.  For example,
    // given the strings "one" and "two", and insert, delete and change costs
    // equal to 1:
    //
    //         _  o  n  e
    //      _  0  1  2  3
    //      t  1  1  2  3
    //      w  2  2  2  3
    //      o  3  2  3  3
    //
    // The dynamic programming recursion is defined as follows:
    //
    //      ar(x,0) := x * s_insert_cost
    //      ar(0,y) := y * s_delete_cost
    //      ar(x,y) := min(a(x - 1, y - 1) + (from[x] == to[y] ? 0 : change),
    //                     a(x - 1, y) + s_insert_cost,
    //                     a(x, y - 1) + s_delete_cost,
    //                     a(x - 2, y - 2) + (from[x] == to[y-1] &&
    //                                        from[x-1] == to[y] ? swap_cost :
    //                                        infinity))
    //
    // Since this only looks at most two rows and three columns back, we need
    // only store the values for the two preceding rows.  In this
    // implementation, we do not explicitly store the zero column, so only 2 *
    // from_len + 2   words are needed.  However, in the implementation of the
    // swap_cost   check, the current matrix value is used as a buffer; we
    // can't overwrite the earlier value until the   swap_cost   check has
    // been performed.  So we use   2 * from_len + 3   elements in the buffer.
    //

#define ar(x,y,index) (((x) == 0) ? (y) * del : (((y) == 0) ? (x) * ins : \
        buffer[mod(index)]))
#define NW(x,y)   ar(x, y, index + from_len + 2)
#define N(x,y)    ar(x, y, index + from_len + 3)
#define W(x,y)    ar(x, y, index + radix - 1)
#define NNWW(x,y) ar(x, y, index + 1)
#define mod(x) ((x) % radix)

    index = 0;

#ifdef DEBUG_EDITDIST
    printf("      ");
    for (col = 0; col < from_len; col++)
    {
        printf(" %c ", from[col]);
    }
    printf("\n   ");

    for (col = 0; col <= from_len; col++)
    {
        printf("%2d ", col * del);
    }
#endif

// Row 0 is handled implicitly; its value at a given column is   col*del.
// The loop below computes the values for Row 1.  At this point we know the
// strings are nonempty.  We also don't need to consider swap costs in row
// 1.
//
// COMMENT:  the indices   row and col   below point into the STRING, so
// the corresponding MATRIX indices are   row+1 and col+1.
//

    buffer[index++] = std::min(ins + del, (from[0] == to[0] ? 0 : ch));
#ifdef TRN_SPEEDUP
    low = buffer[mod(index + radix - 1)];
#endif

#ifdef DEBUG_EDITDIST
    printf("\n %c %2d %2d ", to[0], ins, buffer[index - 1]);
#endif

    for (col = 1; col < from_len; col++)
    {
        buffer[index] = min3(
                col * del + ((from[col] == to[0]) ? 0 : ch),
                (col + 1) * del + ins,
                buffer[index - 1] + del);
#ifdef TRN_SPEEDUP
        low = std::min(buffer[index], low);
#endif
        index++;

#ifdef DEBUG_EDITDIST
        printf("%2d ", buffer[index - 1]);
#endif

    } // for col = 1

#ifdef DEBUG_EDITDIST
    printf("\n %c %2d ", to[1], 2 * ins);
#endif

// Now handle the rest of the matrix

    for (row = 1; row < to_len; row++)
    {
        for (col = 0; col < from_len; col++)
        {
            buffer[index] = min3(
                    NW(row, col) + ((from[col] == to[row]) ? 0 : ch),
                    N(row, col + 1) + ins,
                    W(row + 1, col) + del);
            if (from[col] == to[row - 1] && col > 0 && from[col - 1] == to[row])
            {
                buffer[index] = std::min(buffer[index], NNWW(row - 1, col - 1) + swap_cost);
            }

#ifdef DEBUG_EDITDIST
            printf("%2d ", buffer[index]);
#endif
#ifdef TRN_SPEEDUP
            if (buffer[index] < low || col == 0)
            {
                low = buffer[index];
            }
#endif

            index = mod(index + 1);
        } // for col = 1
#ifdef DEBUG_EDITDIST
        if (row < to_len - 1)
        {
            printf("\n %c %2d ", to[row+1], (row + 2) * ins);
        }
        else
        {
            printf("\n");
        }
#endif
#ifdef TRN_SPEEDUP
        if (low > MIN_DIST)
        {
            break;
        }
#endif
    } // for row = 1

    row = buffer[mod(index + radix - 1)];
    if (buffer != store)
    {
        std::free((char *) buffer);
    }
    return row;
} // edit_distn
