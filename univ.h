/* univ.h
*/
/* Universal selector
 *
 */

#define UN_NONE		0
/* textual placeholder */
#define UN_TXT		1
#define UN_DATASRC	2
#define UN_NEWSGROUP	3
#define UN_GROUPMASK	4
/* an individual article */
#define UN_ARTICLE	5
/* filename for a configuration file */
#define UN_CONFIGFILE	6
/* Virtual newsgroup file (reserved for compatability with strn) */
#define UN_VIRTUAL1	7
/* virtual newsgroup marker (for pass 2) */
#define UN_VGROUP	8
/* text file */
#define UN_TEXTFILE	9
/* keystroke help functions from help.c */
#define UN_HELPKEY	10

/* quick debugging: just has data */
#define UN_DEBUG1	-1
/* group that is deselected (with !group) */
#define UN_GROUP_DESEL  -2
/* virtual newsgroup deselected (with !group) */
#define UN_VGROUP_DESEL -3
/* generic deleted item -- no per-item memory */
#define UN_DELETED	-4

/* selector flags */
#define UF_SEL		0x01
#define UF_DEL		0x02
#define UF_DELSEL	0x04
#define UF_INCLUDED	0x10
#define UF_EXCLUDED	0x20

/* virtual/merged group flags (UNIV_VIRT_GROUP.flags) */
/* articles use minimum score */
#define UF_VG_MINSCORE	0x01
/* articles use maximum score */
#define UF_VG_MAXSCORE	0x02

struct UNIV_GROUPMASK_DATA
{
    char* title;
    char* masklist;
};

struct UNIV_CONFIGFILE_DATA
{
    char* title;
    char* fname;
    char* label;
};

struct UNIV_VIRT_DATA
{
    char* ng;
    char* id;
    char* from;
    char* subj;
    ART_NUM num;
};

struct UNIV_VIRT_GROUP
{
    char* ng;
    int minscore;
    int maxscore;
    char flags;
};

struct UNIV_NEWSGROUP
{
    char* ng;
};

struct UNIV_TEXTFILE
{
    char* fname;
};

union UNIV_DATA
{
    char* str;
    int i;
    UNIV_GROUPMASK_DATA gmask;
    UNIV_CONFIGFILE_DATA cfile;
    UNIV_NEWSGROUP group;
    UNIV_VIRT_DATA virt;
    UNIV_VIRT_GROUP vgroup;
    UNIV_TEXTFILE textfile;
};

struct UNIV_ITEM
{
    UNIV_ITEM* next;
    UNIV_ITEM* prev;
    int num;				/* natural order (for sort) */
    int flags;				/* for selector */
    int type;				/* what kind of object is it? */
    char* desc;				/* default description */
    int score;
    UNIV_DATA data;			/* describes the object */
};

/* have we ever been initialized? */
EXT int univ_ever_init;

/* How deep are we in the tree? */
EXT int univ_level;

/* if true, we are in the "virtual group" second pass */
EXT bool univ_ng_virtflag INIT(false);

/* if true, we are reading an article from a "virtual group" */
EXT bool univ_read_virtflag INIT(false);

/* "follow"-related stuff (virtual groups) */
EXT bool univ_default_cmd INIT(false);
EXT bool univ_follow INIT(true);
EXT bool univ_follow_temp INIT(false);

/* if true, the user has loaded their own top univ. config file */
EXT bool univ_usrtop;

/* items which must be saved in context */
EXT UNIV_ITEM* first_univ;
EXT UNIV_ITEM* last_univ;
EXT UNIV_ITEM* sel_page_univ;
EXT UNIV_ITEM* sel_next_univ;
EXT char* univ_fname;			/* current filename (may be null) */
EXT char* univ_label;			/* current label (may be null) */
EXT char* univ_title;			/* title of current level */
EXT char* univ_tmp_file;		/* temp. file (may be null) */
EXT HASHTABLE* univ_ng_hash INIT(nullptr);
EXT HASHTABLE* univ_vg_hash INIT(nullptr);
/* end of items that must be saved */

void univ_init();
void univ_startup();
void univ_open();
void univ_close();
UNIV_ITEM *univ_add(int type, const char *desc);
char *univ_desc_line(UNIV_ITEM *ui, int linenum);
void univ_add_text(const char *txt);
void univ_add_debug(const char *desc, const char *txt);
void univ_add_group(const char *desc, const char *grpname);
void univ_add_mask(const char *desc, const char *mask);
void univ_add_file(const char *desc, const char *fname, const char *label);
UNIV_ITEM *univ_add_virt_num(const char *desc, const char *grp, ART_NUM art);
void univ_add_textfile(const char *desc, char *name);
void univ_add_virtgroup(const char *grpname);
void univ_use_pattern(const char *pattern, int type);
void univ_use_group_line(char *line, int type);
bool univ_file_load(char *fname, char *title, char *label);
void univ_mask_load(char *mask, const char *title);
void univ_redofile();
void univ_edit();
void univ_page_file(char *fname);
void univ_ng_virtual();
int univ_visit_group_main(const char *gname);
void univ_virt_pass();
void sort_univ();
const char *univ_article_desc(const UNIV_ITEM *ui);
void univ_help_main(int where);
void univ_help(int where);
const char *univ_keyhelp_modestr(const UNIV_ITEM *ui);
