/* univ.c
 */

/* Universal selector
 *
 */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/univ.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/final.h"
#include "trn/hash.h"
#include "trn/head.h"
#include "trn/help.h"
#include "trn/ng.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/score.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/url.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// TODO:
//
// Be friendlier when a file has no contents.
// Implement virtual groups (largely done)
// Help scan mode replacement
// Lots more to do...
//

int  g_univ_level{};          // How deep are we in the tree?
bool g_univ_ng_virt_flag{};   // if true, we are in the "virtual group" second pass
bool g_univ_read_virt_flag{}; // if true, we are reading an article from a "virtual group"
bool g_univ_default_cmd{};    // "follow"-related stuff (virtual groups)
bool g_univ_follow{true};
bool g_univ_follow_temp{};

// items which must be saved in context
UniversalItem *g_first_univ{};
UniversalItem *g_last_univ{};
UniversalItem *sel_page_univ{};
UniversalItem *g_sel_next_univ{};
char          *g_univ_fname{};  // current filename (may be null)
std::string    g_univ_label;    // current label (may be null)
std::string    g_univ_title;    // title of current level
std::string    g_univ_tmp_file; // temp. file (may be null)
HashTable     *g_univ_ng_hash{};
HashTable     *g_univ_vg_hash{};
// end of items that must be saved

static bool           s_univ_virt_pass_needed{}; //
static int            s_univ_item_counter{1};    //
static bool           s_univ_done_startup{};     //
static int            s_univ_min_score{};        // this score is part of the line format, so it is not ifdefed
static bool           s_univ_use_min_score{};    //
static bool           s_univ_begin_found{};      //
static char          *s_univ_begin_label{};      // label to start working with
static char          *s_univ_line_desc{};        // if non-nullptr, the description (printing name) of the entry
static UniversalItem *s_current_vg_ui{};         //
static bool           s_univ_user_top{};         // if true, the user has loaded their own top univ. config file

static void  univ_free_data(UniversalItem *ui);
static bool  univ_do_match(const char *text, const char *p);
static bool  univ_use_file(const char *fname, const char *label);
static bool  univ_include_file(const char *fname);
static void  univ_do_line_ext1(const char *desc, char *line);
static bool  univ_do_line(char *line);
static char *univ_edit_new_user_file();
static void  univ_vg_add_article(ArticleNum a);
static void  univ_vg_add_group();
static int   univ_order_number(const UniversalItem **ui1, const UniversalItem **ui2);
static int   univ_order_score(const UniversalItem **ui1, const UniversalItem **ui2);

void univ_init()
{
    g_univ_level = 0;
}

void univ_startup()
{
    // later: make user top file an option or environment variable?
    if (!univ_file_load("%+/univ/top", "Top Level", nullptr))
    {
        univ_open();
        g_univ_title = "Top Level";
        g_univ_fname = save_str("%+/univ/usertop");

        // read in trn default top file
        (void)univ_include_file("%X/sitetop");          // pure local
        bool sys_top_load = univ_include_file("%X/trn4top");
        bool user_top_load = univ_use_file("%+/univ/usertop", nullptr);

        if (!(sys_top_load || user_top_load))
        {
            // last resort--all newsgroups
            univ_close();
            univ_mask_load(save_str("*"),"All Newsgroups");
        }
        if (user_top_load)
        {
            s_univ_user_top = true;
        }
    }
    else
    {
        s_univ_user_top = true;
    }
    s_univ_done_startup = true;
}

void univ_open()
{
    g_first_univ = nullptr;
    g_last_univ = nullptr;
    sel_page_univ = nullptr;
    g_sel_next_univ = nullptr;
    g_univ_fname = nullptr;
    g_univ_title.clear();
    g_univ_label.clear();
    g_univ_tmp_file.clear();
    s_univ_virt_pass_needed = false;
    g_univ_ng_hash = nullptr;
    g_univ_vg_hash = nullptr;
    g_univ_level++;
}

void univ_close()
{
    UniversalItem* nextnode;

    for (UniversalItem *node = g_first_univ; node; node = nextnode)
    {
        univ_free_data(node);
        safe_free(node->desc);
        nextnode = node->next;
        std::free((char*)node);
    }
    if (!g_univ_tmp_file.empty())
    {
        remove(g_univ_tmp_file.c_str());
        g_univ_tmp_file.clear();
    }
    safe_free(g_univ_fname);
    g_univ_title.clear();
    g_univ_label.clear();
    if (g_univ_ng_hash)
    {
        hash_destroy(g_univ_ng_hash);
        g_univ_ng_hash = nullptr;
    }
    if (g_univ_vg_hash)
    {
        hash_destroy(g_univ_vg_hash);
        g_univ_vg_hash = nullptr;
    }
    g_first_univ = nullptr;
    g_last_univ = nullptr;
    sel_page_univ = nullptr;
    g_sel_next_univ = nullptr;
    g_univ_level--;
}

