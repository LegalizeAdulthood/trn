/* trn/univ.h
*/
/* Universal selector
 *
 */
#ifndef TRN_UNIV_H
#define TRN_UNIV_H

#include <config/typedef.h>

#include "trn/enum-flags.h"
#include "trn/help.h"

#include <cstdint>
#include <string>

struct HASHTABLE;

enum UniversalItemType
{
    UN_NONE = 0,          //
    UN_TXT = 1,           /* textual placeholder */
    UN_DATASRC = 2,       //
    UN_NEWSGROUP = 3,     //
    UN_GROUPMASK = 4,     //
    UN_ARTICLE = 5,       /* an individual article */
    UN_CONFIGFILE = 6,    /* filename for a configuration file */
    UN_VIRTUAL1 = 7,      /* Virtual newsgroup file (reserved for compatability with strn) */
    UN_VGROUP = 8,        /* virtual newsgroup marker (for pass 2) */
    UN_TEXTFILE = 9,      /* text file */
    UN_HELPKEY = 10,      /* keystroke help functions from help.c */
    UN_DEBUG1 = -1,       /* quick debugging: just has data */
    UN_GROUP_DESEL = -2,  /* group that is deselected (with !group) */
    UN_VGROUP_DESEL = -3, /* virtual newsgroup deselected (with !group) */
    UN_DELETED = -4       /* generic deleted item -- no per-item memory */
};

struct UniversalGroupMaskData
{
    char* title;
    char* masklist;
};

struct UniversalConfigFileData
{
    char* title;
    char* fname;
    char* label;
};

struct UniversalVirtualData
{
    char* ng;
    char* id;
    char* from;
    char* subj;
    ART_NUM num;
};

/* virtual/merged group flags (UNIV_VIRT_GROUP.flags) */
enum VirtualGroupFlags : std::uint8_t
{
    UF_VG_NONE = 0x00,
    UF_VG_MINSCORE = 0x01, /* articles use minimum score */
    UF_VG_MAXSCORE = 0x02  /* articles use maximum score */
};
DECLARE_FLAGS_ENUM(VirtualGroupFlags, std::uint8_t);

struct UniversalVirtualGroup
{
    char           *ng;
    int             minscore;
    int             maxscore;
    VirtualGroupFlags flags;
};

struct UniversalNewsgroup
{
    char* ng;
};

struct UniversalTextFile
{
    char* fname;
};

union UNIV_DATA
{
    char* str;
    HelpLocation i;
    UniversalGroupMaskData gmask;
    UniversalConfigFileData cfile;
    UniversalNewsgroup group;
    UniversalVirtualData virt;
    UniversalVirtualGroup vgroup;
    UniversalTextFile textfile;
};

/* selector flags */
enum univitem_flags
{
    UF_NONE = 0x00,
    UF_SEL = 0x01,
    UF_DEL = 0x02,
    UF_DELSEL = 0x04,
    UF_INCLUDED = 0x10,
    UF_EXCLUDED = 0x20
};
DECLARE_FLAGS_ENUM(univitem_flags, int);

struct UNIV_ITEM
{
    UNIV_ITEM     *next;
    UNIV_ITEM     *prev;
    int            num;   /* natural order (for sort) */
    univitem_flags flags; /* for selector */
    UniversalItemType type;  /* what kind of object is it? */
    char          *desc;  /* default description */
    int            score;
    UNIV_DATA      data; /* describes the object */
};

extern int g_univ_level;          /* How deep are we in the tree? */
extern bool g_univ_ng_virtflag;   /* if true, we are in the "virtual group" second pass */
extern bool g_univ_read_virtflag; /* if true, we are reading an article from a "virtual group" */
extern bool g_univ_default_cmd;   /* "follow"-related stuff (virtual groups) */
extern bool g_univ_follow;
extern bool g_univ_follow_temp;

/* items which must be saved in context */
extern UNIV_ITEM *g_first_univ;
extern UNIV_ITEM *g_last_univ;
extern UNIV_ITEM *sel_page_univ;
extern UNIV_ITEM *g_sel_next_univ;
extern char *g_univ_fname;    /* current filename (may be null) */
extern std::string g_univ_label;    /* current label (may be empty) */
extern std::string g_univ_title;    /* title of current level */
extern std::string g_univ_tmp_file; /* temp. file (may be empty) */
extern HASHTABLE *g_univ_ng_hash;
extern HASHTABLE *g_univ_vg_hash;
/* end of items that must be saved */

void        univ_init();
void        univ_startup();
void        univ_open();
void        univ_close();
UNIV_ITEM * univ_add(UniversalItemType type, const char *desc);
char *      univ_desc_line(UNIV_ITEM *ui, int linenum);
void        univ_add_text(const char *txt);
void        univ_add_debug(const char *desc, const char *txt);
void        univ_add_group(const char *desc, const char *grpname);
void        univ_add_mask(const char *desc, const char *mask);
void        univ_add_file(const char *desc, const char *fname, const char *label);
UNIV_ITEM * univ_add_virt_num(const char *desc, const char *grp, ART_NUM art);
void        univ_add_textfile(const char *desc, char *name);
void        univ_add_virtgroup(const char *grpname);
void        univ_use_pattern(const char *pattern, int type);
void        univ_use_group_line(char *line, int type);
bool        univ_file_load(const char *fname, const char *title, const char *label);
void        univ_mask_load(char *mask, const char *title);
void        univ_redofile();
void        univ_edit();
void        univ_page_file(char *fname);
void        univ_ng_virtual();
int         univ_visit_group_main(const char *gname);
void        univ_virt_pass();
void        sort_univ();
const char *univ_article_desc(const UNIV_ITEM *ui);
void        univ_help_main(HelpLocation where);
void        univ_help(HelpLocation where);
const char *univ_keyhelp_modestr(const UNIV_ITEM *ui);

#endif
