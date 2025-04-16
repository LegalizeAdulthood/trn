/* opt.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>
#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/opt.h"

#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artsrch.h"
#include "trn/cache.h"
#include "trn/change_dir.h"
#include "trn/charsubst.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "util/env.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/init.h"
#include "trn/intrp.h"
#include "trn/mime.h"
#include "trn/ng.h"
#include "trn/ngdata.h"
#include "trn/ngstuff.h"
#include "trn/only.h"
#include "trn/respond.h"
#include "trn/rt-page.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rt-wumpus.h"
#include "trn/rthread.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/scorefile.h"
#include "trn/string-algos.h"
#include "trn/sw.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/univ.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

COMPEX      g_optcompex;
std::string g_ini_file;
INI_WORDS   g_options_ini[] = {
    // clang-format off
    { 0, "OPTIONS", nullptr },

    { 0, "*Display Options", nullptr },
    { 0, "Terse Output", "yes/no" },
    { 0, "Pager Line-Marking", "standout/underline/no" },
    { 0, "Erase Screen", "yes/no" },
    { 0, "Erase Each Line", "yes/no" },
    { 0, "Muck Up Clear", "yes/no" },
    { 0, "Background Spinner", "yes/no" },
    { 0, "Charset", "<e.g. patm>" },
    { 0, "Filter Control Characters", "yes/no" },

    { 0, "*Selector Options", nullptr },
    { 0, "Use Universal Selector", "yes/no" },
    { 0, "Universal Selector Order", "natural/points" },
    { 0, "Universal Selector article follow", "yes/no" },
    { 0, "Universal Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Newsrc Selector", "yes/no" },
    { 0, "Newsrc Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Addgroup Selector", "yes/no" },
    { 0, "Addgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Newsgroup Selector", "yes/no" },
    { 0, "Newsgroup Selector Order", "natural/group/count" },
    { 0, "Newsgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Newsgroup Selector Display Styles", "<e.g. slm=short/long/med>" },
    { 0, "Use News Selector", "yes/no/<# articles>" },
    { 0, "News Selector Mode", "threads/subjects/articles" },
    { 0, "News Selector Order", "[reverse] date/subject/author/groups/cnt/points" },
    { 0, "News Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "News Selector Display Styles", "<e.g. lms=long/medium/short>" },
    { 0, "Option Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Selector Numbers", "yes/no" },
    { 0, "Selector Number Auto-Goto", "yes/no" },

    { 0, "*Newsreading Options", nullptr },
    { 0, "Use Threads", "yes/no" },
    { 0, "Select My Postings", "subthread/parent/thread/no" },
    { 0, "Initial Article Lines", "no/<# lines>" },
    { 0, "Article Tree Lines", "no/<# lines>" },
    { 0, "Word-Wrap Margin", "no/<# chars in margin>" },
    { 0, "Auto-Grow Groups", "yes/no" },
    { 0, "Compress Subjects", "yes/no" },
    { 0, "Join Subject Lines", "no/<# chars>" },
    { 0, "Line Num for Goto", "<# line (1-n)>" },
    { 0, "Ignore THRU on Select", "yes/no" },
    { 0, "Read Breadth First", "yes/no" },
    { 0, "Background Threading", "yes/no" },
    { 0, "Scan Mode Count", "no/<# articles>" },
    { 0, "Header Magic", "<[!]header,...>" },
    { 0, "Header Hiding", "<[!]header,...>" },

    { 0, "*Posting Options", nullptr },
    { 0, "Cited Text String", "<e.g. '>'>" },
#if 0
    { 0, "Attribute string", "<e.g. ...>" },
    { 0, "Reply To", "<e.g. ...>" },
#endif

    { 0, "*Save Options", nullptr },
    { 0, "Save Dir", "<directory path>" },
    { 0, "Auto Savename", "yes/no" },
    { 0, "Default Savefile Type", "norm/mail/ask" },

    { 0, "*Mouse Options", nullptr },
    { 0, "Use XTerm Mouse", "yes/no" },
    { 0, "Mouse Modes", "<e.g. acjlptwK>" },
    { 0, "Universal Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>" },
    { 0, "Newsrc Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>" },
    { 0, "Addgroup Selector Mousebar", "<e.g. [Top]^ [Bot]$ [ OK ]Z>" },
    { 0, "Newsgroup Selector Mousebar", "<e.g. [ OK ]Z [Quit]q [Help]?>" },
    { 0, "News Selector Mousebar", "<e.g. [KillPg]D [Read]^j [Quit]Q>" },
    { 0, "Option Selector Mousebar", "<e.g. [Save]S [Use]^I [Abandon]q>" },
    { 0, "Article Pager Mousebar", "<e.g. [Next]n J [Sel]+ [Quit]q>" },

    { 0, "*MIME Options", nullptr },
    { 0, "Multipart Separator", "<string>" },
    { 0, "Auto-View Inline", "yes/no" },

    { 0, "*Misc Options", nullptr },
    { 0, "Check for New Groups", "yes/no" },
    { 0, "Restriction Includes Empty Groups", "yes/no" },
    { 0, "Append Unsubscribed Groups", "yes/no" },
    { 0, "Initial Group List", "no/<# groups>" },
    { 0, "Restart At Last Group", "yes/no" },
    { 0, "Eat Type-Ahead", "yes/no" },
    { 0, "Verify Input", "yes/no" },
    { 0, "Fuzzy Newsgroup Names", "yes/no" },
    { 0, "Auto Arrow Macros", "regular/alternate/no" },
    { 0, "Checkpoint Newsrc Frequency", "<# articles>" },
    { 0, "Default Refetch Time", "never/<1 day 5 hours 8 mins>" },
    { 0, "Novice Delays", "yes/no" },
    { 0, "Old Mthreads Database", "yes/no" },

    { 0, "*Article Scan Mode Options", nullptr },
    { 0, "Follow Threads", "yes/no" },
    { 0, "Fold Subjects", "yes/no" },
    { 0, "Re-fold Subjects", "yes/no" },
    { 0, "Mark Without Moving", "yes/no" },
    { 0, "VI Key Movement Allowed", "yes/no" },
    { 0, "Display Item Numbers", "yes/no" },
    { 0, "Display Article Number", "yes/no" },
    { 0, "Display Author", "yes/no" },
    { 0, "Display Score", "yes/no" },
    { 0, "Display Subject Count", "yes/no" },
    { 0, "Display Subject", "yes/no" },
    { 0, "Display Summary", "yes/no" },
    { 0, "Display Keywords", "yes/no" },

    { 0, "*Scoring Options", nullptr },
    { 0, "Verbose scoring", "yes/no" },

    { 0, nullptr, nullptr }
};
// clang-format on
char        **g_option_def_vals{};
char        **g_option_saved_vals{};
option_flags *g_option_flags{};
int           g_sel_page_op{};

static char s_univ_sel_cmds[3]{"Z>"};

static char* hidden_list();
static char* magic_list();
static void  set_header_list(headtype_flags flag, headtype_flags defflag, const char *str);
static int   parse_mouse_buttons(char **cpp, const char *btns);
static char *expand_mouse_buttons(char *cp, int cnt);

void opt_init(int argc, char *argv[], char **tcbufptr)
{
    g_sel_grp_dmode = savestr("*slm") + 1;
    g_sel_art_dmode = savestr("*lmds") + 1;
    g_univ_sel_btn_cnt = parse_mouse_buttons(&g_univ_sel_btns,
                                        "[Top]^ [PgUp]< [PgDn]> [ OK ]^j [Quit]q [Help]?");
    g_newsrc_sel_btn_cnt = parse_mouse_buttons(&g_newsrc_sel_btns,
                                          "[Top]^ [PgUp]< [PgDn]> [ OK ]^j [Quit]q [Help]?");
    g_add_sel_btn_cnt = parse_mouse_buttons(&g_add_sel_btns,
                                       "[Top]^ [Bot]$ [PgUp]< [PgDn]> [ OK ]Z [Quit]q [Help]?");
    g_option_sel_btn_cnt = parse_mouse_buttons(&g_option_sel_btns,
                                          "[Find]/ [FindNext]/^j [Top]^ [Bot]$ [PgUp]< [PgDn]> [Use]^i [Save]S [Abandon]q [Help]?");
    g_newsgroup_sel_btn_cnt = parse_mouse_buttons(&g_newsgroup_sel_btns,
                                             "[Top]^ [PgUp]< [PgDn]> [ OK ]Z [Quit]q [Help]?");
    g_news_sel_btn_cnt = parse_mouse_buttons(&g_news_sel_btns,
                                        "[Top]^ [Bot]$ [PgUp]< [PgDn]> [KillPg]D [ OK ]Z [Quit]q [Help]?");
    g_art_pager_btn_cnt = parse_mouse_buttons(&g_art_pager_btns,
                                         "[Next]n [Sel]+ [Quit]q [Help]h");

    prep_ini_words(g_options_ini);
    if (argc >= 2 && !strcmp(argv[1],"-c"))
    {
        g_checkflag=true;                       /* so we can optimize for -c */
    }
    interp(*tcbufptr,TCBUF_SIZE,GLOBINIT);
    opt_file(*tcbufptr,tcbufptr,false);

    const int len = ini_len(g_options_ini);
    g_option_def_vals = (char**)safemalloc(len*sizeof(char*));
    std::memset((char*)g_option_def_vals,0,(g_options_ini)[0].checksum * sizeof (char*));
    /* Set DEFHIDE and DEFMAGIC to current values and clear g_user_htype list */
    set_header_list(HT_DEFHIDE,HT_HIDE,"");
    set_header_list(HT_DEFMAGIC,HT_MAGIC,"");

    g_ini_file = filexp(g_use_threads ? get_val_const("TRNRC", "%+/trnrc") : get_val_const("RNRC", "%+/rnrc"));

    char *s = filexp("%+");
    stat_t ini_stat{};
    if (stat(s, &ini_stat) < 0 || !S_ISDIR(ini_stat.st_mode))
    {
        std::printf("Creating the directory %s.\n",s);
        if (makedir(s, MD_DIR))
        {
            std::printf("Unable to create `%s'.\n",s);
            finalize(1); /*$$??*/
        }
    }
    if (stat(g_ini_file.c_str(),&ini_stat) == 0)
    {
        opt_file(g_ini_file.c_str(), tcbufptr, true);
    }
    if (!g_use_threads || (s = get_val("TRNINIT")) == nullptr)
    {
        s = get_val("RNINIT");
    }
    if (*safecpy(*tcbufptr, s, TCBUF_SIZE))
    {
        if (*s == '-' || *s == '+' || isspace(*s))
        {
            sw_list(*tcbufptr);
        }
        else
        {
            sw_file(tcbufptr);
        }
    }
    g_option_saved_vals = (char**)safemalloc(len*sizeof(char*));
    std::memset((char*)g_option_saved_vals,0,(g_options_ini)[0].checksum * sizeof (char*));
    g_option_flags = new option_flags[len];
    std::fill_n(g_option_flags, len, OF_NONE);

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            decode_switch(argv[i]);
        }
    }
    init_compex(&g_optcompex);

    g_privdir = filexp("~/News");
}