UniversalItem *univ_add(UniversalItemType type, const char *desc)
{
    UniversalItem *node = (UniversalItem*)safe_malloc(sizeof(UniversalItem));

    node->flags = UF_NONE;
    if (desc)
    {
        node->desc = save_str(desc);
    }
    else
    {
        node->desc = nullptr;
    }
    node->type = type;
    node->num = s_univ_item_counter++;
    node->score = 0;            // consider other default scores?
    node->next = nullptr;
    node->prev = g_last_univ;
    if (g_last_univ)
    {
        g_last_univ->next = node;
    }
    else
    {
        g_first_univ = node;
    }
    g_last_univ = node;

    return node;
}

static void univ_free_data(UniversalItem *ui)
{
    if (!ui)
    {
        return;
    }

    switch (ui->type)
    {
    case UN_NONE:     // cases where nothing is needed.
    case UN_TXT:
    case UN_HELP_KEY:
        break;

    case UN_DEBUG1:   // methods that use the string
        safe_free(ui->data.str);
        break;

    case UN_GROUP_MASK:        // methods that have custom data
        safe_free(ui->data.gmask.title);
        safe_free(ui->data.gmask.mask_list);
        break;

    case UN_CONFIG_FILE:
        safe_free(ui->data.cfile.title);
        safe_free(ui->data.cfile.fname);
        safe_free(ui->data.cfile.label);
        break;

    case UN_NEWSGROUP:
    case UN_GROUP_DESEL:
        safe_free(ui->data.group.ng);
        break;

    case UN_ARTICLE:
        safe_free(ui->data.virt.ng);
        safe_free(ui->data.virt.from);
        safe_free(ui->data.virt.subj);
        break;

    case UN_VGROUP:
    case UN_VGROUP_DESEL:
        safe_free(ui->data.vgroup.ng);
        break;

    case UN_TEXT_FILE:
        safe_free(ui->data.text_file.fname);
        break;

    case UN_DATA_SOURCE:  // unimplemented methods
    case UN_VIRTUAL1:
    default:
        break;
    }
}

// not used now, but may be used later...
//UNIV_ITEM* ui;                                // universal item
//int linenum;                          // which line to describe (0 base)
char *univ_desc_line(UniversalItem *ui, int linenum)
{
    return ui->desc;
}

void univ_add_text(const char *txt)
{
    // later check text for bad things
    (void)univ_add(UN_TXT,txt);
}

// temp for testing
void univ_add_debug(const char *desc, const char *txt)
{
    // later check text for bad things
    UniversalItem *ui = univ_add(UN_DEBUG1, desc);
    ui->data.str = save_str(txt);
}

void univ_add_group(const char *desc, const char *grpname)
{
    UniversalItem* ui;

    const char *s = grpname;
    if (!s)
    {
        return;
    }
    // later check grpname for bad things?

    if (!g_univ_ng_hash)
    {
        g_univ_ng_hash = hash_create(701, nullptr);
    }

    HashDatum data = hash_fetch(g_univ_ng_hash, grpname, std::strlen(grpname));

    if (data.dat_ptr)
    {
        // group was already added
        // perhaps it is marked as deleted?
        for (ui = g_first_univ; ui; ui = ui->next)
        {
            if ((ui->type == UN_GROUP_DESEL) && ui->data.group.ng //
                && !strcmp(ui->data.group.ng, grpname))
            {
                // undelete the newsgroup
                ui->type = UN_NEWSGROUP;
            }
        }
        return;
    }
    ui = univ_add(UN_NEWSGROUP,desc);
    ui->data.group.ng = save_str(grpname);
    data.dat_ptr = ui->data.group.ng;
    hash_store_last(data);
}

void univ_add_mask(const char *desc, const char *mask)
{
    UniversalItem *ui = univ_add(UN_GROUP_MASK, desc);
    ui->data.gmask.mask_list = save_str(mask);
    ui->data.gmask.title = save_str(desc);
}

