/* trn/addng.h
 */
// This software is copyrighted as detailed in the LICENSE file.

#ifndef TRN_ADDNG_H
#define TRN_ADDNG_H

#include <config/typedef.h>

#include "trn/enum-flags.h"

#include <cstdint>

struct DataSource;

enum AddGroupFlags : std::uint8_t
{
    AGF_NONE = 0x00,
    AGF_SEL = 0x01,
    AGF_DEL = 0x02,
    AGF_DEL_SEL = 0x04,
    AGF_INCLUDED = 0x10,
    AGF_EXCLUDED = 0x20
};
DECLARE_FLAGS_ENUM(AddGroupFlags, std::uint8_t);

struct AddGroup
{
    AddGroup     *next;
    AddGroup     *prev;
    DataSource   *data_src;
    ArticleNum    to_read; // number of articles to be read (for sorting)
    NewsgroupNum  num;     // a possible sort order for this group
    AddGroupFlags flags;
    char          name[1];
};

extern AddGroup *g_first_add_group;
extern AddGroup *g_last_add_group;
extern AddGroup *g_sel_page_gp;
extern AddGroup *g_sel_next_gp;
extern bool      g_quick_start;      // -q
extern bool      g_use_add_selector; //

void add_ng_init();
bool find_new_groups();
bool scan_active(bool add_matching);
void sort_add_groups();

#endif