void opt_final()
{
    g_privdir.clear();
    delete[] g_option_flags;
    g_option_flags = nullptr;
    safefree0(g_option_saved_vals);
    g_ini_file.clear();
    safefree0(g_option_def_vals);
    safefree0(g_art_pager_btns);
    safefree0(g_news_sel_btns);
    safefree0(g_newsgroup_sel_btns);
    safefree0(g_option_sel_btns);
    safefree0(g_add_sel_btns);
    safefree0(g_newsrc_sel_btns);
    safefree0(g_univ_sel_btns);
    g_sel_art_dmode--;
    safefree0(g_sel_art_dmode);
    g_sel_grp_dmode--;
    safefree0(g_sel_grp_dmode);
}

void opt_file(const char *filename, char **tcbufptr, bool bleat)
{
    char*filebuf = *tcbufptr;
    char*section;
    char*cond;
    int  fd = open(filename,0);

    if (fd >= 0)
    {
        stat_t opt_stat{};
        fstat(fd,&opt_stat);
        if (opt_stat.st_size >= TCBUF_SIZE - 1)
        {
            filebuf = saferealloc(filebuf,(MEM_SIZE)opt_stat.st_size+2);
            *tcbufptr = filebuf;
        }
        if (opt_stat.st_size)
        {
            int len = read(fd,filebuf,(int)opt_stat.st_size);
            filebuf[len] = '\0';
            prep_ini_data(filebuf,filename);
            char *s = filebuf;
            while ((s = next_ini_section(s, &section, &cond)) != nullptr)
            {
                if (*cond && !check_ini_cond(cond))
                {
                    continue;
                }
                if (!std::strcmp(section, "options"))
                {
                    s = parse_ini_section(s, g_options_ini);
                    if (!s)
                    {
                        break;
                    }
                    set_options(ini_values(g_options_ini));
                }
                else if (!std::strcmp(section, "environment"))
                {
                    while (*s && *s != '[')
                    {
                        section = s;
                        s += std::strlen(s) + 1;
                        export_var(section,s);
                        s += std::strlen(s) + 1;
                    }
                }
                else if (!std::strcmp(section, "termcap"))
                {
                    while (*s && *s != '[')
                    {
                        section = s;
                        s += std::strlen(s) + 1;
                        add_tc_string(section, s);
                        s += std::strlen(s) + 1;
                    }
                }
                else if (!std::strcmp(section, "attribute"))
                {
                    while (*s && *s != '[')
                    {
                        section = s;
                        s += std::strlen(s) + 1;
                        color_rc_attribute(section, s);
                        s += std::strlen(s) + 1;
                    }
                }
            }
        }
        close(fd);
    }
    else if (bleat)
    {
        std::printf(g_cantopen,filename);
        /*termdown(1);*/
    }

    *filebuf = '\0';
}