//char* fname;                          // May be URL
void univ_add_file(const char *desc, const char *fname, const char *label)
{
    UniversalItem *ui = univ_add(UN_CONFIG_FILE, desc);
    ui->data.cfile.title = save_str(desc);
    ui->data.cfile.fname = save_str(fname);
    if (label && *label)
    {
        ui->data.cfile.label = save_str(label);
    }
    else
    {
        ui->data.cfile.label = nullptr;
    }
}

UniversalItem *univ_add_virt_num(const char *desc, const char *grp, ArticleNum art)
{
    UniversalItem *ui = univ_add(UN_ARTICLE, desc);
    ui->data.virt.ng = save_str(grp);
    ui->data.virt.num = art;
    ui->data.virt.subj = nullptr;
    ui->data.virt.from = nullptr;
    return ui;
}

void univ_add_text_file(const char *desc, char *name)
{
    UniversalItem* ui;
    char* p;
    static char lbuf[1024];

    char *s = name;
    switch (*s)
    {
    // later add URL handling
    case ':':
        s++;
        // FALL THROUGH

    default:
        // XXX later have error checking on length
        std::strcpy(lbuf,g_univ_fname);
        for (p = lbuf+std::strlen(lbuf); p > lbuf && *p != '/'; p--)
        {
        }
        if (p)
        {
            *p++ = '/';
            *p = '\0';
            std::strcat(lbuf,s);
            s = lbuf;
        }
        // FALL THROUGH

    case '~': // ...or full file names
    case '%':
    case '/':
        ui = univ_add(UN_TEXT_FILE,desc);
        ui->data.text_file.fname = save_str(file_exp(s));
        break;
    }
}

// mostly the same as the newsgroup stuff
void univ_add_virtual_group(const char *grpname)
{
    UniversalItem* ui;

    if (!grpname)
    {
        return;
    }

    // later check grpname for bad things?

    // perhaps leave if group has no unread, or other factor
    if (!g_univ_vg_hash)
    {
        g_univ_vg_hash = hash_create(701, nullptr);
    }

    s_univ_virt_pass_needed = true;
    HashDatum data = hash_fetch(g_univ_vg_hash, grpname, std::strlen(grpname));
    if (data.dat_ptr)
    {
        // group was already added
        // perhaps it is marked as deleted?
        for (ui = g_first_univ; ui; ui = ui->next)
        {
            if ((ui->type == UN_VGROUP_DESEL) && ui->data.vgroup.ng //
                && !std::strcmp(ui->data.vgroup.ng, grpname))
            {
                // undelete the newsgroup
                ui->type = UN_VGROUP;
            }
        }
        return;
    }
    ui = univ_add(UN_VGROUP,nullptr);
    ui->data.vgroup.flags = UF_VG_NONE;
    if (s_univ_use_min_score)
    {
        ui->data.vgroup.flags |= UF_VG_MIN_SCORE;
        ui->data.vgroup.min_score = s_univ_min_score;
    }
    ui->data.vgroup.ng = save_str(grpname);
    data.dat_ptr = ui->data.vgroup.ng;
    hash_store_last(data);
}

// univ_DoMatch uses a modified Wildmat function which is
// based on Rich $alz's wildmat, reduced to the simple case of *
// and text.  The complete version can be found in wildmat.c.
//

//
//  Match text and p, return true, false.
//
static bool univ_do_match(const char *text, const char *p)
{
    for (; *p; text++, p++)
    {
        if (*p == '*')
        {
            p = skip_eq(p, '*'); // Consecutive stars act just like one.
            if (*p == '\0')
            {
                // Trailing star matches everything.
                return true;
            }
            while (*text)
            {
                if (univ_do_match(text++, p))
                {
                    return true;
                }
            }
            return false;
        }
        if (*text != *p)
        {
            return false;
        }
    }
    return *text == '\0';
}

