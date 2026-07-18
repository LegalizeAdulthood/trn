/* opt.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/opt.h>

#include <file_contents.h>

#include <config/common.h>
#include <config/env.h>
#include <config/fdio.h>
#include <config/string_case_compare.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/cache.h>
#include <trn/change_dir.h>
#include <trn/charsubst.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/IniDocument.h>
#include <trn/IniSectionValues.h>
#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/only.h>
#include <trn/OptionApplier.h>
#include <trn/OptionCatalog.h>
#include <trn/OptionDraft.h>
#include <trn/respond.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rt-wumpus.h>
#include <trn/rthread.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/scorefile.h>
#include <trn/string-algos.h>
#include <trn/sw.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

static void opt_file(const char *filename, bool bleat);

CompiledRegex g_opt_compex;
std::string   g_ini_file;
OptionValueList g_option_def_vals;
OptionValueList g_option_saved_vals;
OptionFlags *g_option_flags{};
OptionDraft *g_option_draft{};
int          g_sel_page_op{};

static char s_univ_sel_cmds[3]{"Z>"};

static std::string hidden_list();
static std::string magic_list();
static void  set_header_list(HeaderTypeFlags flag, HeaderTypeFlags defflag, std::string_view str);
static int   parse_mouse_buttons(char **cpp, const char *btns);
static std::string expand_mouse_buttons(const char *cp, int cnt);

void opt_init(int argc, char *argv[], char *tcbuf)
{
    g_sel_grp_display_mode = "slm";
    g_sel_art_display_mode = "lmds";
    g_sel_grp_display_mode_index = 0;
    g_sel_art_display_mode_index = 0;
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
    g_news_sel_btn_cnt =
        parse_mouse_buttons(&g_news_sel_btns, "[Top]^ [Bot]$ [PgUp]< [PgDn]> [KillPg]D [ OK ]Z [Quit]q [Help]?");
    g_art_pager_btn_cnt = parse_mouse_buttons(&g_art_pager_btns, "[Next]n [Sel]+ [Quit]q [Help]h");

    if (argc >= 2 && std::string_view{argv[1]} == "-c")
    {
        g_check_flag = true; // so we can optimize for -c
    }
    interp(tcbuf, TCBUF_SIZE, GLOBAL_INIT);
    opt_file(tcbuf, false);
    *tcbuf = '\0';

    const OptionCatalog catalog;
    const int            len = catalog.option_limit();
    g_option_def_vals = OptionValueList(static_cast<std::size_t>(len));
    // Set DEFHIDE and DEFMAGIC to current values and clear g_user_htype list
    set_header_list(HT_DEF_HIDE, HT_HIDE, "");
    set_header_list(HT_DEF_MAGIC, HT_MAGIC, "");

    g_ini_file = file_exp(g_use_threads ? get_val_const("TRNRC", "%+/trnrc") : get_val_const("RNRC", "%+/rnrc"));

    const fs::path trn_dir{file_exp("%+")};
    std::error_code error;
    if (!fs::is_directory(trn_dir, error))
    {
        std::printf("Creating the directory %s.\n", trn_dir.string().c_str());
        if (make_dir(trn_dir.string().c_str(), MD_DIR))
        {
            std::printf("Unable to create `%s'.\n", trn_dir.string().c_str());
            finalize(1);
        }
    }
    error.clear();
    if (fs::exists(g_ini_file, error))
    {
        opt_file(g_ini_file.c_str(), true);
    }
    char *s;
    if (!g_use_threads || (s = get_val("TRNINIT")) == nullptr)
    {
        s = get_val("RNINIT");
    }
    const std::string_view switches{s != nullptr ? s : ""};
    if (!switches.empty())
    {
        if (switches.front() == '-' || switches.front() == '+' ||
            std::isspace(static_cast<unsigned char>(switches.front())))
        {
            sw_list(switches);
        }
        else
        {
            sw_file(switches);
        }
    }
    g_option_saved_vals = OptionValueList(static_cast<std::size_t>(len));
    g_option_flags = new OptionFlags[len];
    std::fill_n(g_option_flags, len, OF_NONE);

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            decode_switch(argv[i]);
        }
    }
    g_opt_compex.init_compex();

    g_priv_dir = file_exp("~/News");
}

void opt_final()
{
    g_priv_dir.clear();
    delete[] g_option_flags;
    g_option_flags = nullptr;
    g_option_saved_vals.clear();
    g_ini_file.clear();
    g_option_def_vals.clear();
    safe_free0(g_art_pager_btns);
    safe_free0(g_news_sel_btns);
    safe_free0(g_newsgroup_sel_btns);
    safe_free0(g_option_sel_btns);
    safe_free0(g_add_sel_btns);
    safe_free0(g_newsrc_sel_btns);
    safe_free0(g_univ_sel_btns);
    g_sel_art_display_mode.clear();
    g_sel_grp_display_mode.clear();
    g_sel_art_display_mode_index = 0;
    g_sel_grp_display_mode_index = 0;
}

static void opt_file(const char *filename, bool bleat)
{
    std::string filebuf = file_contents(filename);

    if (!filebuf.empty())
    {
        IniDocument document{std::move(filebuf), filename};
        for (const IniSection section : document)
        {
            if (section.has_condition())
            {
                if (!check_ini_cond(section.condition()))
                {
                    continue;
                }
            }

            const std::string_view section_name = section.name();
            if (section_name == "options")
            {
                IniSectionValues values;
                parse_ini_section(section, OptionCatalog().schema(), values);
                OptionApplier{}.apply(values);
            }
            else if (section_name == "environment")
            {
                for (const IniSetting setting : section)
                {
                    set_env_var(setting.name(), setting.value());
                }
            }
            else if (section_name == "termcap")
            {
                for (const IniSetting setting : section)
                {
                    const std::string capability{setting.name()};
                    const std::string value = setting.value();
                    add_tc_string(capability.c_str(), value.c_str());
                }
            }
            else if (section_name == "attribute")
            {
                for (const IniSetting setting : section)
                {
                    std::string value = setting.value();
                    color_rc_attribute(setting.name(), value.data());
                }
            }
        }
    }
    else if (bleat && !fs::exists(filename))
    {
        std::printf(g_cant_open, filename);
        // term_down(1);
    }
}

inline bool is_yes(const char *s)
{
    return *s == 'y' || *s == 'Y';
}
inline bool is_no(const char *s)
{
    return *s == 'n' || *s == 'N';
}

void set_options(const OptionDraft &draft)
{
    OptionApplier{}.apply(draft);
}

bool option_draft_contains(OptionIndex num)
{
    return g_option_draft != nullptr && g_option_draft->contains(num);
}

const char *option_draft_value(OptionIndex num)
{
    return g_option_draft != nullptr ? g_option_draft->value(num) : nullptr;
}

void set_option(OptionIndex num, const char *s)
{
    OptionApplier{}.apply(num, s);
}

void apply_global_option(OptionIndex num, const char *s)
{
    if (!g_option_saved_vals.empty())
    {
        if (!g_option_saved_vals[num])
        {
            g_option_saved_vals[num] = option_value(num);
            if (!g_option_def_vals[num])
            {
                g_option_def_vals[num] = g_option_saved_vals[num];
            }
        }
    }
    else if (!g_option_def_vals.empty())
    {
        if (!g_option_def_vals[num])
        {
            g_option_def_vals[num] = option_value(num);
        }
    }
    switch (num)
    {
    case OI_USE_THREADS:
        g_use_threads = is_yes(s);
        break;

    case OI_USE_MOUSE:
        g_use_mouse = is_yes(s);
        if (g_use_mouse)
        {
            // set up the Xterm mouse sequence
            set_macro("\033[M+3","\003");
        }
        break;

    case OI_MOUSE_MODES:
        g_mouse_modes = s;
        break;

    case OI_USE_UNIV_SEL:
        g_use_univ_selector = is_yes(s);
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
        g_univ_follow = is_yes(s);
        break;

    case OI_USE_NEWSRC_SEL:
        g_use_newsrc_selector = is_yes(s);
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
        g_use_add_selector = is_yes(s);
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
        g_use_newsgroup_selector = is_yes(s);
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
        g_sel_grp_display_mode = s;
        g_sel_grp_display_mode_index = 0;
        break;

    case OI_USE_NEWS_SEL:
        if (std::isdigit(*s))
        {
            g_use_news_selector = std::atoi(s);
        }
        else
        {
            g_use_news_selector = static_cast<int>(is_yes(s)) - 1;
        }
        break;

    case OI_NEWS_SEL_MODE:
    {
        const SelectionMode save_sel_mode = g_sel_mode;
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
        set_sel_order(g_sel_default_mode,s);
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
        g_sel_art_display_mode = s;
        g_sel_art_display_mode_index = 0;
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
        if (!g_check_flag)
        {
            if (is_yes(s))
            {
                set_env_var("SAVEDIR", "%p/%c");
                set_env_var("SAVENAME", "%a");
            }
            else if (get_env_var("SAVEDIR") == "%p/%c" //
                     && get_env_var("SAVENAME") == "%a")
            {
                set_env_var("SAVEDIR", "%p");
                set_env_var("SAVENAME", "%^C");
            }
        }
        break;

    case OI_BACKGROUND_THREADING:
        g_thread_always = !is_yes(s);
        break;

    case OI_AUTO_ARROW_MACROS:
    {
        int prev = g_auto_arrow_macros;
        if (is_yes(s) || *s == 'r' || *s == 'R')
        {
            g_auto_arrow_macros = 2;
        }
        else
        {
            g_auto_arrow_macros = !is_no(s);
        }
        if (g_mode != MM_INITIALIZING && g_auto_arrow_macros != prev)
        {
            char tmpbuf[1024];
            arrow_macros(tmpbuf);
        }
        break;
    }

    case OI_READ_BREADTH_FIRST:
        g_breadth_first = is_yes(s);
        break;

    case OI_BACKGROUND_SPINNER:
        g_bkgnd_spinner = is_yes(s);
        break;

    case OI_CHECKPOINT_NEWSRC_FREQUENCY:
        g_do_check_when = std::atoi(s);
        break;

    case OI_SAVE_DIR:
        if (!g_check_flag)
        {
            g_save_dir = s;
            if (!g_priv_dir.empty())
            {
                change_dir(g_priv_dir);
            }
            g_priv_dir = file_exp(s);
        }
        break;

    case OI_ERASE_SCREEN:
        g_erase_screen = is_yes(s);
        break;

    case OI_NOVICE_DELAYS:
        g_novice_delays = is_yes(s);
        break;

    case OI_CITED_TEXT_STRING:
        g_indent_string = s;
        break;

    case OI_GOTO_LINE_NUM:
        g_g_line = std::atoi(s)-1;
        break;

    case OI_FUZZY_NEWSGROUP_NAMES:
        g_fuzzy_get = is_yes(s);
        break;

    case OI_HEADER_MAGIC:
        if (!g_check_flag)
        {
            set_header_list(HT_MAGIC, HT_DEF_MAGIC, s);
        }
        break;

    case OI_HEADER_HIDING:
        set_header_list(HT_HIDE, HT_DEF_HIDE, s);
        break;

    case OI_INITIAL_ARTICLE_LINES:
        g_init_lines = ArticleLine{std::atoi(s)};
        break;

    case OI_APPEND_UNSUBSCRIBED_GROUPS:
        g_append_unsub = is_yes(s);
        break;

    case OI_FILTER_CONTROL_CHARACTERS:
        g_dont_filter_control = !is_yes(s);
        break;

    case OI_JOIN_SUBJECT_LINES:
        if (std::isdigit(*s))
        {
            change_join_subject_len(std::atoi(s));
        }
        else
        {
            change_join_subject_len(is_yes(s)? 30 : 0);
        }
        break;

    case OI_IGNORE_THRU_ON_SELECT:
        g_kill_thru_kludge = is_yes(s);
        break;

    case OI_AUTO_GROW_GROUPS:
        g_keep_the_group_static = !is_yes(s);
        break;

    case OI_MUCK_UP_CLEAR:
        g_muck_up_clear = is_yes(s);
        break;

    case OI_ERASE_EACH_LINE:
        g_erase_each_line = is_yes(s);
        break;

    case OI_SAVE_FILE_TYPE:
        g_mbox_always = (*s == 'm' || *s == 'M');
        g_norm_always = (*s == 'n' || *s == 'N');
        break;

    case OI_PAGER_LINE_MARKING:
        if (std::isdigit(*s))
        {
            g_marking_areas = static_cast<MarkingAreas>(std::atoi(s));
        }
        else
        {
            g_marking_areas = HALF_PAGE_MARKING;
        }
        if (is_no(s))
        {
            g_marking = NO_MARKING;
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
            g_olden_days = is_yes(s);
        }
        break;

    case OI_SELECT_MY_POSTS:
        if (is_no(s))
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
        g_auto_view_inline = is_yes(s);
        break;

    case OI_NEW_GROUP_CHECK:
        g_quick_start = !is_yes(s);
        break;

    case OI_RESTRICTION_INCLUDES_EMPTIES:
        g_empty_only_char = is_yes(s)? 'o' : 'O';
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
            g_suppress_cn = is_no(s);
            if (!g_suppress_cn)
            {
                g_countdown = 5;
            }
        }
        break;

    case OI_RESTART_AT_LAST_GROUP:
        g_find_last = is_yes(s) * (g_mode == MM_INITIALIZING? 1 : -1);
        break;

    case OI_SCAN_MODE_COUNT:
        if (std::isdigit(*s))
        {
            g_scan_on = std::atoi(s);
        }
        else
        {
            g_scan_on = is_yes(s)*3;
        }
        break;

    case OI_TERSE_OUTPUT:
        g_verbose = !is_yes(s);
        if (!g_verbose)
        {
            g_novice_delays = false;
        }
        break;

    case OI_EAT_TYPEAHEAD:
        g_allow_typeahead = !is_yes(s);
        break;

    case OI_COMPRESS_SUBJECTS:
        g_unbroken_subjects = !is_yes(s);
        break;

    case OI_VERIFY_INPUT:
        g_verify = is_yes(s);
        break;

    case OI_ARTICLE_TREE_LINES:
        if (std::isdigit(*s))
        {
            g_max_tree_lines = std::atoi(s);
            g_max_tree_lines = std::min(g_max_tree_lines, 11);
        }
        else
        {
            g_max_tree_lines = is_yes(s) * 6;
        }
        break;

    case OI_WORD_WRAP_MARGIN:
        if (std::isdigit(*s))
        {
            g_word_wrap_offset = std::atoi(s);
        }
        else if (is_yes(s))
        {
            g_word_wrap_offset = 8;
        }
        else
        {
            g_word_wrap_offset = -1;
        }
        break;

    case OI_DEFAULT_REFETCH_TIME:
        g_def_refetch_secs = text_to_secs(s, DEFAULT_REFETCH_SECS);
        break;

    case OI_ART_PAGER_BTNS:
        g_art_pager_btn_cnt = parse_mouse_buttons(&g_art_pager_btns,s);
        break;

    case OI_SCAN_ITEM_NUM:
        g_s_item_num = is_yes(s);
        break;

    case OI_SCAN_VI:
        g_s_mode_vi = is_yes(s);
        break;

    case OI_SCAN_ART_FOLLOW:
        g_sa_follow = is_yes(s);
        break;

    case OI_SCAN_ART_FOLD:
        g_sa_mode_fold = is_yes(s);
        break;

    case OI_SCAN_ART_UNZOOM_FOLD:
        g_sa_unzoom_refold = is_yes(s);
        break;

    case OI_SCAN_ART_MARK_STAY:
        g_sa_mark_stay = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_ART_NUM:
        g_sa_mode_desc_art_num = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_AUTHOR:
        g_sa_mode_desc_author = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_SCORE:
        g_sa_mode_desc_score = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_SUB_COUNT:
        g_sa_mode_desc_thread_count = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_SUBJ:
        break;

    case OI_SCAN_ART_DISP_SUMMARY:
        g_sa_mode_desc_summary = is_yes(s);
        break;

    case OI_SCAN_ART_DISP_KEYW:
        g_sa_mode_desc_keyw = is_yes(s);
        break;

    case OI_SC_VERBOSE:
        g_sf_verbose = is_yes(s);
        break;

    case OI_USE_SEL_NUM:
        g_use_sel_num = is_yes(s);
        break;

    case OI_SEL_NUM_GOTO:
        g_sel_num_goto = is_yes(s);
        break;

    default:
        std::printf("*** Internal error: Unknown Option ***\n");
        break;
    }
}

void save_options(const char *filename)
{
    std::string filebuf;
    char* line = nullptr;
    static bool first_time = true;
    const fs::path filename_path{filename};
    fs::path new_filename{filename_path};
    new_filename += ".new";
    fs::path old_filename{filename_path};
    old_filename += ".old";

    std::FILE *fp_out = std::fopen(new_filename.string().c_str(), "w");
    if (!fp_out)
    {
        std::printf(g_cant_create,new_filename.string().c_str());
        return;
    }
    std::ifstream input{filename_path};
    const bool had_existing_file = input.good();
    if (had_existing_file)
    {
        filebuf.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        input.close();
        char* cp;
        char* nlp = nullptr;
        char* comments = nullptr;

        for (line = filebuf.empty() ? nullptr : filebuf.data(); line && *line; line = nlp)
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
    const OptionCatalog catalog;
    for (int row = catalog.first_row(); row <= catalog.row_count(); row++)
    {
        if (catalog.is_group(row))
        {
            const std::string_view name = catalog.name(row);
            std::fprintf(fp_out, "# ==%.*s========\n", static_cast<int>(name.size()), name.data());
        }
        else
        {
            const OptionIndex      option = catalog.option(row);
            const std::string_view name = catalog.name(row);
            std::fprintf(fp_out, "%.*s = ", static_cast<int>(name.size()), name.data());
            if (!g_option_def_vals[option])
            {
                std::fputs("#default of ", fp_out);
            }
            std::fprintf(fp_out, "%s\n", quote_string(option_value(option)));
            if (g_option_saved_vals[option])
            {
                g_option_saved_vals[option].reset();
            }
        }
    }
    if (line)
    {
        // std::putc('\n',fp_out);
        std::fputs(line,fp_out);
    }
    std::fclose(fp_out);

    std::error_code error;
    if (first_time)
    {
        if (had_existing_file)
        {
            fs::remove(old_filename, error);
            fs::rename(filename_path, old_filename, error);
        }
        first_time = false;
    }
    else
    {
        fs::remove(filename_path, error);
    }

    fs::rename(new_filename, filename_path, error);
}

std::string option_value(OptionIndex num)
{
    switch (num)
    {
    case OI_USE_THREADS:
        return yes_or_no(g_use_threads);

    case OI_USE_MOUSE:
        return yes_or_no(g_use_mouse);

    case OI_MOUSE_MODES:
        return g_mouse_modes.c_str();

    case OI_USE_UNIV_SEL:
        return yes_or_no(g_use_univ_selector);

    case OI_UNIV_SEL_CMDS:
        return s_univ_sel_cmds;

    case OI_UNIV_SEL_BTNS:
        return expand_mouse_buttons(g_univ_sel_btns,g_univ_sel_btn_cnt);

    case OI_UNIV_SEL_ORDER:
        return get_sel_order(SM_UNIVERSAL);

    case OI_UNIV_FOLLOW:
        return yes_or_no(g_univ_follow);
        break;

    case OI_USE_NEWSRC_SEL:
        return yes_or_no(g_use_newsrc_selector);

    case OI_NEWSRC_SEL_CMDS:
        return g_newsrc_sel_cmds;

    case OI_NEWSRC_SEL_BTNS:
        return expand_mouse_buttons(g_newsrc_sel_btns,g_newsrc_sel_btn_cnt);

    case OI_USE_ADD_SEL:
        return yes_or_no(g_use_add_selector);

    case OI_ADD_SEL_CMDS:
        return g_add_sel_cmds;

    case OI_ADD_SEL_BTNS:
        return expand_mouse_buttons(g_add_sel_btns,g_add_sel_btn_cnt);

    case OI_USE_NEWSGROUP_SEL:
        return yes_or_no(g_use_newsgroup_selector);

    case OI_NEWSGROUP_SEL_ORDER:
        return get_sel_order(SM_NEWSGROUP);

    case OI_NEWSGROUP_SEL_CMDS:
        return g_newsgroup_sel_cmds;

    case OI_NEWSGROUP_SEL_BTNS:
        return expand_mouse_buttons(g_newsgroup_sel_btns,g_newsgroup_sel_btn_cnt);

    case OI_NEWSGROUP_SEL_STYLES:
        return g_sel_grp_display_mode.c_str();

    case OI_USE_NEWS_SEL:
        if (g_use_news_selector < 1)
        {
            return yes_or_no(g_use_news_selector+1);
        }
        return fmt::format("{}", g_use_news_selector);

    case OI_NEWS_SEL_MODE:
    {
        const SelectionMode save_sel_mode = g_sel_mode;
        const int save_Threaded = g_threaded_group;
        g_threaded_group = true;
        set_selector(g_sel_default_mode, SS_MAGIC_NUMBER);
        const std::string s{g_sel_mode_string};
        g_sel_mode = save_sel_mode;
        g_threaded_group = save_Threaded;
        set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
        return s;
    }

    case OI_NEWS_SEL_ORDER:
        return get_sel_order(g_sel_default_mode);

    case OI_NEWS_SEL_CMDS:
        return g_news_sel_cmds;

    case OI_NEWS_SEL_BTNS:
        return expand_mouse_buttons(g_news_sel_btns,g_news_sel_btn_cnt);

    case OI_NEWS_SEL_STYLES:
        return g_sel_art_display_mode.c_str();

    case OI_OPTION_SEL_CMDS:
        return g_option_sel_cmds;

    case OI_OPTION_SEL_BTNS:
        return expand_mouse_buttons(g_option_sel_btns,g_option_sel_btn_cnt);

    case OI_AUTO_SAVE_NAME:
        return yes_or_no(get_env_var("SAVEDIR", SAVEDIR) == "%p/%c");

    case OI_BACKGROUND_THREADING:
        return yes_or_no(!g_thread_always);

    case OI_AUTO_ARROW_MACROS:
        switch (g_auto_arrow_macros)
        {
        case 2:
            return "regular";

        case 1:
            return "alternate";

        default:
            return yes_or_no(false);
        }

    case OI_READ_BREADTH_FIRST:
        return yes_or_no(g_breadth_first);

    case OI_BACKGROUND_SPINNER:
        return yes_or_no(g_bkgnd_spinner);

    case OI_CHECKPOINT_NEWSRC_FREQUENCY:
        return fmt::format("{}", g_do_check_when);

    case OI_SAVE_DIR:
        return g_save_dir.empty() ? "%./News" : g_save_dir.c_str();

    case OI_ERASE_SCREEN:
        return yes_or_no(g_erase_screen);

    case OI_NOVICE_DELAYS:
        return yes_or_no(g_novice_delays);

    case OI_CITED_TEXT_STRING:
        return g_indent_string.c_str();

    case OI_GOTO_LINE_NUM:
        return fmt::format("{}", g_g_line + 1);

    case OI_FUZZY_NEWSGROUP_NAMES:
        return yes_or_no(g_fuzzy_get);

    case OI_HEADER_MAGIC:
        return magic_list();

    case OI_HEADER_HIDING:
        return hidden_list();

    case OI_INITIAL_ARTICLE_LINES:
        if (!g_option_def_vals[OI_INITIAL_ARTICLE_LINES])
        {
            return "$LINES";
        }
        return fmt::format("{}", g_init_lines.value_of());

    case OI_APPEND_UNSUBSCRIBED_GROUPS:
        return yes_or_no(g_append_unsub);

    case OI_FILTER_CONTROL_CHARACTERS:
        return yes_or_no(!g_dont_filter_control);

    case OI_JOIN_SUBJECT_LINES:
        if (g_join_subject_len)
        {
            return fmt::format("{}", g_join_subject_len);
        }
        return yes_or_no(false);

    case OI_IGNORE_THRU_ON_SELECT:
        return yes_or_no(g_kill_thru_kludge);

    case OI_AUTO_GROW_GROUPS:
        return yes_or_no(!g_keep_the_group_static);

    case OI_MUCK_UP_CLEAR:
        return yes_or_no(g_muck_up_clear);

    case OI_ERASE_EACH_LINE:
        return yes_or_no(g_erase_each_line);

    case OI_SAVE_FILE_TYPE:
        return g_mbox_always? "mail" : (g_norm_always? "norm" : "ask");

    case OI_PAGER_LINE_MARKING:
        if (g_marking == NO_MARKING)
        {
            return yes_or_no(false);
        }
        if (g_marking_areas != HALF_PAGE_MARKING)
        {
            return fmt::format("{}{}", static_cast<int>(g_marking_areas),
                               g_marking == UNDERLINE ? "underline" : "standout");
        }
        return g_marking == UNDERLINE ? "underline" : "standout";

    case OI_OLD_MTHREADS_DATABASE:
        if (g_olden_days <= 1)
        {
            return yes_or_no(g_olden_days);
        }
        return fmt::format("{}", g_olden_days);

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
        return yes_or_no(false);

    case OI_MULTIPART_SEPARATOR:
        return g_multipart_separator.c_str();

    case OI_AUTO_VIEW_INLINE:
        return yes_or_no(g_auto_view_inline);

    case OI_NEW_GROUP_CHECK:
        return yes_or_no(!g_quick_start);

    case OI_RESTRICTION_INCLUDES_EMPTIES:
        return yes_or_no(g_empty_only_char == 'o');

    case OI_CHARSET:
        return g_charsets.c_str();

    case OI_INITIAL_GROUP_LIST:
        if (g_suppress_cn)
        {
            return yes_or_no(false);
        }
        return fmt::format("{}", g_countdown);

    case OI_RESTART_AT_LAST_GROUP:
        return yes_or_no(g_find_last != 0);

    case OI_SCAN_MODE_COUNT:
        return fmt::format("{}", g_scan_on);

    case OI_TERSE_OUTPUT:
        return yes_or_no(!g_verbose);

    case OI_EAT_TYPEAHEAD:
        return yes_or_no(!g_allow_typeahead);

    case OI_COMPRESS_SUBJECTS:
        return yes_or_no(!g_unbroken_subjects);

    case OI_VERIFY_INPUT:
        return yes_or_no(g_verify);

    case OI_ARTICLE_TREE_LINES:
        return fmt::format("{}", g_max_tree_lines);

    case OI_WORD_WRAP_MARGIN:
        if (g_word_wrap_offset >= 0)
        {
            return fmt::format("{}", g_word_wrap_offset);
        }
        return yes_or_no(false);

    case OI_DEFAULT_REFETCH_TIME:
        return secs_to_text(g_def_refetch_secs);

    case OI_ART_PAGER_BTNS:
        return expand_mouse_buttons(g_art_pager_btns,g_art_pager_btn_cnt);

    case OI_SCAN_ITEM_NUM:
        return yes_or_no(g_s_item_num);

    case OI_SCAN_VI:
        return yes_or_no(g_s_mode_vi);

    case OI_SCAN_ART_FOLLOW:
        return yes_or_no(g_sa_follow);

    case OI_SCAN_ART_FOLD:
        return yes_or_no(g_sa_mode_fold);

    case OI_SCAN_ART_UNZOOM_FOLD:
        return yes_or_no(g_sa_unzoom_refold);

    case OI_SCAN_ART_MARK_STAY:
        return yes_or_no(g_sa_mark_stay);

    case OI_SCAN_ART_DISP_ART_NUM:
        return yes_or_no(g_sa_mode_desc_art_num);

    case OI_SCAN_ART_DISP_AUTHOR:
        return yes_or_no(g_sa_mode_desc_author);

    case OI_SCAN_ART_DISP_SCORE:
        return yes_or_no(g_sa_mode_desc_score);

    case OI_SCAN_ART_DISP_SUB_COUNT:
        return yes_or_no(g_sa_mode_desc_thread_count);

    case OI_SCAN_ART_DISP_SUBJ:
        return yes_or_no(g_sa_mode_desc_subject);

    case OI_SCAN_ART_DISP_SUMMARY:
        return yes_or_no(g_sa_mode_desc_summary);

    case OI_SCAN_ART_DISP_KEYW:
        return yes_or_no(g_sa_mode_desc_keyw);

    case OI_SC_VERBOSE:
        return yes_or_no(g_sf_verbose);

    case OI_USE_SEL_NUM:
        return yes_or_no(g_use_sel_num);

    case OI_SEL_NUM_GOTO:
        return yes_or_no(g_sel_num_goto);

    default:
        std::printf("*** Internal error: Unknown Option ***\n");
        break;
    }
    return "<UNKNOWN>";
}

static std::string hidden_list()
{
    std::string headers;
    headers.reserve(LINE_BUF_LEN);
    for (int i = 1; i < g_user_header_type_count; i++)
    {
        if (!headers.empty())
        {
            headers += ',';
        }
        if (!g_user_header_type[i].flags)
        {
            headers += '!';
        }
        headers += g_user_header_type[i].name;
    }
    return headers;
}

static std::string magic_list()
{
    std::string headers;
    headers.reserve(LINE_BUF_LEN);
    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        if (!(g_header_type[i].flags & HT_MAGIC) != !(g_header_type[i].flags & HT_DEF_MAGIC))
        {
            if (!headers.empty())
            {
                headers += ',';
            }
            if (g_header_type[i].flags & HT_DEF_MAGIC)
            {
                headers += '!';
            }
            headers += g_header_type[i].name;
        }
    }
    return headers;
}

static void set_header_list(HeaderTypeFlags flag, HeaderTypeFlags defflag, std::string_view str)
{
    bool setit;

    if (flag == HT_HIDE || flag == HT_DEF_HIDE)
    {
        // Free old g_user_htype list
        while (g_user_header_type_count > 1)
        {
            g_user_header_type[--g_user_header_type_count].name.clear();
        }
        std::memset((char*)g_user_header_type_index,0,sizeof g_user_header_type_index);
    }

    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        g_header_type[i].flags = ((g_header_type[i].flags & defflag)
                       ? (g_header_type[i].flags | flag)
                       : (g_header_type[i].flags & ~flag));
    }
    std::string buffer{str};
    if (buffer.empty())
    {
        buffer = " ";
    }
    char *buff = buffer.data();
    while (true)
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

void set_header(std::string_view s, HeaderTypeFlags flag, bool setit)
{
    const int len = static_cast<int>(s.size());
    for (int i = HEAD_FIRST; i < HEAD_LAST; i++)
    {
        if (!len || string_case_equal(s.data(), g_header_type[i].name.c_str(), len))
        {
            if (setit && (flag != HT_MAGIC || (g_header_type[i].flags & HT_MAGIC_OK)))
            {
                g_header_type[i].flags |= flag;
            }
            else
            {
                g_header_type[i].flags &= ~flag;
            }
        }
    }
    if (flag == HT_HIDE && !s.empty() && isalpha(s.front()))
    {
        char ch = std::isupper(s.front()) ? std::tolower(s.front()) : s.front();
        int  add_at = 0;
        int  killed = 0;
        bool save_it = true;
        for (int i = g_user_header_type_index[ch - 'a']; g_user_header_type[i].name[0] == ch; i--)
        {
            if (len <= g_user_header_type[i].length //
                && string_case_equal(s.data(), g_user_header_type[i].name.c_str(), len))
            {
                g_user_header_type[i].name.clear();
                killed = i;
            }
            else if (len > g_user_header_type[i].length //
                     && string_case_equal(s.data(), g_user_header_type[i].name.c_str(), g_user_header_type[i].length))
            {
                if (!add_at)
                {
                    if (g_user_header_type[i].flags == (setit? flag : 0))
                    {
                        save_it = false;
                    }
                    add_at = i+1;
                }
            }
        }
        if (save_it)
        {
            if (!killed || (add_at && !g_user_header_type[add_at].name.empty()))
            {
                if (g_user_header_type_count >= g_user_header_type_max)
                {
                    g_user_header_type_max += 10;
                    g_user_header_type.resize(g_user_header_type_max);
                }
                if (!add_at)
                {
                    add_at = g_user_header_type_index[ch - 'a']+1;
                    if (add_at == 1)
                    {
                        add_at = g_user_header_type_count;
                    }
                }
                for (int i = g_user_header_type_count; i > add_at; i--)
                {
                    g_user_header_type[i] = g_user_header_type[i - 1];
                }
                g_user_header_type_count++;
            }
            else if (!add_at)
            {
                add_at = killed;
            }
            g_user_header_type[add_at].length = len;
            g_user_header_type[add_at].flags = setit? flag : 0;
            g_user_header_type[add_at].name.assign(s.data(), s.size());
            for (char &c : g_user_header_type[add_at].name)
            {
                if (std::isupper(c))
                {
                    c = static_cast<char>(std::tolower(c));
                }
            }
        }
        if (killed)
        {
            while (killed < g_user_header_type_count && !g_user_header_type[killed].name.empty())
            {
                killed++;
            }
            for (int i = killed + 1; i < g_user_header_type_count; i++)
            {
                if (!g_user_header_type[i].name.empty())
                {
                    g_user_header_type[killed++] = g_user_header_type[i];
                }
            }
            g_user_header_type_count = killed;
        }
        std::memset((char*)g_user_header_type_index,0,sizeof g_user_header_type_index);
        for (int i = 1; i < g_user_header_type_count; i++)
        {
            g_user_header_type_index[g_user_header_type[i].name[0] - 'a'] = i;
        }
    }
}

static int parse_mouse_buttons(char **cpp, const char *btns)
{
    char* t = *cpp;
    int cnt = 0;

    safe_free(t);
    btns = skip_eq(btns, ' ');
    *cpp = safe_malloc(std::strlen(btns) + 1);
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

static std::string expand_mouse_buttons(const char *cp, int cnt)
{
    std::string result;
    result.reserve(sizeof g_buf);
    while (cnt--)
    {
        if (!result.empty())
        {
            result += ' ';
        }
        if (*cp == '[')
        {
            const std::size_t len = std::strlen(cp);
            result.append(cp, len);
            result += ']';
            cp += len + 1;
            result += cp;
        }
        else
        {
            result += cp;
        }
        if (!result.empty() && result.back() == '\n')
        {
            result.pop_back();
        }
        cp += std::strlen(cp) + 1;
    }
    return result;
}

const char *quote_string(std::string_view val)
{
    static std::string buff;

    bool needs_quotes = false;
    int  ticks = 0;
    int  quotes = 0;
    int  backslashes = 0;
    buff.clear();

    if (!val.empty() && std::isspace(val.front()))
    {
        needs_quotes = true;
    }
    for (std::size_t i = 0; i < val.size(); i++)
    {
        switch (val[i])
        {
        case ' ':
        case '\t':
            if (i + 1 == val.size() || isspace(val[i + 1]))
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
        for (const char ch : val)
        {
            if (ch == usequote || ch == '\\')
            {
                buff += '\\';
            }
            buff += ch;
        }
        buff += usequote;
    }
    else
    {
        buff.assign(val.data(), val.size());
    }
    return buff.c_str();
}

void cwd_check()
{
    std::string save_dir;
    save_dir.reserve(LINE_BUF_LEN);

    if (g_priv_dir.empty())
    {
        g_priv_dir = file_exp("~/News");
    }
    save_dir = g_priv_dir;
    if (change_dir(g_priv_dir))
    {
        save_dir = file_exp(g_priv_dir);
        if (make_dir(save_dir.c_str(), MD_DIR) || change_dir(save_dir))
        {
            interp(g_cmd_buf, (sizeof g_cmd_buf), "%~/News");
            if (make_dir(g_cmd_buf, MD_DIR))
            {
                save_dir = g_home_dir;
            }
            else
            {
                save_dir = g_cmd_buf;
            }
            change_dir(save_dir);
            if (g_verbose)
            {
                std::printf("Cannot make directory %s--\n"
                            "        articles will be saved to %s\n"
                            "\n",
                            g_priv_dir.c_str(), save_dir.c_str());
            }
            else
            {
                std::printf("Can't make %s--\n"
                            "        using %s\n"
                            "\n",
                            g_priv_dir.c_str(), save_dir.c_str());
            }
        }
    }

    std::error_code error;
    save_dir = fs::current_path(error).generic_string();
    if (error)
    {
        std::printf("Cannot determine current working directory!\n");
        finalize(1);
    }
    if (eaccess(save_dir.c_str(), 2))
    {
        if (g_verbose)
        {
            std::printf("Current directory %s is not writeable--\n"
                        "        articles will be saved to home directory\n"
                        "\n",
                        save_dir.c_str());
        }
        else
        {
            std::printf("%s not writeable--using ~\n\n", save_dir.c_str());
        }
        save_dir = g_home_dir;
    }
    g_priv_dir = save_dir;
}