inline bool YES(const char *s)
{
    return *s == 'y' || *s == 'Y';
}
inline bool NO(const char *s)
{
    return *s == 'n' || *s == 'N';
}

void set_options(char **vals)
{
    int limit = ini_len(g_options_ini);
    for (int i = 1; i < limit; i++)
    {
        if (*++vals)
        {
            set_option(static_cast<option_index>(i), *vals);
        }
    }
}

void set_option(option_index num, const char *s)
{
    if (g_option_saved_vals)
    {
        if (!g_option_saved_vals[num])
        {
            g_option_saved_vals[num] = savestr(option_value(num));
            if (!g_option_def_vals[num])
            {
                g_option_def_vals[num] = g_option_saved_vals[num];
            }
        }
    }
    else if (g_option_def_vals)
    {
        if (!g_option_def_vals[num])
        {
            g_option_def_vals[num] = savestr(option_value(num));
        }
    }
    switch (num)
    {
      case OI_USE_THREADS:
        g_use_threads = YES(s);
        break;
      case OI_USE_MOUSE:
        g_use_mouse = YES(s);
        if (g_use_mouse)
        {
            /* set up the Xterm mouse sequence */
            set_macro("\033[M+3","\003");
        }
        break;
      case OI_MOUSE_MODES:
        safecpy(g_mouse_modes, s, sizeof g_mouse_modes);
        break;
      case OI_USE_UNIV_SEL:
        g_use_univ_selector = YES(s);
        break;
      case OI_UNIV_SEL_CMDS:
        *s_univ_sel_cmds = *s;
        if (s[1])
        {
            s_univ_sel_cmds[1] = s[1];
        }
        break;
      case OI_UNIV_SEL_BTNS:
        g_univ_sel_btn_cnt = parse_mouse_buttons(&g_univ_sel_btns,s);
        break;
      case OI_UNIV_SEL_ORDER:
        set_sel_order(SM_UNIVERSAL,s);
        break;
      case OI_UNIV_FOLLOW:
        g_univ_follow = YES(s);
        break;
      case OI_USE_NEWSRC_SEL:
        g_use_newsrc_selector = YES(s);
        break;
      case OI_NEWSRC_SEL_CMDS:
        *g_newsrc_sel_cmds = *s;
        if (s[1])
        {
            g_newsrc_sel_cmds[1] = s[1];
        }
        break;
      case OI_NEWSRC_SEL_BTNS:
        g_newsrc_sel_btn_cnt = parse_mouse_buttons(&g_newsrc_sel_btns,s);
        break;
      case OI_USE_ADD_SEL:
        g_use_add_selector = YES(s);
        break;
      case OI_ADD_SEL_CMDS:
        *g_add_sel_cmds = *s;
        if (s[1])
        {
            g_add_sel_cmds[1] = s[1];
        }
        break;
      case OI_ADD_SEL_BTNS:
        g_add_sel_btn_cnt = parse_mouse_buttons(&g_add_sel_btns,s);
        break;
      case OI_USE_NEWSGROUP_SEL:
        g_use_newsgroup_selector = YES(s);
        break;
      case OI_NEWSGROUP_SEL_ORDER:
        set_sel_order(SM_NEWSGROUP,s);
        break;
      case OI_NEWSGROUP_SEL_CMDS:
        *g_newsgroup_sel_cmds = *s;
        if (s[1])
        {
            g_newsgroup_sel_cmds[1] = s[1];
        }
        break;
      case OI_NEWSGROUP_SEL_BTNS:
        g_newsgroup_sel_btn_cnt = parse_mouse_buttons(&g_newsgroup_sel_btns,s);
        break;
      case OI_NEWSGROUP_SEL_STYLES:
        g_sel_grp_dmode--;
        safefree0(g_sel_grp_dmode);
        g_sel_grp_dmode = safemalloc(std::strlen(s)+2);
        *g_sel_grp_dmode++ = '*';
        std::strcpy(g_sel_grp_dmode, s);
        break;
      case OI_USE_NEWS_SEL:
        if (std::isdigit(*s))
        {
            g_use_news_selector = std::atoi(s);
        }
        else
        {
            g_use_news_selector = static_cast<int>(YES(s)) - 1;
        }
        break;
      case OI_NEWS_SEL_MODE:
      {
        const sel_mode save_sel_mode = g_sel_mode;
        set_sel_mode(*s);
        if (save_sel_mode != SM_ARTICLE && save_sel_mode != SM_SUBJECT //
            && save_sel_mode != SM_THREAD)
        {
            g_sel_mode = save_sel_mode;
            set_selector(SM_MAGIC_NUMBER,SS_MAGIC_NUMBER);
        }
        break;
      }
      case OI_NEWS_SEL_ORDER:
        set_sel_order(g_sel_defaultmode,s);
        break;
      case OI_NEWS_SEL_CMDS:
        *g_news_sel_cmds = *s;
        if (s[1])
        {
            g_news_sel_cmds[1] = s[1];
        }
        break;
      case OI_NEWS_SEL_BTNS:
        g_news_sel_btn_cnt = parse_mouse_buttons(&g_news_sel_btns,s);
        break;
      case OI_NEWS_SEL_STYLES:
        g_sel_art_dmode--;
        safefree0(g_sel_art_dmode);
        g_sel_art_dmode = safemalloc(std::strlen(s)+2);
        *g_sel_art_dmode++ = '*';
        std::strcpy(g_sel_art_dmode, s);
        break;
      case OI_OPTION_SEL_CMDS:
        *g_option_sel_cmds = *s;
        if (s[1])
        {
            g_option_sel_cmds[1] = s[1];
        }
        break;
      case OI_OPTION_SEL_BTNS:
        g_option_sel_btn_cnt = parse_mouse_buttons(&g_option_sel_btns,s);
        break;
      case OI_AUTO_SAVE_NAME:
        if (!g_checkflag)
        {
            if (YES(s))
            {
                export_var("SAVEDIR",  "%p/%c");
                export_var("SAVENAME", "%a");
            }
            else if (!std::strcmp(get_val_const("SAVEDIR", ""), "%p/%c") //
                     && !std::strcmp(get_val_const("SAVENAME", ""), "%a"))
            {
                export_var("SAVEDIR", "%p");
                export_var("SAVENAME", "%^C");
            }
        }
        break;
      case OI_BKGND_THREADING:
        g_thread_always = !YES(s);
        break;
      case OI_AUTO_ARROW_MACROS:
      {
        int prev = g_auto_arrow_macros;
        if (YES(s) || *s == 'r' || *s == 'R')
        {
            g_auto_arrow_macros = 2;
        }
        else
        {
            g_auto_arrow_macros = !NO(s);
        }
        if (g_mode != MM_INITIALIZING && g_auto_arrow_macros != prev)
        {
            char tmpbuf[1024];
            arrow_macros(tmpbuf);
        }
        break;
      }
      case OI_READ_BREADTH_FIRST:
        g_breadth_first = YES(s);
        break;
      case OI_BKGND_SPINNER:
        g_bkgnd_spinner = YES(s);
        break;
      case OI_CHECKPOINT_NEWSRC_FREQUENCY:
        g_docheckwhen = std::atoi(s);
        break;
      case OI_SAVE_DIR:
        if (!g_checkflag)
        {
            g_savedir = s;
            if (!g_privdir.empty())
            {
                change_dir(g_privdir);
            }
            g_privdir = filexp(s);
        }
        break;
      case OI_ERASE_SCREEN:
        g_erase_screen = YES(s);
        break;
      case OI_NOVICE_DELAYS:
        g_novice_delays = YES(s);
        break;
      case OI_CITED_TEXT_STRING:
        g_indstr = s;
        break;
      case OI_GOTO_LINE_NUM:
        g_gline = std::atoi(s)-1;
        break;
      case OI_FUZZY_NEWSGROUP_NAMES:
        g_fuzzy_get = YES(s);
        break;
      case OI_HEADER_MAGIC:
        if (!g_checkflag)
        {
            set_header_list(HT_MAGIC, HT_DEFMAGIC, s);
        }
        break;
      case OI_HEADER_HIDING:
        set_header_list(HT_HIDE, HT_DEFHIDE, s);
        break;
      case OI_INITIAL_ARTICLE_LINES:
        g_initlines = std::atoi(s);
        break;
      case OI_APPEND_UNSUBSCRIBED_GROUPS:
        g_append_unsub = YES(s);
        break;
      case OI_FILTER_CONTROL_CHARACTERS:
        g_dont_filter_control = !YES(s);
        break;
      case OI_JOIN_SUBJECT_LINES:
        if (std::isdigit(*s))
        {
            change_join_subject_len(std::atoi(s));
        }
        else
        {
            change_join_subject_len(YES(s)? 30 : 0);
        }
        break;
      case OI_IGNORE_THRU_ON_SELECT:
        g_kill_thru_kludge = YES(s);
        break;
      case OI_AUTO_GROW_GROUPS:
        g_keep_the_group_static = !YES(s);
        break;
      case OI_MUCK_UP_CLEAR:
        g_muck_up_clear = YES(s);
        break;
      case OI_ERASE_EACH_LINE:
        g_erase_each_line = YES(s);
        break;
      case OI_SAVEFILE_TYPE:
        g_mbox_always = (*s == 'm' || *s == 'M');
        g_norm_always = (*s == 'n' || *s == 'N');
        break;
      case OI_PAGER_LINE_MARKING:
        if (std::isdigit(*s))
        {
            g_marking_areas = static_cast<marking_areas>(std::atoi(s));
        }
        else
        {
            g_marking_areas = HALFPAGE_MARKING;
        }
        if (NO(s))
        {
            g_marking = NOMARKING;
        }
        else if (*s == 'u')
        {
            g_marking = UNDERLINE;
        }
        else
        {
            g_marking = STANDOUT;
        }
        break;
      case OI_OLD_MTHREADS_DATABASE:
        if (std::isdigit(*s))
        {
            g_olden_days = std::atoi(s);
        }
        else
        {
            g_olden_days = YES(s);
        }
        break;
      case OI_SELECT_MY_POSTS:
        if (NO(s))
        {
            g_auto_select_postings = 0;
        }
        else
        {
            switch (*s)
            {
              case 't':
                g_auto_select_postings = '+';
                break;
              case 'p':
                g_auto_select_postings = 'p';
                break;
              default:
                g_auto_select_postings = '.';
                break;
            }
        }
        break;
      case OI_MULTIPART_SEPARATOR:
        g_multipart_separator = s;
        break;
      case OI_AUTO_VIEW_INLINE:
        g_auto_view_inline = YES(s);
        break;
      case OI_NEWGROUP_CHECK:
        g_quickstart = !YES(s);
        break;
      case OI_RESTRICTION_INCLUDES_EMPTIES:
        g_empty_only_char = YES(s)? 'o' : 'O';
        break;
      case OI_CHARSET:
        g_charsets = s;
        break;
      case OI_INITIAL_GROUP_LIST:
        if (std::isdigit(*s))
        {
            g_countdown = std::atoi(s);
            g_suppress_cn = (g_countdown == 0);
        }
        else
        {
            g_suppress_cn = NO(s);
            if (!g_suppress_cn)
            {
                g_countdown = 5;
            }
        }
        break;
      case OI_RESTART_AT_LAST_GROUP:
        g_findlast = YES(s) * (g_mode == MM_INITIALIZING? 1 : -1);
        break;
      case OI_SCANMODE_COUNT:
        if (std::isdigit(*s))
        {
            g_scanon = std::atoi(s);
        }
        else
        {
            g_scanon = YES(s)*3;
        }
        break;
      case OI_TERSE_OUTPUT:
        g_verbose = !YES(s);
        if (!g_verbose)
        {
            g_novice_delays = false;
        }
        break;
      case OI_EAT_TYPEAHEAD:
        g_allow_typeahead = !YES(s);
        break;
      case OI_COMPRESS_SUBJECTS:
        g_unbroken_subjects = !YES(s);
        break;
      case OI_VERIFY_INPUT:
        g_verify = YES(s);
        break;
      case OI_ARTICLE_TREE_LINES:
        if (std::isdigit(*s))
        {
            g_max_tree_lines = std::atoi(s);
            g_max_tree_lines = std::min(g_max_tree_lines, 11);
        }
        else
        {
            g_max_tree_lines = YES(s) * 6;
        }
        break;
      case OI_WORD_WRAP_MARGIN:
        if (std::isdigit(*s))
        {
            g_word_wrap_offset = std::atoi(s);
        }
        else if (YES(s))
        {
            g_word_wrap_offset = 8;
        }
        else
        {
            g_word_wrap_offset = -1;
        }
        break;
      case OI_DEFAULT_REFETCH_TIME:
        g_def_refetch_secs = text2secs(s, DEFAULT_REFETCH_SECS);
        break;
      case OI_ART_PAGER_BTNS:
        g_art_pager_btn_cnt = parse_mouse_buttons(&g_art_pager_btns,s);
        break;
      case OI_SCAN_ITEMNUM:
        g_s_itemnum = YES(s);
        break;
      case OI_SCAN_VI:
        g_s_mode_vi = YES(s);
        break;
      case OI_SCANA_FOLLOW:
        g_sa_follow = YES(s);
        break;
      case OI_SCANA_FOLD:
        g_sa_mode_fold = YES(s);
        break;
      case OI_SCANA_UNZOOMFOLD:
        g_sa_unzoomrefold = YES(s);
        break;
      case OI_SCANA_MARKSTAY:
        g_sa_mark_stay = YES(s);
        break;
      case OI_SCANA_DISPANUM:
        g_sa_mode_desc_artnum = YES(s);
        break;
      case OI_SCANA_DISPAUTHOR:
        g_sa_mode_desc_author = YES(s);
        break;
      case OI_SCANA_DISPSCORE:
        g_sa_mode_desc_score = YES(s);
        break;
      case OI_SCANA_DISPSUBCNT:
        g_sa_mode_desc_threadcount = YES(s);
        break;
      case OI_SCANA_DISPSUBJ:
#if 0
        /* CAA: for now, always on. */
        g_sa_mode_desc_subject = YES(s);
#endif
        break;
      case OI_SCANA_DISPSUMMARY:
        g_sa_mode_desc_summary = YES(s);
        break;
      case OI_SCANA_DISPKEYW:
        g_sa_mode_desc_keyw = YES(s);
        break;
      case OI_SC_VERBOSE:
        g_sf_verbose = YES(s);
        break;
      case OI_USE_SEL_NUM:
        g_use_sel_num = YES(s);
        break;
      case OI_SEL_NUM_GOTO:
        g_sel_num_goto = YES(s);
        break;
      default:
        std::printf("*** Internal error: Unknown Option ***\n");
        break;
    }
}