// type: 0=newsgroup, 1=virtual (more in future?)
void univ_use_pattern(const char *pattern, int type)
{
    const char* s = pattern;
    NewsgroupData* np;
    UniversalItem* ui;

    if (!s || !*s)
    {
        std::printf("\ngroup pattern: empty regular expression\n");
        return;
    }
    // XXX later: match all newsgroups in current datasrc to the pattern.
    // XXX later do a quick check to see if the group is a simple one.

    if (*s == '!')
    {
        s++;
        switch (type)
        {
        case 0:
            for (ui = g_first_univ; ui; ui = ui->next)
            {
                if (ui->type == UN_NEWSGROUP && ui->data.group.ng //
                    && univ_do_match(ui->data.group.ng, s))
                {
                    ui->type = UN_GROUP_DESEL;
                }
            }
            break;

        case 1:
            for (ui = g_first_univ; ui; ui = ui->next)
            {
                if (ui->type == UN_VGROUP && ui->data.vgroup.ng //
                    && univ_do_match(ui->data.vgroup.ng, s))
                {
                    ui->type = UN_VGROUP_DESEL;
                }
            }
            break;
        }
    }
    else
    {
        switch (type)
        {
        case 0:
            for (np = g_first_newsgroup; np; np = np->next)
            {
                if (univ_do_match(np->rc_line, s))
                {
                    univ_add_group(np->rc_line,np->rc_line);
                }
            }
            break;

        case 1:
            for (np = g_first_newsgroup; np; np = np->next)
            {
                if (univ_do_match(np->rc_line, s))
                {
                    univ_add_virtual_group(np->rc_line);
                }
            }
            break;
        }
    }
}

// interprets a line of newsgroups, adding or subtracting each pattern
// Newsgroup patterns are separated by spaces and/or commas
void univ_use_group_line(char *line, int type)
{
    char* p;

    char *s = line;
    if (!s || !*s)
    {
        return;
    }

    // newsgroup patterns will be separated by space(s) and/or comma(s)
    while (*s)
    {
        while (*s == ' ' || *s == ',')
        {
            s++;
        }
        for (p = s; *p && *p != ' ' && *p != ','; p++)
        {
        }
        char ch = *p;
        *p = '\0';
        univ_use_pattern(s,type);
        *p = ch;
        s = p;
    }
}

// returns true on success, false otherwise
static bool univ_use_file(const char *fname, const char *label)
{
    static char lbuf[LINE_BUF_LEN];

    bool begin_top = true;      // default assumption (might be changed later)

    if (!fname)
    {
        return false;   // bad argument
    }

    const char *s = fname;
    const char *open_name = fname;
    // open URLs and translate them into local temporary filenames
    if (string_case_equal(fname, "URL:", 4))
    {
        open_name = temp_filename();
        g_univ_tmp_file = open_name;
        if (!url_get(fname+4,open_name))
        {
            open_name = nullptr;
        }
        begin_top = false;      // we will need a "begin group"
    }
    else if (*s == ':')         // relative to last file's directory
    {
        std::printf("Colon filespec not supported for |%s|\n",s);
        open_name = nullptr;
    }
    if (!open_name)
    {
        return false;
    }
    s_univ_begin_found = begin_top;
    safe_free0(s_univ_begin_label);
    if (label)
    {
        s_univ_begin_label = save_str(label);
    }
    std::FILE *fp = std::fopen(file_exp(open_name), "r");
    if (!fp)
    {
        return false;           // unsuccessful (XXX: complain)
    }
    // Later considerations:
    // 1. Long lines
    // 2. Backslash continuations
    //
    while (std::fgets(lbuf, sizeof lbuf, fp) != nullptr)
    {
        if (!univ_do_line(lbuf))
        {
            break;      // end of useful file
        }
    }
    std::fclose(fp);
    if (!s_univ_begin_found)
    {
        std::printf("\"begin group\" not found.\n");
    }
    if (s_univ_begin_label)
    {
        std::printf("label not found: %s\n",s_univ_begin_label);
    }
    if (s_univ_virt_pass_needed)
    {
        univ_virt_pass();
    }
    sort_univ();
    return true;
}

static bool univ_include_file(const char *fname)
{
    char *old_univ_fname = g_univ_fname;
    g_univ_fname = save_str(fname);      // LEAK
    bool retval = univ_use_file(g_univ_fname, nullptr);
    g_univ_fname = old_univ_fname;
    return retval;
}

// do the '$' extensions of the line.
//char* line;                   // may be temporarily edited
static void univ_do_line_ext1(const char *desc, char *line)
{
    char* p;
    char* q;
    ArticleNum a;
    char ch;

    char *s = line;

    s++;
    switch (*s)
    {
    case 'v':
        s++;
        switch (*s)
        {
        case '0':             // test vector: "desc" $v0
            s++;
            (void) univ_add_virt_num(desc ? desc : s, "news.software.readers", ArticleNum{15000});
            break;

        case '1':             // "desc" $v1 1500 news.admin
            // XXX error checking
            s++;
            s = skip_space(s);
            p = skip_digits(s);
            ch = *p;
            *p = '\0';
            a = ArticleNum{atoi(s)};
            *p = ch;
            if (*p)
            {
                p++;
                (void)univ_add_virt_num(desc ? desc : s, p, a);
            }
            break;

        case 'g':             // $vg [scorenum] news.* !news.foo.*
            p = s;
            p++;
            p = skip_space(p);
            q = p;
            if ((*p=='+') || (*p=='-'))
            {
              p++;
            }
            p = skip_digits(p);
            if (std::isspace(*p))
            {
              *p = '\0';
              s_univ_min_score = std::atoi(q);
              s_univ_use_min_score = true;
              s = p;
              s++;
            }
            univ_use_group_line(s,1);
            s_univ_use_min_score = false;
            break;
        }
        break;

    case 't':         // text file
        s++;
        switch (*s)
        {
        case '0':             // test vector: "desc" $t0
            univ_add_text_file(desc? desc : s, "/home/c/caadams/ztext");
            break;
        }
        break;

    default:
          break;
    }
}

// returns false when no more lines should be interpreted
static bool univ_do_line(char *line)
{
    char* p;

    char *s = line + std::strlen(line) - 1;
    if (*s == '\n')
    {
        *s = '\0';                              // delete newline
    }

    s = skip_space(line);
    if (*s == '\0')
    {
        return true;    // empty line
    }

    if (!s_univ_begin_found)
    {
        if (string_case_compare(s,"begin group",11))
        {
            return true;        // wait until "begin group" is found
        }
        s_univ_begin_found = true;
    }
    if (s_univ_begin_label)
    {
        if (*s == '>' && s[1] == ':' && !std::strcmp(s + 2, s_univ_begin_label))
        {
            safe_free0(s_univ_begin_label); // interpret starting at next line
        }
        return true;
    }
    safe_free0(s_univ_line_desc);
    if (*s == '"')      // description name
    {
        p = copy_till(s,s+1,'"');
        if (!*p)
        {
            std::printf("univ: unmatched quote in string:\n\"%s\"\n", s);
            return true;
        }
        *p = '\0';
        s_univ_line_desc = save_str(s);
        s = p+1;
    }
    s = skip_space(s);
    if (string_case_equal(s, "end group",9))
    {
        return false;
    }
    if (string_case_equal(s, "URL:", 4))
    {
        p = skip_ne(s, '>');
        if (*p)
        {
            p++;
            if (!*p)            // empty label
            {
                p = nullptr;
            }
            // XXX later do more error checking
        }
        else
        {
            p = nullptr;
        }
        // description defaults to name
        univ_add_file(s_univ_line_desc? s_univ_line_desc : s, s, p);
    }
    else
    {
        switch (*s)
        {
        case '#':     // comment
            break;

        case ':':     // relative to g_univ_fname
            // XXX hack the variable and fall through
            if (g_univ_fname && std::strlen(g_univ_fname)+std::strlen(s) < 1020)
            {
                static char lbuf[1024];
                std::strcpy(lbuf,g_univ_fname);
                for (p = lbuf+std::strlen(lbuf); p > lbuf && *p != '/'; p--)
                {
                }
                if (p)
                {
                    *p++ = '/';
                    *p = '\0';
                    s++;
                    std::strcat(lbuf,s);
                    s = lbuf;
                }
            } // XXX later have else which will complain
            // FALL THROUGH

        case '~':     // ...or full file names
        case '%':
        case '/':
            p = skip_ne(s, '>');
            if (*p)
            {
                if (std::strlen(s) < 1020)
                {
                    static char lbuf[1024];
                    std::strcpy(lbuf,s);
                    s = lbuf;

                    p = skip_ne(s, '>'); // XXX Ick!
                    *p++ = '\0';        // separate label

                    if (!*p)            // empty label
                    {
                        p = nullptr;
                    }
                    // XXX later do more error checking
                }
            }
            else
            {
                p = nullptr;
            }
            // description defaults to name
            univ_add_file(s_univ_line_desc? s_univ_line_desc : s, file_exp(s), p);
            break;

        case '-':     // label within same file
            s++;
            if (*s++ != '>')
            {
                // XXX give an error message later
                break;
            }
            if (!g_univ_tmp_file.empty())
            {
                static char buff[1024];
                std::strcpy(buff, g_univ_tmp_file.c_str());
                p = buff;
            }
            else
            {
                p = g_univ_fname;
            }
            univ_add_file(s_univ_line_desc? s_univ_line_desc : s, g_univ_fname, s);
            break;

        case '>':
            if (s[1] == ':')
            {
                return false;   // label found, end of previous block
            }
            break;      // just ignore the line (print warning later?)

        case '@':       // virtual newsgroup file
            break;      // not used now

        case '&':       // text file shortcut (for help files)
            s++;
            univ_add_text_file(s_univ_line_desc? s_univ_line_desc : s, s);
            break;

        case '$':       // extension 1
            univ_do_line_ext1(s_univ_line_desc,s);
            break;

        default:
            // if there is a description, this must be a restriction list
            if (s_univ_line_desc)
            {
                univ_add_mask(s_univ_line_desc,s);
                break;
            }
            // one or more newsgroups instead
            univ_use_group_line(s,0);
            break;
        }
    }
    return true;        // continue reading
}