void save_options(const char *filename)
{
    char* filebuf = nullptr;
    char* line = nullptr;
    static bool first_time = true;

    std::sprintf(g_buf,"%s.new",filename);
    std::FILE *fp_out = std::fopen(g_buf, "w");
    if (!fp_out)
    {
        std::printf(g_cantcreate,g_buf);
        return;
    }
    int fd_in = open(filename, 0);
    if (fd_in >= 0)
    {
        char* cp;
        char* nlp = nullptr;
        char* comments = nullptr;
        stat_t opt_stat{};
        fstat(fd_in,&opt_stat);
        if (opt_stat.st_size)
        {
            filebuf = safemalloc((MEM_SIZE)opt_stat.st_size+2);
            int len = read(fd_in, filebuf, (int)opt_stat.st_size);
            filebuf[len] = '\0';
        }
        close(fd_in);

        for (line = filebuf; line && *line; line = nlp)
        {
            cp = line;
            nlp = std::strchr(cp, '\n');
            if (nlp)
            {
                *nlp++ = '\0';
            }
            cp = skip_space(cp);
            if (*cp == '[' && !std::strncmp(cp + 1, "options]", 8))
            {
                cp = skip_space(cp + 9);
                if (!*cp)
                {
                    break;
                }
            }
            std::fputs(line, fp_out);
            std::fputc('\n', fp_out);
        }
        for (line = nlp; line && *line; line = nlp)
        {
            cp = line;
            nlp = std::strchr(cp, '\n');
            if (nlp)
            {
                nlp++;
            }
            while (*cp != '\n' && std::isspace(*cp))
            {
                cp++;
            }
            if (*cp == '[')
            {
                break;
            }
            if (std::isalpha(*cp))
            {
                comments = nullptr;
            }
            else if (!comments)
            {
                comments = line;
            }
        }
        if (comments)
        {
            line = comments;
        }
    }
    else
    {
        const char *t = g_use_threads? "T" : "";
        std::printf("\n"
               "This is the first save of the option file, %s.\n"
               "By default this file overrides your %sRNINIT variable, but if you\n"
               "want to continue to use an old-style init file (that overrides the\n"
               "settings in the option file), edit the option file and change the\n"
               "line that sets %sRNINIT.\n",
               g_ini_file.c_str(), t, t);
        get_anything();
        std::fprintf(fp_out, "# trnrc file auto-generated\n[environment]\n");
        write_init_environment(fp_out);
        std::fprintf(fp_out, "%sRNINIT = ''\n\n", t);
    }
    std::fprintf(fp_out,"[options]\n");
    for (int i = 1; g_options_ini[i].checksum; i++)
    {
        if (*g_options_ini[i].item == '*')
        {
            std::fprintf(fp_out, "# ==%s========\n", g_options_ini[i].item + 1);
        }
        else
        {
            std::fprintf(fp_out,"%s = ",g_options_ini[i].item);
            if (!g_option_def_vals[i])
            {
                std::fputs("#default of ", fp_out);
            }
            std::fprintf(fp_out,"%s\n",quote_string(option_value(static_cast<option_index>(i))));
            if (g_option_saved_vals[i])
            {
                if (g_option_saved_vals[i] != g_option_def_vals[i])
                {
                    std::free(g_option_saved_vals[i]);
                }
                g_option_saved_vals[i] = nullptr;
            }
        }
    }
    if (line)
    {
        /*std::putc('\n',fp_out);*/
        std::fputs(line,fp_out);
    }
    std::fclose(fp_out);

    safefree(filebuf);

    if (first_time)
    {
        if (fd_in >= 0)
        {
            std::sprintf(g_buf,"%s.old",filename);
            remove(g_buf);
            rename(filename,g_buf);
        }
        first_time = false;
    }
    else
    {
        remove(filename);
    }

    std::sprintf(g_buf,"%s.new",filename);
    rename(g_buf,filename);
}