// features to return later (?):
//   text files
//

// level generator
bool univ_file_load(const char *fname, const char *title, const char *label)
{
    univ_open();

    if (fname)
    {
        g_univ_fname = save_str(fname);
    }
    if (title)
    {
        g_univ_title = title;
    }
    if (label)
    {
        g_univ_label = label;
    }
    bool flag = univ_use_file(fname, label);
    if (!flag)
    {
        univ_close();
    }
    if (g_int_count)
    {
        g_int_count = 0;
    }
    if (finput_pending(true))
    {
        // later, *maybe* eat input
    }
    return flag;
}

// level generator
void univ_mask_load(char *mask, const char *title)
{
    univ_open();

    univ_use_group_line(mask,0);
    if (title)
    {
        g_univ_title = title;
    }
    if (g_int_count)
    {
        g_int_count = 0;
    }
}

void univ_redo_file()
{
    char *tmp_fname = (g_univ_fname ? save_str(g_univ_fname) : nullptr);
    const std::string tmp_title = g_univ_title;
    const std::string tmp_label = g_univ_label;

    univ_close();
    if (g_univ_level)
    {
        (void)univ_file_load(tmp_fname,tmp_title.c_str(),tmp_label.c_str());
    }
    else
    {
        univ_startup();
    }

    safe_free(tmp_fname);
}


static char *univ_edit_new_user_file()
{
    char *s = save_str(file_exp("%+/univ/usertop"));       // LEAK

    // later, create a new user top file, and return its filename.
    // later perhaps ask whether to create or edit current file.
    // note: the user may need to restart in order to use the new file.
    //       (trn could do a univ_redofile, but it may be confusing.)
    //

    // if the file exists, do not create a new one
    std::FILE *fp = std::fopen(s, "r");
    if (fp)
    {
        std::fclose(fp);
        return g_univ_fname;    // as if this function was not called
    }

    make_dir(s, MD_FILE);

    fp = std::fopen(s,"w");
    if (!fp)
    {
        std::printf("Could not create new user file.\n");
        std::printf("Editing current system file\n");
        (void)get_anything();
        return g_univ_fname;
    }
    std::fprintf(fp,"# User Toplevel (Universal Selector)\n");
    std::fclose(fp);
    std::printf("New User Toplevel file created.\n");
    std::printf("After editing this file, exit and restart trn to use it.\n");
    (void)get_anything();
    s_univ_user_top = true;               // do not overwrite this file
    return s;
}

// code adapted from edit_kfile in kfile.c
// XXX problem if elements expand to larger than g_cmd_buf
void univ_edit()
{
    const char* s;

    if (s_univ_user_top || !(s_univ_done_startup))
    {
        if (!g_univ_tmp_file.empty())
        {
            s = g_univ_tmp_file.c_str();
        }
        else
        {
            s = g_univ_fname;
        }
    }
    else
    {
        s = univ_edit_new_user_file();
    }

    // later consider directory push/pop pair around editing
    (void)edit_file(s);
}

// later use some internal pager
void univ_page_file(char *fname)
{
    if (!fname || !*fname)
    {
        return;
    }

    std::sprintf(g_cmd_buf,"%s ",
            file_exp(get_val_const("HELPPAGER",get_val_const("PAGER","more"))));
    std::strcat(g_cmd_buf, file_exp(fname));
    term_down(3);
    reset_tty();                  // make sure tty is friendly
    do_shell(SH,g_cmd_buf);      // invoke the shell
    no_echo();                   // and make terminal
    cr_mode();                   // unfriendly again
    // later: consider something else that will return the key, and
    //        returning different codes based on the key.
    //
    if (!std::strncmp(g_cmd_buf,"more ",5))
    {
        get_anything();
    }
}

// virtual newsgroup second pass function
// called from within newsgroup
void univ_newsgroup_virtual()
{
    switch (s_current_vg_ui->type)
    {
    case UN_VGROUP:
        univ_vg_add_group();
        break;

    case UN_ARTICLE:
        // get article number from message-id
        break;

    default:
        break;
    }

    // later, get subjects and article numbers when needed
    // also do things like check scores, add newsgroups, etc.
}

static void univ_vg_add_article(ArticleNum a)
{
    char lbuf[70];

    int score = sc_score_art(a, false);
    if (s_univ_use_min_score && (score<s_univ_min_score))
    {
        return;
    }
    char *subj = fetch_subj(a, false);
    if (!subj || !*subj)
    {
        return;
    }
    char *from = fetch_from(a, false);
    if (!from || !*from)
    {
        from = "<No Author>";
    }

    safe_copy(lbuf,subj,sizeof lbuf - 4);
    // later scan/replace bad characters

    // later consider author in description, scoring, etc.
    UniversalItem *ui = univ_add_virt_num(nullptr, g_newsgroup_name.c_str(), a);
    ui->score = score;
    ui->data.virt.subj = save_str(subj);
    ui->data.virt.from = save_str(from);
}


static void univ_vg_add_group()
{
    // later: allow was-read articles, etc...
    for (ArticleNum a = article_first(g_first_art); a <= g_last_art; a = article_next(a))
    {
        if (!article_unread(a))
        {
            continue;
        }
        // minimum score check
        univ_vg_add_article(a);
    }
}

// returns do_newsgroup() value
int univ_visit_group_main(const char *gname)
{
    if (!gname || !*gname)
    {
        return NG_ERROR;
    }

    NewsgroupData *np = find_newsgroup(gname);
    if (!np)
    {
        std::printf("Univ/Virt: newsgroup %s not found!", gname);
        return NG_ERROR;
    }
    // unsubscribed, bogus, etc. groups are not visited
    if (np->to_read <= TR_UNSUB)
    {
      return NG_ERROR;
    }

    set_newsgroup(np);
    if (np != g_current_newsgroup)
    {
        // probably unnecessary...
        g_recent_newsgroup = g_current_newsgroup;
        g_current_newsgroup = np;
    }
    bool old_threaded = g_threaded_group;
    g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
    std::printf("\nScanning newsgroup %s\n",gname);
    int ret = do_newsgroup("");
    g_threaded_group = old_threaded;
    return ret;
}

// LATER: allow the loop to be interrupted
void univ_virt_pass()
{
    g_univ_ng_virt_flag = true;
    s_univ_virt_pass_needed = false;

    for (UniversalItem *ui = g_first_univ; ui; ui = ui->next)
    {
        if (input_pending())
        {
            // later consider cleaning up the remains
            break;
        }
        switch (ui->type)
        {
        case UN_VGROUP:
            if (!ui->data.vgroup.ng)
            {
                break;                  // XXX whine
            }
            s_current_vg_ui = ui;
            if (ui->data.vgroup.flags & UF_VG_MIN_SCORE)
            {
                s_univ_use_min_score = true;
                s_univ_min_score = ui->data.vgroup.min_score;
            }
            (void)univ_visit_group(ui->data.vgroup.ng);
            s_univ_use_min_score = false;
            // later do something with return value
            univ_free_data(ui);
            safe_free(ui->desc);
            ui->type = UN_DELETED;
            break;

        case UN_ARTICLE:
            // if article number is not set, visit newsgroup with callback
            // later also check for descriptions
            if ((ui->data.virt.num) && (ui->desc))
            {
              break;
            }
            if (ui->data.virt.subj)
            {
              break;
            }
            s_current_vg_ui = ui;
            (void)univ_visit_group(ui->data.virt.ng);
            // later do something with return value
            break;

        default:
            break;
        }
    }
    g_univ_ng_virt_flag = false;
}

static int univ_order_number(const UniversalItem** ui1, const UniversalItem** ui2)
{
    return (int)((*ui1)->num - (*ui2)->num) * g_sel_direction;
}

static int univ_order_score(const UniversalItem** ui1, const UniversalItem** ui2)
{
    if ((*ui1)->score != (*ui2)->score)
    {
        return (int)((*ui2)->score - (*ui1)->score) * g_sel_direction;
    }
    return (int)((*ui1)->num - (*ui2)->num) * g_sel_direction;
}