const char *option_value(option_index num)
{
    switch (num)
    {
      case OI_USE_THREADS:
        return YESorNO(g_use_threads);
      case OI_USE_MOUSE:
        return YESorNO(g_use_mouse);
      case OI_MOUSE_MODES:
        return g_mouse_modes;
      case OI_USE_UNIV_SEL:
        return YESorNO(g_use_univ_selector);
      case OI_UNIV_SEL_CMDS:
        return s_univ_sel_cmds;
      case OI_UNIV_SEL_BTNS:
        return expand_mouse_buttons(g_univ_sel_btns,g_univ_sel_btn_cnt);
      case OI_UNIV_SEL_ORDER:
        return get_sel_order(SM_UNIVERSAL);
      case OI_UNIV_FOLLOW:
        return YESorNO(g_univ_follow);
        break;
      case OI_USE_NEWSRC_SEL:
        return YESorNO(g_use_newsrc_selector);
      case OI_NEWSRC_SEL_CMDS:
        return g_newsrc_sel_cmds;
      case OI_NEWSRC_SEL_BTNS:
        return expand_mouse_buttons(g_newsrc_sel_btns,g_newsrc_sel_btn_cnt);
      case OI_USE_ADD_SEL:
        return YESorNO(g_use_add_selector);
      case OI_ADD_SEL_CMDS:
        return g_add_sel_cmds;
      case OI_ADD_SEL_BTNS:
        return expand_mouse_buttons(g_add_sel_btns,g_add_sel_btn_cnt);
      case OI_USE_NEWSGROUP_SEL:
        return YESorNO(g_use_newsgroup_selector);
      case OI_NEWSGROUP_SEL_ORDER:
        return get_sel_order(SM_NEWSGROUP);
      case OI_NEWSGROUP_SEL_CMDS:
        return g_newsgroup_sel_cmds;
      case OI_NEWSGROUP_SEL_BTNS:
        return expand_mouse_buttons(g_newsgroup_sel_btns,g_newsgroup_sel_btn_cnt);
      case OI_NEWSGROUP_SEL_STYLES:
      {
        char* s = g_sel_grp_dmode;
        while (s[-1] != '*')
        {
            s--;
        }
        return s;
      }
      case OI_USE_NEWS_SEL:
        if (g_use_news_selector < 1)
        {
            return YESorNO(g_use_news_selector+1);
        }
        std::sprintf(g_buf,"%d",g_use_news_selector);
        return g_buf;
      case OI_NEWS_SEL_MODE:
      {
        const sel_mode save_sel_mode = g_sel_mode;
        const int save_Threaded = g_threaded_group;
        g_threaded_group = true;
        set_selector(g_sel_defaultmode, SS_MAGIC_NUMBER);
        const char *s = g_sel_mode_string;
        g_sel_mode = save_sel_mode;
        g_threaded_group = save_Threaded;
        set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
        return s;
      }
      case OI_NEWS_SEL_ORDER:
        return get_sel_order(g_sel_defaultmode);
      case OI_NEWS_SEL_CMDS:
        return g_news_sel_cmds;
      case OI_NEWS_SEL_BTNS:
        return expand_mouse_buttons(g_news_sel_btns,g_news_sel_btn_cnt);
      case OI_NEWS_SEL_STYLES:
      {
        char* s = g_sel_art_dmode;
        while (s[-1] != '*')
        {
            s--;
        }
        return s;
      }
      case OI_OPTION_SEL_CMDS:
        return g_option_sel_cmds;
      case OI_OPTION_SEL_BTNS:
        return expand_mouse_buttons(g_option_sel_btns,g_option_sel_btn_cnt);
      case OI_AUTO_SAVE_NAME:
        return YESorNO(!std::strcmp(get_val_const("SAVEDIR",SAVEDIR),"%p/%c"));
      case OI_BKGND_THREADING:
        return YESorNO(!g_thread_always);
      case OI_AUTO_ARROW_MACROS:
        switch (g_auto_arrow_macros)
        {
          case 2:
            return "regular";
          case 1:
            return "alternate";
          default:
            return YESorNO(false);
        }
      case OI_READ_BREADTH_FIRST:
        return YESorNO(g_breadth_first);
      case OI_BKGND_SPINNER:
        return YESorNO(g_bkgnd_spinner);
      case OI_CHECKPOINT_NEWSRC_FREQUENCY:
        std::sprintf(g_buf,"%d",g_docheckwhen);
        return g_buf;
      case OI_SAVE_DIR:
        return g_savedir.empty() ? "%./News" : g_savedir.c_str();
      case OI_ERASE_SCREEN:
        return YESorNO(g_erase_screen);
      case OI_NOVICE_DELAYS:
        return YESorNO(g_novice_delays);
      case OI_CITED_TEXT_STRING:
        return g_indstr.c_str();
      case OI_GOTO_LINE_NUM:
        std::sprintf(g_buf,"%d",g_gline+1);
        return g_buf;
      case OI_FUZZY_NEWSGROUP_NAMES:
        return YESorNO(g_fuzzy_get);
      case OI_HEADER_MAGIC:
        return magic_list();
      case OI_HEADER_HIDING:
        return hidden_list();
      case OI_INITIAL_ARTICLE_LINES:
        if (!g_option_def_vals[OI_INITIAL_ARTICLE_LINES])
        {
            return "$LINES";
        }
        std::sprintf(g_buf,"%d",g_initlines);
        return g_buf;
      case OI_APPEND_UNSUBSCRIBED_GROUPS:
        return YESorNO(g_append_unsub);
      case OI_FILTER_CONTROL_CHARACTERS:
        return YESorNO(!g_dont_filter_control);
      case OI_JOIN_SUBJECT_LINES:
        if (g_join_subject_len)
        {
            std::sprintf(g_buf,"%d",g_join_subject_len);
            return g_buf;
        }
        return YESorNO(false);
      case OI_IGNORE_THRU_ON_SELECT:
        return YESorNO(g_kill_thru_kludge);
      case OI_AUTO_GROW_GROUPS:
        return YESorNO(!g_keep_the_group_static);
      case OI_MUCK_UP_CLEAR:
        return YESorNO(g_muck_up_clear);
      case OI_ERASE_EACH_LINE:
        return YESorNO(g_erase_each_line);
      case OI_SAVEFILE_TYPE:
        return g_mbox_always? "mail" : (g_norm_always? "norm" : "ask");
      case OI_PAGER_LINE_MARKING:
        if (g_marking == NOMARKING)
        {
            return YESorNO(false);
        }
        if (g_marking_areas != HALFPAGE_MARKING)
        {
            std::sprintf(g_buf,"%d", static_cast<int>(g_marking_areas));
        }
        else
        {
            *g_buf = '\0';
        }
        if (g_marking == UNDERLINE)
        {
            std::strcat(g_buf,"underline");
        }
        else
        {
            std::strcat(g_buf,"standout");
        }
        return g_buf;
      case OI_OLD_MTHREADS_DATABASE:
        if (g_olden_days <= 1)
        {
            return YESorNO(g_olden_days);
        }
        std::sprintf(g_buf,"%d",g_olden_days);
        return g_buf;
      case OI_SELECT_MY_POSTS:
        switch (g_auto_select_postings)
        {
          case '+':
            return "thread";
          case 'p':
            return "parent";
          case '.':
            return "subthread";
          default:
            break;
        }
        return YESorNO(false);
      case OI_MULTIPART_SEPARATOR:
        return g_multipart_separator.c_str();
      case OI_AUTO_VIEW_INLINE:
        return YESorNO(g_auto_view_inline);
      case OI_NEWGROUP_CHECK:
        return YESorNO(!g_quickstart);
      case OI_RESTRICTION_INCLUDES_EMPTIES:
        return YESorNO(g_empty_only_char == 'o');
      case OI_CHARSET:
        return g_charsets.c_str();
      case OI_INITIAL_GROUP_LIST:
        if (g_suppress_cn)
        {
            return YESorNO(false);
        }
        std::sprintf(g_buf,"%d",g_countdown);
        return g_buf;
      case OI_RESTART_AT_LAST_GROUP:
        return YESorNO(g_findlast != 0);
      case OI_SCANMODE_COUNT:
        std::sprintf(g_buf,"%d",g_scanon);
        return g_buf;
      case OI_TERSE_OUTPUT:
        return YESorNO(!g_verbose);
      case OI_EAT_TYPEAHEAD:
        return YESorNO(!g_allow_typeahead);
      case OI_COMPRESS_SUBJECTS:
        return YESorNO(!g_unbroken_subjects);
      case OI_VERIFY_INPUT:
        return YESorNO(g_verify);
      case OI_ARTICLE_TREE_LINES:
        std::sprintf(g_buf,"%d",g_max_tree_lines);
        return g_buf;
      case OI_WORD_WRAP_MARGIN:
        if (g_word_wrap_offset >= 0)
        {
            std::sprintf(g_buf,"%d",g_word_wrap_offset);
            return g_buf;
        }
        return YESorNO(false);
      case OI_DEFAULT_REFETCH_TIME:
        return secs2text(g_def_refetch_secs);
      case OI_ART_PAGER_BTNS:
        return expand_mouse_buttons(g_art_pager_btns,g_art_pager_btn_cnt);
      case OI_SCAN_ITEMNUM:
        return YESorNO(g_s_itemnum);
      case OI_SCAN_VI:
        return YESorNO(g_s_mode_vi);
      case OI_SCANA_FOLLOW:
        return YESorNO(g_sa_follow);
      case OI_SCANA_FOLD:
        return YESorNO(g_sa_mode_fold);
      case OI_SCANA_UNZOOMFOLD:
        return YESorNO(g_sa_unzoomrefold);
      case OI_SCANA_MARKSTAY:
        return YESorNO(g_sa_mark_stay);
      case OI_SCANA_DISPANUM:
        return YESorNO(g_sa_mode_desc_artnum);
      case OI_SCANA_DISPAUTHOR:
        return YESorNO(g_sa_mode_desc_author);
      case OI_SCANA_DISPSCORE:
        return YESorNO(g_sa_mode_desc_score);
      case OI_SCANA_DISPSUBCNT:
        return YESorNO(g_sa_mode_desc_threadcount);
      case OI_SCANA_DISPSUBJ:
        return YESorNO(g_sa_mode_desc_subject);
      case OI_SCANA_DISPSUMMARY:
        return YESorNO(g_sa_mode_desc_summary);
      case OI_SCANA_DISPKEYW:
        return YESorNO(g_sa_mode_desc_keyw);
      case OI_SC_VERBOSE:
        return YESorNO(g_sf_verbose);
      case OI_USE_SEL_NUM:
        return YESorNO(g_use_sel_num);
      case OI_SEL_NUM_GOTO:
        return YESorNO(g_sel_num_goto);
      default:
        std::printf("*** Internal error: Unknown Option ***\n");
        break;
    }
    return "<UNKNOWN>";
}