void sort_univ()
{
    int cnt = 0;
    for (const UniversalItem *i = g_first_univ; i; i = i->next)
    {
        cnt++;
    }

    if (cnt<=1)
    {
        return;
    }

    int (*sort_procedure)(const UniversalItem** ui1, const UniversalItem** ui2);
    switch (g_sel_sort)
    {
    case SS_SCORE:
        sort_procedure = univ_order_score;
        break;

    case SS_NATURAL:
    default:
        sort_procedure = univ_order_number;
        break;
    }

    UniversalItem **univ_sort_list = (UniversalItem**)safe_malloc(cnt * sizeof(UniversalItem*));
    UniversalItem** lp = univ_sort_list;
    for (UniversalItem *ui = g_first_univ; ui; ui = ui->next)
    {
        *lp++ = ui;
    }
    TRN_ASSERT(lp - univ_sort_list == cnt);

    std::qsort(univ_sort_list, cnt, sizeof(UniversalItem *), (int(*)(const void *, const void *))sort_procedure);

    g_first_univ = univ_sort_list[0];
    lp = univ_sort_list;
    for (int i = cnt; --i; lp++)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
    }
    g_last_univ = lp[0];
    g_last_univ->next = nullptr;

    std::free((char*)univ_sort_list);
}

// return a description of the article
// do this better later, like the code in sadesc.c
const char *univ_article_desc(const UniversalItem *ui)
{
    static char dbuf[200];
    static char sbuf[200];
    static char fbuf[200];

    char *s = ui->data.virt.subj;
    const char *f = ui->data.virt.from;
    if (!f)
    {
        std::strcpy(fbuf,"<No Author> ");
    }
    else
    {
        safe_copy(fbuf,compress_from(f,16),17);
    }
    if (!s)
    {
        std::strcpy(sbuf,"<No Subject>");
    }
    else
    {
        if ((s[0] == 'R') && //
            (s[1] == 'e') && //
            (s[2] == ':') && //
            (s[3] == ' '))
        {
            sbuf[0] = '>';
            safe_copy(sbuf+1,s+4,79);
        }
        else
        {
            safe_copy(sbuf,s,80);
        }
    }
    fbuf[16] = '\0';
    sbuf[55] = '\0';
    std::sprintf(dbuf,"[%3d] %16s %s",ui->score,fbuf,sbuf);
    for (char *t = dbuf; *t; t++)
    {
        if ((*t == Ctl('h')) || (*t == '\t') || (*t == '\n') || (*t == '\r'))
        {
            *t = ' ';
        }
    }
    dbuf[70] = '\0';
    return dbuf;
}

// Help start

// later: add online help as a new item type, add appropriate item
//        to the new level
//
//int where;    // what context were we in--use later for key help?
void univ_help_main(HelpLocation where)
{
    univ_open();
    g_univ_title = "Extended Help";

    // first add help on current mode
    UniversalItem *ui = univ_add(UN_HELP_KEY, nullptr);
    ui->data.i = where;

    // later, do other mode sensitive stuff

    // site-specific help
    univ_include_file("%X/sitehelp/top");

    // read in main help file
    g_univ_fname = save_str("%X/HelpFiles/top");
    bool flag = univ_use_file(g_univ_fname, g_univ_label.c_str());

    // later: if flag is not true, then add message?
}

void univ_help(HelpLocation where)
{
    univ_visit_help(where);     // push old selector info to stack
}

const char *univ_key_help_mode_str(const UniversalItem *ui)
{
    switch (ui->data.i)
    {
    case UHELP_PAGE:
        return "Article Pager Mode";

    case UHELP_ART:
        return "Article Display/Selection Mode";

    case UHELP_NG:
        return "Newsgroup Browse Mode";

    case UHELP_NGSEL:
        return "Newsgroup Selector";

    case UHELP_ADDSEL:
        return "Add-Newsgroup Selector";

    case UHELP_SUBS:
        return "Escape Substitutions";

    case UHELP_ARTSEL:
        return "Thread/Subject/Article Selector";

    case UHELP_MULTIRC:
        return "Newsrc Selector";

    case UHELP_OPTIONS:
        return "Option Selector";

    case UHELP_SCANART:
        return "Article Scan Mode";

    case UHELP_UNIV:
        return "Universal Selector";

    default:
        return nullptr;
    }
}