static char *hidden_list()
{
    g_buf[0] = '\0';
    g_buf[1] = '\0';
    for (int i = 1; i < g_user_htype_cnt; i++)
    {
        std::sprintf(g_buf+std::strlen(g_buf),",%s%s", g_user_htype[i].flags? "" : "!",
                g_user_htype[i].name);
    }
    return g_buf+1;
}

static char *magic_list()
{
    g_buf[0] = '\0';
    g_buf[1] = '\0';
    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        if (!(g_htype[i].flags & HT_MAGIC) != !(g_htype[i].flags & HT_DEFMAGIC))
        {
            std::sprintf(g_buf+std::strlen(g_buf),",%s%s",
                    (g_htype[i].flags & HT_DEFMAGIC)? "!" : "",
                    g_htype[i].name);
        }
    }
    return g_buf+1;
}

static void set_header_list(headtype_flags flag, headtype_flags defflag, const char *str)
{
    bool setit;

    if (flag == HT_HIDE || flag == HT_DEFHIDE)
    {
        /* Free old g_user_htype list */
        while (g_user_htype_cnt > 1)
        {
            std::free(g_user_htype[--g_user_htype_cnt].name);
        }
        std::memset((char*)g_user_htypeix,0,sizeof g_user_htypeix);
    }

    if (!*str)
    {
        str = " ";
    }
    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        g_htype[i].flags = ((g_htype[i].flags & defflag)
                       ? (g_htype[i].flags | flag)
                       : (g_htype[i].flags & ~flag));
    }
    std::unique_ptr<char[]> buffer(new char[std::strlen(str) + 1]);
    char *buff = buffer.get();
    std::strcpy(buff, str);
    for (;;)
    {
        char *cp = std::strchr(buff, ',');
        if (cp != nullptr)
        {
            *cp = '\0';
        }
        if (*buff == '!')
        {
            setit = false;
            buff++;
        }
        else
        {
            setit = true;
        }
        set_header(buff,flag,setit);
        if (!cp)
        {
            break;
        }
        *cp = ',';
        buff = cp+1;
    }
}

void set_header(const char *s, headtype_flags flag, bool setit)
{
    int len = std::strlen(s);
    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        if (!len || string_case_equal(s, g_htype[i].name, len))
        {
            if (setit && (flag != HT_MAGIC || (g_htype[i].flags & HT_MAGICOK)))
            {
                g_htype[i].flags |= flag;
            }
            else
            {
                g_htype[i].flags &= ~flag;
            }
        }
    }
    if (flag == HT_HIDE && *s && isalpha(*s))
    {
        char ch = std::isupper(*s)? std::tolower(*s) : *s;
        int  add_at = 0;
        int  killed = 0;
        bool save_it = true;
        for (int i = g_user_htypeix[ch - 'a']; *g_user_htype[i].name == ch; i--)
        {
            if (len <= g_user_htype[i].length //
                && string_case_equal(s, g_user_htype[i].name, len))
            {
                std::free(g_user_htype[i].name);
                g_user_htype[i].name = nullptr;
                killed = i;
            }
            else if (len > g_user_htype[i].length //
                     && string_case_equal(s, g_user_htype[i].name, g_user_htype[i].length))
            {
                if (!add_at)
                {
                    if (g_user_htype[i].flags == (setit? flag : 0))
                    {
                        save_it = false;
                    }
                    add_at = i+1;
                }
            }
        }
        if (save_it)
        {
            if (!killed || (add_at && g_user_htype[add_at].name))
            {
                if (g_user_htype_cnt >= g_user_htype_max)
                {
                    g_user_htype = (USER_HEADTYPE*)
                        std::realloc(g_user_htype, (g_user_htype_max += 10)
                                            * sizeof (USER_HEADTYPE));
                }
                if (!add_at)
                {
                    add_at = g_user_htypeix[ch - 'a']+1;
                    if (add_at == 1)
                    {
                        add_at = g_user_htype_cnt;
                    }
                }
                for (int i = g_user_htype_cnt; i > add_at; i--)
                {
                    g_user_htype[i] = g_user_htype[i - 1];
                }
                g_user_htype_cnt++;
            }
            else if (!add_at)
            {
                add_at = killed;
            }
            g_user_htype[add_at].length = len;
            g_user_htype[add_at].flags = setit? flag : 0;
            g_user_htype[add_at].name = savestr(s);
            for (char *tmp = g_user_htype[add_at].name; *tmp; tmp++)
            {
                if (std::isupper(*tmp))
                {
                    *tmp = static_cast<char>(std::tolower(*tmp));
                }
            }
        }
        if (killed)
        {
            while (killed < g_user_htype_cnt && g_user_htype[killed].name != nullptr)
            {
                killed++;
            }
            for (int i = killed + 1; i < g_user_htype_cnt; i++)
            {
                if (g_user_htype[i].name != nullptr)
                {
                    g_user_htype[killed++] = g_user_htype[i];
                }
            }
            g_user_htype_cnt = killed;
        }
        std::memset((char*)g_user_htypeix,0,sizeof g_user_htypeix);
        for (int i = 1; i < g_user_htype_cnt; i++)
        {
            g_user_htypeix[*g_user_htype[i].name - 'a'] = i;
        }
    }
}

static int parse_mouse_buttons(char **cpp, const char *btns)
{
    char* t = *cpp;
    int cnt = 0;

    safefree(t);
    btns = skip_eq(btns, ' ');
    *cpp = safemalloc(std::strlen(btns) + 1);
    t = *cpp;

    while (*btns)
    {
        if (*btns == '[')
        {
            if (!btns[1])
            {
                break;
            }
            while (*btns)
            {
                if (*btns == '\\' && btns[1])
                {
                    btns++;
                }
                else if (*btns == ']')
                {
                    break;
                }
                *t++ = *btns++;
            }
            *t++ = '\0';
            if (*btns)
            {
                btns = skip_eq(++btns, ' ');
            }
        }
        while (*btns && *btns != ' ')
        {
            *t++ = *btns++;
        }
        btns = skip_eq(btns, ' ');
        *t++ = '\0';
        cnt++;
    }

    return cnt;
}

static char *expand_mouse_buttons(char *cp, int cnt)
{
    *g_buf = '\0';
    while (cnt--)
    {
        if (*cp == '[')
        {
            int len = std::strlen(cp);
            cp[len] = ']';
            safecat(g_buf,cp,sizeof g_buf);
            cp += len;
            *cp++ = '\0';
        }
        else
        {
            safecat(g_buf, cp, sizeof g_buf);
        }
        cp += std::strlen(cp)+1;
    }
    return g_buf;
}

const char *quote_string(const char *val)
{
    static std::string buff;

    bool needs_quotes = false;
    int  ticks = 0;
    int  quotes = 0;
    int  backslashes = 0;
    buff.clear();

    if (std::isspace(*val))
    {
        needs_quotes = true;
    }
    for (const char *cp = val; *cp; cp++)
    {
        switch (*cp)
        {
          case ' ':
          case '\t':
            if (!cp[1] || isspace(cp[1]))
            {
                needs_quotes = true;
            }
            break;
          case '\n':
          case '#':
            needs_quotes = true;
            break;
          case '\'':
            ticks++;
            break;
          case '"':
            quotes++;
            break;
          case '\\':
            backslashes++;
            break;
        }
    }

    if (needs_quotes || ticks || quotes || backslashes)
    {
        const char usequote = quotes > ticks? '\'' : '"';
        buff = usequote;
        while (*val)
        {
            if (*val == usequote || *val == '\\')
            {
                buff += '\\';
            }
            buff += *val++;
        }
        buff += usequote;
        return buff.c_str();
    }
    return val;
}

void cwd_check()
{
    char tmpbuf[LBUFLEN];

    if (g_privdir.empty())
    {
        g_privdir = filexp("~/News");
    }
    std::strcpy(tmpbuf,g_privdir.c_str());
    if (change_dir(g_privdir))
    {
        safecpy(tmpbuf,filexp(g_privdir.c_str()),sizeof tmpbuf);
        if (makedir(tmpbuf, MD_DIR) || change_dir(tmpbuf))
        {
            interp(g_cmd_buf, (sizeof g_cmd_buf), "%~/News");
            if (makedir(g_cmd_buf, MD_DIR))
            {
                std::strcpy(tmpbuf, g_home_dir);
            }
            else
            {
                std::strcpy(tmpbuf, g_cmd_buf);
            }
            change_dir(tmpbuf);
            if (g_verbose)
            {
                std::printf("Cannot make directory %s--\n"
                      "        articles will be saved to %s\n"
                      "\n",
                      g_privdir.c_str(), tmpbuf);
            }
            else
            {
                std::printf("Can't make %s--\n"
                      "        using %s\n"
                      "\n",
                      g_privdir.c_str(), tmpbuf);
            }
        }
    }
    trn_getwd(tmpbuf, sizeof(tmpbuf));
    if (eaccess(tmpbuf, 2))
    {
        if (g_verbose)
        {
            std::printf("Current directory %s is not writeable--\n"
                  "        articles will be saved to home directory\n"
                  "\n",
                  tmpbuf);
        }
        else
        {
            std::printf("%s not writeable--using ~\n\n", tmpbuf);
        }
        std::strcpy(tmpbuf,g_home_dir);
    }
    g_privdir = tmpbuf;
}
